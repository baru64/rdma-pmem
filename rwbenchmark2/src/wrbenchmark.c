#include <errno.h>
#include <getopt.h>
#include <libpmem.h>
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "common.h"
#include <rdma/rdma_cma.h>

struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
  union key {
    /* if we send, we call it local key */
    uint32_t local_key;
    /* if we receive, we call it remote key */
    uint32_t remote_key;
  } key;
};

struct statistics {
  uint64_t ops;
  uint64_t latency;
  uint64_t last_latency;
  uint64_t jitter;
  uint64_t elapsed_nanoseconds;
};

struct benchmark_node {
  int id;
  struct rdma_cm_id *cma_id;
  int connected;
  struct ibv_pd *pd;
  struct ibv_cq *cq[2];
  struct ibv_mr *mr;
  struct ibv_mr *src_mem_mr;
  struct ibv_mr *server_metadata_mr;
  struct statistics *stats;
  struct rdma_buffer_attr *server_metadata;
  void *src_mem;
  void *mem;
};

enum CQ_INDEX { SEND_CQ_INDEX, RECV_CQ_INDEX };

struct benchmark {
  struct rdma_event_channel *channel;
  struct benchmark_node *nodes;
  pthread_t *threads;
  int conn_index;
  int connects_left;
  int disconnects_left;

  struct rdma_addrinfo *rai;
};

static struct benchmark test;
static int connections = 1;
static int message_size = 100;
static const char *port = "7471";
static uint8_t set_tos = 0;
static uint8_t tos;
static char *dst_addr;
static char *src_addr;
static struct rdma_addrinfo hints;
static uint8_t set_timeout;
static uint8_t timeout;
static size_t metadata_size = sizeof(struct rdma_buffer_attr);
static size_t pmem_mapped_len;
int is_pmem;
atomic_bool begin = false;
atomic_bool stop = false;
bool use_pmem = false;
struct timespec sleep_time;
struct timespec prepare_time;
struct statistics total_stats;
char pmem_file_path[128] = {0};
bool csv_output = false;
bool debug_log = true;
void *pmem;

uint64_t get_time_ns() {
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  return (uint64_t)spec.tv_sec * (1000 * 1000 * 1000) + (uint64_t)spec.tv_nsec;
}

static void print_stats(struct statistics *stats) {
  if (csv_output) {
    printf("%lu;%lu;%lu;%f\n", stats->ops, stats->latency / stats->ops,
           stats->jitter / (stats->ops - 1),
           (double)stats->ops * message_size / (1024 * 1024 * 1024) *
               1000000000 / stats->elapsed_nanoseconds);
  } else {
    puts("ops | avg lat [ns] | avg jitter [ns] | throughput [GB/s]");
    printf("%lu %lu %lu %f\n", stats->ops, stats->latency / stats->ops,
           stats->jitter / (stats->ops - 1),
           (double)stats->ops * message_size / (1024 * 1024 * 1024) *
               1000000000 / stats->elapsed_nanoseconds);
  }
}

static void node_print_stats(struct benchmark_node *node) {
  puts("th | ops | time [ns] | avg lat [ns] | avg jitter [ns] | throughput "
       "[GB/s]");
  printf("%d %lu %lu %lu %lu %f\n", node->id, node->stats->ops,
         node->stats->elapsed_nanoseconds,
         node->stats->latency / node->stats->ops,
         node->stats->jitter / (node->stats->ops - 1),
         (double)node->stats->ops * message_size / (1024 * 1024 * 1024) *
             1000000000 / node->stats->elapsed_nanoseconds);
}

static void print_metadata(struct benchmark_node *node) {
  if (debug_log)
    printf("Server addr:len:key for node %d > %lu:%u:%u\n", node->id,
           node->server_metadata->address, node->server_metadata->length,
           node->server_metadata->key.local_key);
}

static void print_mem(struct benchmark_node *node) {
  puts("print mem");
  printf("MEM node %d > %.99s\n", node->id, (char *)node->mem);
}

