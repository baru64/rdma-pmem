#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <time.h>

#include <rdma/rdma_cma.h>
#include "common.h"

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
	int						id;
	struct rdma_cm_id		*cma_id;
	int						connected;
	struct ibv_pd			*pd;
	struct ibv_cq			*cq[2];
	struct ibv_mr			*mr;
	struct ibv_mr			*server_metadata_mr;
	struct statistics		*stats;
	struct rdma_buffer_attr *server_metadata;
	void					*mem;
};

enum CQ_INDEX {
	SEND_CQ_INDEX,
	RECV_CQ_INDEX
};

struct benchmark {
	struct rdma_event_channel *channel;
	struct benchmark_node	*nodes;
	int			conn_index;
	int			connects_left;
	int			disconnects_left;

	struct rdma_addrinfo	*rai;
};

static struct benchmark test;
static int connections = 1;
static int message_size = 100;
static int message_count = 1;
static const char *port = "7471";
static uint8_t set_tos = 0;
static uint8_t tos;
static char *dst_addr;
static char *src_addr;
static struct rdma_addrinfo hints;
static uint8_t set_timeout;
static uint8_t timeout;
size_t metadata_size = sizeof(struct rdma_buffer_attr);

uint64_t get_time_ns() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return (uint64_t) spec.tv_sec * (1000 * 1000 * 1000) + (uint64_t) spec.tv_nsec;
}

static void print_metadata(struct benchmark_node* node) {
	printf("Server addr:len:key for node %d > %lu:%u:%u\n", node->id,
		node->server_metadata->address,
		node->server_metadata->length,
		node->server_metadata->key.local_key);
}