static int create_message(struct benchmark_node *node) {
  // buffer for rdma operations
  if (use_pmem) {
    node->mem = pmem + message_size*node->id;
    if (node->mem == NULL) {
      printf("failed pmem allocation\n");
      return -1;
    }
    if (!is_pmem) {
      printf("error: not pmem\n");
      return -1;
    }
  } else {
    node->mem = malloc(message_size);
    if (!node->mem) {
      printf("failed message allocation\n");
      return -1;
    }
  }

  node->mr = ibv_reg_mr(node->pd, node->mem, message_size,
                        (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE));
  if (!node->mr) {
    printf("failed to reg MR errno %d\n", errno);
    goto err;
  }

  // source buffer
  node->src_mem = malloc(message_size);
  if (!node->src_mem) {
    printf("failed src_mem allocation\n");
    return -1;
  }
  node->src_mem_mr = ibv_reg_mr(node->pd, node->src_mem, message_size,
                                (IBV_ACCESS_LOCAL_WRITE));
  if (!node->src_mem_mr) {
    printf("failed to reg MR\n");
    goto err;
  }
  // temporary
  // sprintf(node->src_mem, "%d-testSTRING12345", node->id);

  return 0;
err:
  if (!use_pmem)
    free(node->mem);
  return -1;
}

static void server_set_metadata(struct benchmark_node *node) {
  node->server_metadata->address = (uint64_t)node->mr->addr;
  node->server_metadata->length = node->mr->length;
  node->server_metadata->key.local_key = node->mr->rkey;
  print_metadata(node);
}

static int create_metadata(struct benchmark_node *node) {
  node->server_metadata = calloc(metadata_size, 1);
  if (!node->server_metadata) {
    printf("failed server_metadata allocation\n");
    return -1;
  }
  node->server_metadata_mr = ibv_reg_mr(node->pd, node->server_metadata,
                                        metadata_size, IBV_ACCESS_LOCAL_WRITE);
  if (!node->server_metadata_mr) {
    printf("failed to reg server_metadata_mr\n");
    goto err;
  }

  return 0;
err:
  free(node->server_metadata);
  return -1;
}

static int init_node(struct benchmark_node *node) {
  struct ibv_qp_init_attr init_qp_attr;
  int cqe, ret;

  node->stats = calloc(sizeof(struct statistics), 1);
  if (!node->stats) {
    ret = -ENOMEM;
    printf("rwbenchmark: unable to allocate statistics errno: %d", errno);
    goto out;
  }

  node->pd = ibv_alloc_pd(node->cma_id->verbs);
  if (!node->pd) {
    ret = -ENOMEM;
    printf("rwbenchmark: unable to allocate PD\n");
    goto out;
  }

  cqe = 1;
  node->cq[SEND_CQ_INDEX] =
      ibv_create_cq(node->cma_id->verbs, cqe, node, NULL, 0);
  node->cq[RECV_CQ_INDEX] =
      ibv_create_cq(node->cma_id->verbs, cqe, node, NULL, 0);
  if (!node->cq[SEND_CQ_INDEX] || !node->cq[RECV_CQ_INDEX]) {
    ret = -ENOMEM;
    printf("rwbenchmark: unable to create CQ\n");
    goto out;
  }

  memset(&init_qp_attr, 0, sizeof init_qp_attr);
  init_qp_attr.cap.max_send_wr = cqe;
  init_qp_attr.cap.max_recv_wr = cqe;
  init_qp_attr.cap.max_send_sge = 1;
  init_qp_attr.cap.max_recv_sge = 1;
  init_qp_attr.qp_context = node;
  init_qp_attr.sq_sig_all = 1;
  init_qp_attr.qp_type = IBV_QPT_RC;
  init_qp_attr.send_cq = node->cq[SEND_CQ_INDEX];
  init_qp_attr.recv_cq = node->cq[RECV_CQ_INDEX];
  ret = rdma_create_qp(node->cma_id, node->pd, &init_qp_attr);
  if (ret) {
    perror("rwbenchmark: unable to create QP");
    goto out;
  }

  // allocate metadata buffer and mr
  ret = create_metadata(node);
  if (ret) {
    printf("rwbenchmark: failed to create metadata buffer: %d\n", ret);
    goto out;
  }

  print_metadata(node);

  // allocate buffer and create message MR
  ret = create_message(node);
  if (ret) {
    printf("rwbenchmark: failed to create messages: %d\n", ret);
    goto out;
  }
out:
  return ret;
}

static int post_recv_metadata(struct benchmark_node *node) {
  struct ibv_recv_wr recv_wr, *recv_failure;
  struct ibv_sge sge;
  int ret = 0;

  recv_wr.next = NULL;
  recv_wr.sg_list = &sge;
  recv_wr.num_sge = 1;
  recv_wr.wr_id = (uintptr_t)node;

  sge.length = metadata_size;
  sge.lkey = node->server_metadata_mr->lkey;
  sge.addr = (uintptr_t)node->server_metadata;

  ret = ibv_post_recv(node->cma_id->qp, &recv_wr, &recv_failure);
  if (ret) {
    printf("failed to post receive metadata: %d\n", ret);
  }

  return ret;
}

static int post_send_metadata(struct benchmark_node *node) {
  struct ibv_send_wr send_wr, *bad_send_wr;
  struct ibv_sge sge;
  int i, ret = 0;

  if (!node->connected)
    return 0;

  send_wr.next = NULL;
  send_wr.sg_list = &sge;
  send_wr.num_sge = 1;
  send_wr.opcode = IBV_WR_SEND;
  send_wr.send_flags = 0;
  send_wr.wr_id = (unsigned long)node;

  sge.length = metadata_size;
  sge.lkey = node->server_metadata_mr->lkey;
  sge.addr = (uintptr_t)node->server_metadata;

  ret = ibv_post_send(node->cma_id->qp, &send_wr, &bad_send_wr);
  if (ret)
    printf("failed to post send metadata: %d\n", ret);
  return ret;
}

static int post_send_write(struct benchmark_node *node) {
  struct ibv_send_wr send_wr, *bad_send_wr;
  struct ibv_sge sge;
  int i, ret = 0;

  if (!node->connected)
    return 0;

  send_wr.next = NULL;
  send_wr.sg_list = &sge;
  send_wr.num_sge = 1;
  send_wr.opcode = IBV_WR_RDMA_WRITE;
  send_wr.send_flags = 0;
  send_wr.wr_id = (unsigned long)node;

  // source
  sge.length = message_size;
  sge.lkey = node->src_mem_mr->lkey;
  sge.addr = (uintptr_t)node->src_mem_mr->addr;

  // remote write destination
  send_wr.wr.rdma.rkey = node->server_metadata->key.remote_key;
  send_wr.wr.rdma.remote_addr = node->server_metadata->address;

  ret = ibv_post_send(node->cma_id->qp, &send_wr, &bad_send_wr);
  if (ret)
    printf("failed to post send metadata: %d\n", ret);
  return ret;
}

static int post_send_read(struct benchmark_node *node) {
  struct ibv_send_wr send_wr, *bad_send_wr;
  struct ibv_sge sge;
  int i, ret = 0;

  if (!node->connected)
    return 0;

  send_wr.next = NULL;
  send_wr.sg_list = &sge;
  send_wr.num_sge = 1;
  send_wr.opcode = IBV_WR_RDMA_READ;
  send_wr.send_flags = 0;
  send_wr.wr_id = (unsigned long)node;

  // destination
  sge.length = message_size;
  sge.lkey = node->mr->lkey;
  sge.addr = (uintptr_t)node->mem;

  // remote read source
  send_wr.wr.rdma.rkey = node->server_metadata->key.remote_key;
  send_wr.wr.rdma.remote_addr = node->server_metadata->address;

  ret = ibv_post_send(node->cma_id->qp, &send_wr, &bad_send_wr);
  if (ret)
    printf("failed to post send metadata: %d\n", ret);
  return ret;
}

static void connect_error(void) { test.connects_left--; }

static int addr_handler(struct benchmark_node *node) {
  int ret;

  if (set_tos) {
    ret = rdma_set_option(node->cma_id, RDMA_OPTION_ID, RDMA_OPTION_ID_TOS,
                          &tos, sizeof tos);
    if (ret)
      perror("rwbenchmark: set TOS option failed");
  }
  ret = rdma_resolve_route(node->cma_id, 2000);
  if (ret) {
    perror("rwbenchmark: resolve route failed");
    connect_error();
  }
  return ret;
}