static int create_message(struct benchmark_node *node)
{
	if (!message_size)
		message_count = 0;

	if (!message_count)
		return 0;

	node->mem = malloc(message_size);
	if (!node->mem) {
		printf("failed message allocation\n");
		return -1;
	}
	node->mr = ibv_reg_mr(node->pd, node->mem, message_size,
			     (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
	if (!node->mr) {
		printf("failed to reg MR\n");
		goto err;
	}

	return 0;
err:
	free(node->mem);
	return -1;
}

static void server_set_metadata(struct benchmark_node* node)
{
	node->server_metadata->address = (uint64_t) node->mr->addr;
	node->server_metadata->length = node->mr->length;
	node->server_metadata->key.local_key = node->mr->lkey;
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

static int init_node(struct benchmark_node *node)
{
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

	cqe = message_count ? message_count : 1;
	node->cq[SEND_CQ_INDEX] = ibv_create_cq(node->cma_id->verbs, cqe, node, NULL, 0);
	node->cq[RECV_CQ_INDEX] = ibv_create_cq(node->cma_id->verbs, cqe, node, NULL, 0);
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
	free(node->stats);
	return ret;
}

static int post_recvs(struct benchmark_node *node)
{
	struct ibv_recv_wr recv_wr, *recv_failure;
	struct ibv_sge sge;
	int i, ret = 0;

	if (!message_count)
		return 0;

	recv_wr.next = NULL;
	recv_wr.sg_list = &sge;
	recv_wr.num_sge = 1;
	recv_wr.wr_id = (uintptr_t) node;

	sge.length = message_size;
	sge.lkey = node->mr->lkey;
	sge.addr = (uintptr_t) node->mem;

	for (i = 0; i < message_count && !ret; i++ ) {
		ret = ibv_post_recv(node->cma_id->qp, &recv_wr, &recv_failure);
		if (ret) {
			printf("failed to post receives: %d\n", ret);
			break;
		}
	}
	return ret;
}

static int post_recv_metadata(struct benchmark_node *node) {
	struct ibv_recv_wr recv_wr, *recv_failure;
	struct ibv_sge sge;
	int ret = 0;

	recv_wr.next = NULL;
	recv_wr.sg_list = &sge;
	recv_wr.num_sge = 1;
	recv_wr.wr_id = (uintptr_t) node;

	sge.length = metadata_size;
	sge.lkey = node->server_metadata_mr->lkey;
	sge.addr = (uintptr_t) node->server_metadata;

	ret = ibv_post_recv(node->cma_id->qp, &recv_wr, &recv_failure);
	if (ret) {
		printf("failed to post receive metadata: %d\n", ret);
	}
	
	return ret;
}

static int post_sends(struct benchmark_node *node)
{
	struct ibv_send_wr send_wr, *bad_send_wr;
	struct ibv_sge sge;
	int i, ret = 0;

	if (!node->connected || !message_count)
		return 0;

	send_wr.next = NULL;
	send_wr.sg_list = &sge;
	send_wr.num_sge = 1;
	send_wr.opcode = IBV_WR_SEND;
	send_wr.send_flags = 0;
	send_wr.wr_id = (unsigned long)node;

	sge.length = message_size;
	sge.lkey = node->mr->lkey;
	sge.addr = (uintptr_t) node->mem;

	for (i = 0; i < message_count && !ret; i++) {
		ret = ibv_post_send(node->cma_id->qp, &send_wr, &bad_send_wr);
		if (ret) 
			printf("failed to post sends: %d\n", ret);
	}
	return ret;
}

static int post_send_metadata(struct benchmark_node *node)
{
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
	sge.addr = (uintptr_t) node->server_metadata;

	ret = ibv_post_send(node->cma_id->qp, &send_wr, &bad_send_wr);
	if (ret) 
		printf("failed to post send metadata: %d\n", ret);
	return ret;
}

static void connect_error(void)
{
	test.connects_left--;
}

static int addr_handler(struct benchmark_node *node)
{
	int ret;

	if (set_tos) {
		ret = rdma_set_option(node->cma_id, RDMA_OPTION_ID,
				      RDMA_OPTION_ID_TOS, &tos, sizeof tos);
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
static int route_handler(struct benchmark_node *node)
{
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
static int connect_handler(struct rdma_cm_id *cma_id)
{
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

	// todo remove this
	// ret = post_recvs(node);
	// if (ret)
	// 	goto err2;

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

static int cma_handler(struct rdma_cm_id *cma_id, struct rdma_cm_event *event)
{
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
		((struct benchmark_node *) cma_id->context)->connected = 1;
		test.connects_left--;
		test.disconnects_left++;
		break;
	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		printf("rwbenchmark: event: %s, error: %d\n",
		       rdma_event_str(event->event), event->status);
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

static void destroy_node(struct benchmark_node *node)
{
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
		free(node->mem);
	}

	if (node->pd)
		ibv_dealloc_pd(node->pd);

	/* Destroy the RDMA ID after all device resources */
	rdma_destroy_id(node->cma_id);
}

static int alloc_nodes(void)
{
	int ret, i;

	test.nodes = malloc(sizeof *test.nodes * connections);
	if (!test.nodes) {
		printf("rwbenchmark: unable to allocate memory for test nodes\n");
		return -ENOMEM;
	}
	memset(test.nodes, 0, sizeof *test.nodes * connections);

	for (i = 0; i < connections; i++) {
		test.nodes[i].id = i;
		if (dst_addr) {
			ret = rdma_create_id(test.channel,
					     &test.nodes[i].cma_id,
					     &test.nodes[i], hints.ai_port_space);
			if (ret)
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

static void destroy_nodes(void)
{
	int i;

	for (i = 0; i < connections; i++)
		destroy_node(&test.nodes[i]);
	free(test.nodes);
}

// TODO poll only sent metadata wc
static int poll_cqs(enum CQ_INDEX index)
{
	struct ibv_wc wc[8];
	int done, i, ret;

	for (i = 0; i < connections; i++) {
		if (!test.nodes[i].connected)
			continue;

		for (done = 0; done < message_count; done += ret) {
			ret = ibv_poll_cq(test.nodes[i].cq[index], 8, wc);
			if (ret < 0) {
				printf("rwbenchmark: failed polling CQ: %d\n", ret);
				return ret;
			}
			printf("rwbenchmark: received %d work completions\n", ret);
		}
	}
	return 0;
}

static int poll_metadata_wc(enum CQ_INDEX index)
{
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
			printf("rwbenchmark: received work completion wr_id: %lu len: %u s: %d f: %u\n",
					wc->wr_id, wc->byte_len, wc->status, wc->wc_flags);
		}
	}
	return 0;
}

static int connect_events(void)
{
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

static int disconnect_events(void)
{
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

static int run_server(void)
{
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

	ret = rdma_listen(listen_id, 0);
	if (ret) {
		perror("rwbenchmark: failure trying to listen");
		goto out;
	}

	ret = connect_events();
	if (ret)
		goto out;

	if (message_count) {
		printf("exchanging metadata\n");
		for (i = 0; i < connections; i++) {
			server_set_metadata(&test.nodes[i]);
			ret = post_send_metadata(&test.nodes[i]);
			if (ret)
				goto out;
		}

		printf("completing sends\n");
		ret = poll_cqs(SEND_CQ_INDEX);
		if (ret)
			goto out;
		
		printf("metadata sent\n");

		// printf("receiving data transfers\n");
		// ret = poll_cqs(RECV_CQ_INDEX);
		// if (ret)
		// 	goto out;
		// printf("data transfers complete\n");

	}

	// printf("rwbenchmark: disconnecting\n");
	// for (i = 0; i < connections; i++) {
	// 	if (!test.nodes[i].connected)
	// 		continue;

	// 	test.nodes[i].connected = 0;
	// 	rdma_disconnect(test.nodes[i].cma_id);
	// }

	ret = disconnect_events();

 	printf("disconnected\n");

out:
	rdma_destroy_id(listen_id);
	return ret;
}

static int run_client(void)
{
	int i, ret, ret2;

	printf("rwbenchmark: starting client\n");

	ret = get_rdma_addr(src_addr, dst_addr, port, &hints, &test.rai);
	if (ret) {
		printf("rwbenchmark: getaddrinfo error: %s\n", gai_strerror(ret));
		return ret;
	}

	printf("rwbenchmark: connecting\n");
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

    // zamiast tego odpalamy thready
    // każdy thread odbiera najpierw dane MR, (albo nic nie robi przy send) 
    // następnie robi w pętli:
    // - post, poll
    // - X razy post, poll wszystkiego
    // na końcu wypisuje podsumowanie
	if (message_count) {
		printf("receiving metadata\n");
		ret = poll_metadata_wc(RECV_CQ_INDEX);
		if (ret)
			goto disc;
		
		for (i = 0; i < connections; i++) print_metadata(&test.nodes[i]);

		printf("metadata received\n");
		// TODO benchmark
		// printf("sending replies\n");
		// for (i = 0; i < connections; i++) {
		// 	ret = post_sends(&test.nodes[i]);
		// 	if (ret)
		// 		goto disc;
		// }

		// printf("completing sends\n");
		// ret = poll_cqs(SEND_CQ_INDEX);

		// printf("data transfers complete\n");
	}

	ret = 0;
disc:
	ret2 = disconnect_events();
	if (ret2)
		ret = ret2;
out:
	return ret;
}

void worker()
{
    // Todo
}

int main(int argc, char **argv)
{
	int op, ret;

	hints.ai_port_space = RDMA_PS_TCP;
	while ((op = getopt(argc, argv, "s:b:f:P:c:C:S:t:p:a:m")) != -1) {
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
		case 'C':
			message_count = atoi(optarg);
			break;
		case 'S':
			message_size = atoi(optarg);
			break;
		case 't':
			set_tos = 1;
			tos = (uint8_t) strtoul(optarg, NULL, 0);
			break;
		case 'p':
			port = optarg;
			break;
		case 'a':
			set_timeout = 1;
			timeout = (uint8_t) strtoul(optarg, NULL, 0);
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
			printf("\t[-C message_count]\n");
			printf("\t[-S message_size]\n");
			printf("\t[-t type_of_service]\n");
			printf("\t[-p port_number]\n");
			printf("\t[-m(igrate)]\n");
			printf("\t[-a ack_timeout]\n");
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

	printf("test complete\n");
	destroy_nodes();
	rdma_destroy_event_channel(test.channel);
	if (test.rai)
		rdma_freeaddrinfo(test.rai);

	printf("return status %d\n", ret);
	return ret;
}