// client event
static int route_handler(struct benchmark_node *node) {
  struct rdma_conn_param conn_param;
  int ret;

  ret = init_node(node);
  if (ret)
    goto err;

  ret = post_recv_metadata(node);
  if (ret)
    goto err;

  memset(&conn_param, 0, sizeof conn_param);
  conn_param.responder_resources = 1;
  conn_param.initiator_depth = 1;
  conn_param.retry_count = 5;
  conn_param.private_data = test.rai->ai_connect;
  conn_param.private_data_len = test.rai->ai_connect_len;
  ret = rdma_connect(node->cma_id, &conn_param);
  if (ret) {
    perror("rwbenchmark: failure connecting");
    goto err;
  }
  return 0;
err:
  connect_error();
  return ret;
}

// server event
static int connect_handler(struct rdma_cm_id *cma_id) {
  struct benchmark_node *node;
  int ret;

  if (test.conn_index == connections) {
    ret = -ENOMEM;
    goto err1;
  }
  node = &test.nodes[test.conn_index++];

  node->cma_id = cma_id;
  cma_id->context = node;

  ret = init_node(node);
  if (ret)
    goto err2;

  ret = rdma_accept(node->cma_id, NULL);
  if (ret) {
    perror("rwbenchmark: failure accepting");
    goto err2;
  }
  return 0;

err2:
  node->cma_id = NULL;
  connect_error();
err1:
  printf("rwbenchmark: failing connection request\n");
  rdma_reject(cma_id, NULL, 0);
  return ret;
}

static int cma_handler(struct rdma_cm_id *cma_id, struct rdma_cm_event *event) {
  int ret = 0;

  switch (event->event) {
  case RDMA_CM_EVENT_ADDR_RESOLVED:
    ret = addr_handler(cma_id->context);
    break;
  case RDMA_CM_EVENT_ROUTE_RESOLVED:
    ret = route_handler(cma_id->context);
    break;
  case RDMA_CM_EVENT_CONNECT_REQUEST:
    ret = connect_handler(cma_id);
    break;
  case RDMA_CM_EVENT_ESTABLISHED:
    ((struct benchmark_node *)cma_id->context)->connected = 1;
    test.connects_left--;
    test.disconnects_left++;
    break;
  case RDMA_CM_EVENT_ADDR_ERROR:
  case RDMA_CM_EVENT_ROUTE_ERROR:
  case RDMA_CM_EVENT_CONNECT_ERROR:
  case RDMA_CM_EVENT_UNREACHABLE:
  case RDMA_CM_EVENT_REJECTED:
    printf("rwbenchmark: event: %s, error: %d\n", rdma_event_str(event->event),
           event->status);
    connect_error();
    ret = event->status;
    break;
  case RDMA_CM_EVENT_DISCONNECTED:
    rdma_disconnect(cma_id);
    test.disconnects_left--;
    break;
  case RDMA_CM_EVENT_DEVICE_REMOVAL:
    /* Cleanup will occur after test completes. */
    break;
  default:
    break;
  }
  return ret;
}

static void destroy_node(struct benchmark_node *node) {
  if (!node->cma_id)
    return;

  if (node->cma_id->qp)
    rdma_destroy_qp(node->cma_id);

  if (node->cq[SEND_CQ_INDEX])
    ibv_destroy_cq(node->cq[SEND_CQ_INDEX]);

  if (node->cq[RECV_CQ_INDEX])
    ibv_destroy_cq(node->cq[RECV_CQ_INDEX]);

  if (node->mem) {
    ibv_dereg_mr(node->mr);
    if (!use_pmem)
      free(node->mem);
  }

  if (node->src_mem) {
    ibv_dereg_mr(node->src_mem_mr);
    free(node->src_mem);
  }

  if (node->server_metadata) {
    ibv_dereg_mr(node->server_metadata_mr);
    free(node->server_metadata);
  }

  if (node->stats) {
    free(node->stats);
  }

  if (node->pd)
    ibv_dealloc_pd(node->pd);

  /* Destroy the RDMA ID after all device resources */
  rdma_destroy_id(node->cma_id);
}

static int alloc_nodes(void) {
  int ret, i;

  test.nodes = malloc(sizeof *test.nodes * connections);
  if (!test.nodes) {
    printf("rwbenchmark: unable to allocate memory for test nodes\n");
    return -ENOMEM;
  }
  memset(test.nodes, 0, sizeof *test.nodes * connections);

  test.threads = malloc(sizeof *test.threads * connections);
  if (!test.threads) {
    printf("rwbenchmark: unable to allocate memory for threads\n");
    return -ENOMEM;
  }
  memset(test.threads, 0, sizeof *test.threads * connections);

  for (i = 0; i < connections; i++) {
    test.nodes[i].id = i;
    if (dst_addr) {
      ret = rdma_create_id(test.channel, &test.nodes[i].cma_id, &test.nodes[i],
                           hints.ai_port_space);
      if (ret)
        goto err;
    }
  }
  if (use_pmem) {
    pmem = pmem_map_file(pmem_file_path, 0 /* len */, 0 /* flags */, 0 /* mode */,
                         &pmem_mapped_len, &is_pmem);
    if (!pmem) {
      printf("rwbenchmark: unable to allocate persistent memory %d\n", errno);
      goto err;
    }
    if (pmem_mapped_len < (message_size*connections)) {
      printf("rwbenchmark: not enough persistent memory %d\n", errno);
      goto err;
    }
  }
  return 0;
err:
  while (--i >= 0)
    rdma_destroy_id(test.nodes[i].cma_id);
  free(test.nodes);
  return ret;
}

static void destroy_nodes(void) {
  int i;

  for (i = 0; i < connections; i++)
    destroy_node(&test.nodes[i]);
  free(test.nodes);
}

static int poll_one_wc(enum CQ_INDEX index) {
  struct ibv_wc wc[8];
  int done, i, ret;

  for (i = 0; i < connections; i++) {
    if (!test.nodes[i].connected)
      continue;

    for (done = 0; done < 1; done += ret) {
      ret = ibv_poll_cq(test.nodes[i].cq[index], 1, wc);
      if (ret < 0) {
        printf("rwbenchmark: failed polling CQ: %d\n", ret);
        return ret;
      }
      if (ret > 0 && debug_log)
        printf("rwbenchmark: received work completion wr_id: %lu len: %u s: %s "
               "f: %u\n",
               wc->wr_id, wc->byte_len, ibv_wc_status_str(wc->status),
               wc->wc_flags);
    }
  }
  return 0;
}

static int node_poll_n_cq(struct benchmark_node *node, enum CQ_INDEX index,
                          int n) {
  struct ibv_wc wc[8];
  int done, i, ret;

  if (!node->connected)
    return 0;

  for (done = 0; done < n; done += ret) {
    ret = ibv_poll_cq(node->cq[index], 8, wc);
    if (ret < 0) {
      printf("rwbenchmark: failed polling CQ: %d\n", ret);
      return ret;
    }
  }
  return 0;
}

static int connect_events(void) {
  struct rdma_cm_event *event;
  int ret = 0;

  while (test.connects_left && !ret) {
    ret = rdma_get_cm_event(test.channel, &event);
    if (!ret) {
      ret = cma_handler(event->id, event);
      rdma_ack_cm_event(event);
    } else {
      perror("rwbenchmark: failure in rdma_get_cm_event in connect events");
      ret = errno;
    }
  }

  return ret;
}

static int disconnect_events(void) {
  struct rdma_cm_event *event;
  int ret = 0;

  while (test.disconnects_left && !ret) {
    ret = rdma_get_cm_event(test.channel, &event);
    if (!ret) {
      ret = cma_handler(event->id, event);
      rdma_ack_cm_event(event);
    } else {
      perror("rwbenchmark: failure in rdma_get_cm_event in disconnect events");
      ret = errno;
    }
  }

  return ret;
}

static int run_server(void) {
  struct rdma_cm_id *listen_id;
  int i, ret;

  printf("rwbenchmark: starting server\n");
  ret = rdma_create_id(test.channel, &listen_id, &test, hints.ai_port_space);
  if (ret) {
    perror("rwbenchmark: listen request failed");
    return ret;
  }

  ret = get_rdma_addr(src_addr, dst_addr, port, &hints, &test.rai);
  if (ret) {
    printf("rwbenchmark: getrdmaaddr error: %s\n", gai_strerror(ret));
    goto out;
  }

  ret = rdma_bind_addr(listen_id, test.rai->ai_src_addr);
  if (ret) {
    perror("rwbenchmark: bind address failed");
    goto out;
  }

  ret = rdma_listen(listen_id, 8);
  if (ret) {
    perror("rwbenchmark: failure trying to listen");
    goto out;
  }

  ret = connect_events();
  if (ret)
    goto out;

  printf("exchanging metadata\n");
  for (i = 0; i < connections; i++) {
    server_set_metadata(&test.nodes[i]);
    ret = post_send_metadata(&test.nodes[i]);
    if (ret)
      goto out;
  }

  printf("completing sends\n");
  ret = poll_one_wc(SEND_CQ_INDEX);
  if (ret)
    goto out;

  printf("metadata sent\n");

  ret = disconnect_events();

  printf("disconnected\n");

out:
  rdma_destroy_id(listen_id);
  return ret;
}

void *worker(void *index) {
  int ret;
  uint64_t start, end, current_latency;
  struct benchmark_node *node = &test.nodes[*(int *)index];

  while (!begin) { /* wait */
  }
  node->stats->elapsed_nanoseconds = get_time_ns();

  while (!stop) {
    start = get_time_ns();
    // RDMA WRITE
    ret = post_send_write(node);
    if (ret) {
      printf("rwbenchmark: worker post_send_write error %d\n", ret);
      return NULL;
    }
    // wait for completion
    ret = node_poll_n_cq(node, SEND_CQ_INDEX, 1);
    if (ret) {
      printf("rwbenchmark: worker node_poll_n_cq error %d\n", ret);
      return NULL;
    }
    // RDMA READ
    ret = post_send_read(node);
    if (ret) {
      printf("rwbenchmark: worker post_send_read error %d\n", ret);
      return NULL;
    }
    // wait for completion
    ret = node_poll_n_cq(node, SEND_CQ_INDEX, 1);
    if (ret) {
      printf("rwbenchmark: worker node_poll_n_cq error %d\n", ret);
      return NULL;
    }
    end = get_time_ns();

    node->stats->ops++;
    current_latency = end - start;
    node->stats->latency += current_latency;
    if (node->stats->last_latency != 0)
      node->stats->jitter +=
          labs((long)node->stats->last_latency - (long)current_latency);
    node->stats->last_latency = end - start;
  }
  node->stats->elapsed_nanoseconds =
      get_time_ns() - node->stats->elapsed_nanoseconds;
  if (debug_log) node_print_stats(node);
  return NULL;
}

static int run_client(void) {
  int i, ret, ret2;

  if (debug_log) printf("rwbenchmark: starting client\n");

  ret = get_rdma_addr(src_addr, dst_addr, port, &hints, &test.rai);
  if (ret) {
    printf("rwbenchmark: getaddrinfo error: %s\n", gai_strerror(ret));
    return ret;
  }

  if (debug_log) printf("rwbenchmark: connecting\n");
  for (i = 0; i < connections; i++) {
    ret = rdma_resolve_addr(test.nodes[i].cma_id, test.rai->ai_src_addr,
                            test.rai->ai_dst_addr, 2000);
    if (ret) {
      perror("rwbenchmark: failure getting addr");
      connect_error();
      return ret;
    }
  }

  ret = connect_events();
  if (ret)
    goto disc;

  if (debug_log) printf("receiving metadata\n");
  ret = poll_one_wc(RECV_CQ_INDEX);
  if (ret)
    goto disc;

  if (debug_log) for (i = 0; i < connections; i++)
    print_metadata(&test.nodes[i]);

  if (debug_log) printf("metadata received\n");
  // run workers
  for (i = 0; i < connections; i++) {
    pthread_create(&test.threads[i], NULL, worker, (void *)&test.nodes[i].id);
  }
  nanosleep(&prepare_time, NULL);
  begin = true;
  nanosleep(&sleep_time, NULL); // benchmark work
  stop = true;
  // join workers
  for (i = 0; i < connections; i++) {
    pthread_join(test.threads[i], NULL);
  }
  // total statistics
  memset(&total_stats, 0, sizeof(struct statistics));
  for (i = 0; i < connections; i++) {
    total_stats.latency += test.nodes[i].stats->latency;
    total_stats.ops += test.nodes[i].stats->ops;
    total_stats.jitter += test.nodes[i].stats->jitter;
    total_stats.elapsed_nanoseconds +=
        test.nodes[i].stats->elapsed_nanoseconds;
  }
  // avg time
  total_stats.elapsed_nanoseconds = total_stats.elapsed_nanoseconds / connections;
  print_stats(&total_stats);

  ret = 0;
disc:

  for (i = 0; i < connections; i++) {
    rdma_disconnect(test.nodes[i].cma_id);
  }
  ret2 = disconnect_events();
  if (ret2)
    ret = ret2;
out:
  return ret;
}

int main(int argc, char **argv) {
  int op, ret, option_index;

  sleep_time.tv_sec = 1;
  sleep_time.tv_nsec = 0;
  prepare_time.tv_sec = 1;
  prepare_time.tv_nsec = 0;

  hints.ai_port_space = RDMA_PS_TCP;

  static struct option long_options[] = {{"pmem", required_argument, NULL, 0}};
  while ((op = getopt_long(argc, argv, "s:b:f:P:c:S:t:p:a:v0", long_options,
                           &option_index)) != -1) {
    switch (op) {
    case 's':
      dst_addr = optarg;
      break;
    case 'b':
      src_addr = optarg;
      break;
    case 'f':
      if (!strncasecmp("ip", optarg, 2)) {
        hints.ai_flags = RAI_NUMERICHOST;
      } else if (!strncasecmp("gid", optarg, 3)) {
        hints.ai_flags = RAI_NUMERICHOST | RAI_FAMILY;
        hints.ai_family = AF_IB;
      } else if (strncasecmp("name", optarg, 4)) {
        fprintf(stderr, "Warning: unknown address format\n");
      }
      break;
    case 'P':
      if (!strncasecmp("ib", optarg, 2)) {
        hints.ai_port_space = RDMA_PS_IB;
      } else if (strncasecmp("tcp", optarg, 3)) {
        fprintf(stderr, "Warning: unknown port space format\n");
      }
      break;
    case 'c':
      connections = atoi(optarg);
      break;
    case 'S':
      message_size = atoi(optarg);
      break;
    case 't':
      sleep_time.tv_sec = atoi(optarg);
      break;
    case 'p':
      port = optarg;
      break;
    case 'a':
      set_timeout = 1;
      timeout = (uint8_t)strtoul(optarg, NULL, 0);
      break;
    case 'v':
      csv_output = true;
      debug_log = false;
      break;
    case 0:
      strcpy(pmem_file_path, optarg);
      use_pmem = true;
      break;
    default:
      printf("usage: %s\n", argv[0]);
      printf("\t[-s server_address]\n");
      printf("\t[-b bind_address]\n");
      printf("\t[-f address_format]\n");
      printf("\t    name, ip, ipv6, or gid\n");
      printf("\t[-P port_space]\n");
      printf("\t    tcp or ib\n");
      printf("\t[-c connections]\n");
      printf("\t[-S message_size]\n");
      printf("\t[-t benchmark_time]\n");
      printf("\t[-p port_number]\n");
      printf("\t[-a ack_timeout]\n");
      printf("\t[-v] enable csv ouput\n");
      printf("\t[--pmem pmem_file_path]\n");
      exit(1);
    }
  }

  test.connects_left = connections;

  test.channel = create_first_event_channel();
  if (!test.channel) {
    exit(1);
  }

  if (alloc_nodes())
    exit(1);

  if (dst_addr) {
    ret = run_client();
  } else {
    hints.ai_flags |= RAI_PASSIVE;
    ret = run_server();
  }

  if (debug_log) printf("test complete\n");
  destroy_nodes();
  rdma_destroy_event_channel(test.channel);
  if (test.rai)
    rdma_freeaddrinfo(test.rai);

  if (debug_log) printf("return status %d\n", ret);
  return ret;
}
