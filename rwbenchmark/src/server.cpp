#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstring>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

/* Capacity of the completion queue (CQ) */
#define CQ_CAPACITY (16)
/* MAX SGE capacity */
#define MAX_SGE (2)
/* MAX work requests */
#define MAX_WR (8)
/* Default port where the RDMA server is listening */
#define DEFAULT_RDMA_PORT (20886)

struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
  union stag {
    /* if we send, we call it local stags */
    uint32_t local_stag;
    /* if we receive, we call it remote stag */
    uint32_t remote_stag;
  } stag;
};

int get_addr(const char *dst, struct sockaddr *addr) {
  struct addrinfo *res;
  int ret = -1;
  ret = getaddrinfo(dst, NULL, NULL, &res);
  if (ret) {
    std::cerr << "getaddrinfo failed - invalid hostname or IP address"
              << std::endl;
    return ret;
  }
  memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
  freeaddrinfo(res);
  return ret;
}

int process_cm_event(
  struct rdma_event_channel *ev_channel,
  enum rdma_cm_event_type expected_event,
  struct rdma_cm_event **cm_event
) {
  int ret = 1;
  ret = rdma_get_cm_event(ev_channel, cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to retrieve a cm event, errno: %d \n", -errno);
    return -errno;
  }
  if (0 != (*cm_event)->status) { // chech if it is a good event
    std::fprintf(stderr, "CM event has non zero status: %d\n",
                 (*cm_event)->status);
    ret = -((*cm_event)->status);
    rdma_ack_cm_event(*cm_event); // important, we acknowledge the event
    return ret;
  }
  if ((*cm_event)->event != expected_event) { // check event type
    std::fprintf(stderr, "Unexpected event received: %s [ expecting: %s ]",
                 rdma_event_str((*cm_event)->event),
                 rdma_event_str(expected_event));
    rdma_ack_cm_event(*cm_event);
    return -1; // unexpected event :(
  }
  std::fprintf(stderr, "A new %s type event is received \n",
               rdma_event_str((*cm_event)->event));
  return ret;
}

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr,
                                    uint32_t length,
                                    enum ibv_access_flags permission) {
  struct ibv_mr *mr = NULL;
  if (!pd) {
    std::fprintf(stderr, "Protection domain is NULL, ignoring \n");
    return NULL;
  }
  mr = ibv_reg_mr(pd, addr, length, permission);
  if (!mr) {
    std::fprintf(stderr, "Failed to create mr on buffer, errno: %d \n", -errno);
    return NULL;
  }
  std::printf("Registered: %p , len: %u , stag: 0x%x \n", mr->addr,
              (unsigned int)mr->length, mr->lkey);
  return mr;
}

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr) {
  if (!attr) {
    std::fprintf(stderr, "Passed ptr is NULL\n");
    return;
  }
  printf("---------------------------------------------------------\n");
  printf("buffer attr, addr: %p , len: %u , stag : 0x%x \n",
         (void *)attr->address, (unsigned int)attr->length,
         attr->stag.local_stag);
  printf("---------------------------------------------------------\n");
}

int process_work_completion_events(struct ibv_comp_channel *comp_channel,
                                   struct ibv_wc *wc, int max_wc) {
  struct ibv_cq *cq_ptr = NULL;
  void *context = NULL;
  int ret = -1, i, total_wc = 0;
  /* We wait for the notification on the CQ channel */
  ret = ibv_get_cq_event(
      comp_channel, /* IO channel where we are expecting the notification */
      &cq_ptr,   /* which CQ has an activity. This should be the same as CQ we
                    created before */
      &context); /* Associated CQ user context, which we did set */
  if (ret) {
    std::fprintf(stderr, "Failed to get next CQ event due to %d \n", -errno);
    return -errno;
  }
  /* Request for more notifications. */
  ret = ibv_req_notify_cq(cq_ptr, 0);
  if (ret) {
    std::fprintf(stderr, "Failed to request further notifications %d \n",
                 -errno);
    return -errno;
  }
  /* We got notification. We reap the work completion (WC) element. It is
   * unlikely but a good practice it write the CQ polling code that
   * can handle zero WCs. ibv_poll_cq can return zero. Same logic as
   * MUTEX conditional variables in pthread programming.
   */
  total_wc = 0;
  do {
    ret = ibv_poll_cq(cq_ptr /* the CQ, we got notification for */,
                      max_wc - total_wc /* number of remaining WC elements*/,
                      wc + total_wc /* where to store */);
    if (ret < 0) {
      std::fprintf(stderr, "Failed to poll cq for wc due to %d \n", ret);
      /* ret is errno here */
      return ret;
    }
    total_wc += ret;
  } while (total_wc < max_wc);
  std::printf("%d WC are completed \n", total_wc);
  /* Now we check validity and status of I/O work completions */
  for (i = 0; i < total_wc; i++) {
    if (wc[i].status != IBV_WC_SUCCESS) {
      std::fprintf(stderr,
                   "Work completion (WC) has error status: %s at index %d",
                   ibv_wc_status_str(wc[i].status), i);
      /* return negative value */
      return -(wc[i].status);
    }
  }
  /* Similar to connection management events, we need to acknowledge CQ events
   */
  ibv_ack_cq_events(cq_ptr, 
		       1 /* we received one event notification. This is not 
		       number of WC elements */);
  return total_wc;
}

struct ibv_mr *rdma_buffer_alloc(struct ibv_pd *pd, uint32_t size,
                                 enum ibv_access_flags permission) {
  struct ibv_mr *mr = NULL;
  if (!pd) {
    std::fprintf(stderr, "Protection domain is NULL \n");
    return NULL;
  }
  void *buf = calloc(1, size);
  if (!buf) {
    std::fprintf(stderr, "failed to allocate buffer, -ENOMEM\n");
    return NULL;
  }
  std::printf("Buffer allocated: %p , len: %u \n", buf, size);
  mr = rdma_buffer_register(pd, buf, size, permission);
  if (!mr) {
    free(buf);
  }
  return mr;
}

class ClientSession {
public:
  ClientSession(struct rdma_cm_id *cm_client_id)
    : cm_client_id(cm_client_id) {}
  int setup_connection();
  int exchange_metadata();
  int teardown();
  struct rdma_cm_id *cm_client_id;
  struct ibv_pd *pd;
  struct ibv_cq *cq;
  struct ibv_qp *qp;
  struct ibv_comp_channel *io_completion_channel;
  struct ibv_qp_init_attr qp_init_attr;
  struct ibv_mr *client_metadata_mr, *server_buffer_mr, *server_metadata_mr;
  struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
  struct ibv_sge recv_sge, send_sge;
  struct ibv_recv_wr recv_wr, *bad_recv_wr;
private:
};

int ClientSession::setup_connection() {
  int ret = 0;
  pd = ibv_alloc_pd(cm_client_id->verbs);
  if (!pd) {
    std::cerr << "Failed to allocate a protection domain errno: "
              << errno << std::endl;
    return -errno;
  }
  std::cout << "A new protection domain is allocated at " << pd << std::endl;
  io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
  if (!io_completion_channel) {
    std::cerr << "Failed to create an I/O completion event channel, "
              << errno << std::endl;
    return -errno;
  }
  std::cout << "An I/O completion event channel is created at " << 
          io_completion_channel << std::endl;
  cq = ibv_create_cq(cm_client_id->verbs /* which device*/, 
          CQ_CAPACITY /* maximum capacity*/, 
          NULL /* user context, not used here */,
          io_completion_channel /* which IO completion channel */, 
          0 /* signaling vector, not used here*/);
  if (!cq) {
    std::cerr << "Failed to create a completion queue (cq), errno: "
              << -errno << std::endl;
    return -errno;
  }
  std::printf("Completion queue (CQ) is created at %p with %d elements", cq, cq->cqe);
  /* Ask for the event for all activities in the completion queue*/
  ret = ibv_req_notify_cq(cq /* on which CQ */, 
          0 /* 0 = all event type, no filter*/);
  if (ret) {
    std::cerr << "Failed to request notifications on CQ errno: " << -errno << std::endl;
    return -errno;
  }
  bzero(&qp_init_attr, sizeof qp_init_attr);                                                         
  qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
  qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
  qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
  qp_init_attr.cap.max_send_wr = MAX_WR; /* Maximum send posting capacity */
  qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
  /* We use same completion queue, but one can use different queues */
  qp_init_attr.recv_cq = cq; /* Where should I notify for receive completion operations */
  qp_init_attr.send_cq = cq; /* Where should I notify for send completion operations */
  /*Lets create a QP */
  ret = rdma_create_qp(cm_client_id /* which connection id */,
          pd /* which protection domain*/,
          &qp_init_attr /* Initial attributes */);
  if (ret) {
    std::cerr << "Failed to create QP due to errno: " << -errno << std::endl;
    return -errno;
  }
  qp = cm_client_id->qp;
  std::cout << "Client QP created at " << qp << std::endl;
  if (!qp) {
    std::fprintf(stderr, "Client resources are not properly setup\n");
    return -EINVAL;
  }
  client_metadata_mr = rdma_buffer_register(
      pd /* which protection domain */, &client_metadata_attr /* what memory */,
      sizeof(client_metadata_attr) /* what length */,
      (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
  if (!client_metadata_mr) {
    std::fprintf(stderr, "Failed to register client attr buffer\n");
    // we assume ENOMEM
    return -ENOMEM;
  }
  /*  SGE credentials is where we receive the metadata from the client */
  recv_sge.addr =
      (uint64_t)client_metadata_mr->addr; // same as &client_buffer_attr
  recv_sge.length = client_metadata_mr->length;
  recv_sge.lkey = client_metadata_mr->lkey;
  /* Now we link this SGE to the work request (WR) */
  bzero(&recv_wr, sizeof(recv_wr));
  recv_wr.sg_list = &recv_sge;
  recv_wr.num_sge = 1; // only one SGE
  ret = ibv_post_recv(qp /* which QP */,
                      &recv_wr /* receive work request*/,
                      &bad_recv_wr /* error WRs */);
  if (ret) {
    std::fprintf(stderr, "Failed to pre-post the receive buffer, errno: %d \n",
                 ret);
    return ret;
  }
  std::puts("Receive buffer pre-posting is successful");
  return ret;
}

int ClientSession::exchange_metadata() {
  struct ibv_wc wc;
  int ret = -1;
  ret = process_work_completion_events(io_completion_channel, &wc, 1);
  if (ret != 1) {
    std::fprintf(stderr, "Failed to receive , ret = %d \n", ret);
    return ret;
  }
  /* if all good, then we should have client's buffer information, lets see */
  printf("Client side buffer information is received...\n");
  show_rdma_buffer_attr(&client_metadata_attr);
  printf("The client has requested buffer length of : %u bytes \n",
         client_metadata_attr.length);
  /* allocate memory region for client operations */
  server_buffer_mr = rdma_buffer_alloc(
      pd /* which protection domain */,
      client_metadata_attr.length /* what size to allocate */,
      (ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE) /* access permissions */);
  if (!server_buffer_mr) {
    std::fprintf(stderr, "Server failed to create a buffer \n");
    /* we assume that it is due to out of memory error */
    return -ENOMEM;
  }
  /* This buffer is used to transmit information about the above
   * buffer to the client. So this contains the metadata about the server
   * buffer. Hence this is called metadata buffer. Since this is already
   * on allocated, we just register it.
   * We need to prepare a send I/O operation that will tell the
   * client the address of the server buffer.
   */
  server_metadata_attr.address = (uint64_t)server_buffer_mr->addr;
  server_metadata_attr.length = (uint32_t)server_buffer_mr->length;
  server_metadata_attr.stag.local_stag = (uint32_t)server_buffer_mr->lkey;
  server_metadata_mr = rdma_buffer_register(
      pd /* which protection domain*/,
      &server_metadata_attr /* which memory to register */,
      sizeof(server_metadata_attr) /* what is the size of memory */,
      IBV_ACCESS_LOCAL_WRITE /* what access permission */);
  if (!server_metadata_mr) {
    std::fprintf(stderr, "Server failed to create to hold server metadata \n");
    /* we assume that this is due to out of memory error */
    return -ENOMEM;
  }
  // send metadata  TODO split this method
  struct ibv_send_wr server_send_wr, *bad_server_send_wr;
  struct ibv_sge server_send_sge;
  server_send_sge.addr = (uint64_t)&server_metadata_attr;
  server_send_sge.length = sizeof(server_metadata_attr);
  server_send_sge.lkey = server_metadata_mr->lkey;
  /* now we link this sge to the send request */
  bzero(&server_send_wr, sizeof(server_send_wr));
  server_send_wr.sg_list = &server_send_sge;
  server_send_wr.num_sge = 1;          // only 1 SGE element in the array
  server_send_wr.opcode = IBV_WR_SEND; // This is a send request
  server_send_wr.send_flags = IBV_SEND_SIGNALED; // We want to get notification
  /* This is a fast data path operation. Posting an I/O request */
  ret = ibv_post_send(
      qp /* which QP */,
      &server_send_wr /* Send request that we prepared before */,
      &bad_server_send_wr /* In case of error, this will contain failed requests */);
  if (ret) {
    std::fprintf(stderr, "Posting of server metdata failed, errno: %d \n",
                 -errno);
    return -errno;
  }
  /* We check for completion notification */
  ret = process_work_completion_events(io_completion_channel, &wc, 1);
  if (ret != 1) {
    std::fprintf(stderr, "Failed to send server metadata, ret = %d \n", ret);
    return ret;
  }
  std::puts("Local buffer metadata has been sent to the client");
  return 0;
}

int ClientSession::teardown() {
  return 0;
}

class Server {
public:
  struct sockaddr_in server_sockaddr;
  std::string server_addr;
  std::string server_port;
  struct rdma_event_channel *cm_event_channel;
  struct rdma_cm_id *cm_server_id;
  struct ibv_pd *pd;
  struct ibv_comp_channel *io_completion_channel;
  struct ibv_cq *cq;
  struct ibv_qp_init_attr qp_init_attr;
  struct ibv_qp *client_qp;

  Server(std::string server_addr, std::string server_port);
  int run(); // server loop
  int teardown();
private:
  // TODO hashmap zamiast vectora
  //std::vector<std::unique_ptr<ClientSession>> sessions;
  std::unordered_map<int, std::unique_ptr<ClientSession>> sessions;
  int handle_connect_request(struct rdma_cm_event *event);
  int handle_connection_established(struct rdma_cm_event *event);
  int handle_disconnect(struct rdma_cm_event *event);
  int connections;
};

Server::Server(std::string server_addr, std::string server_port) {
  cm_event_channel = nullptr;
  cm_server_id = nullptr;
  pd = nullptr;
  io_completion_channel = nullptr;
  cq = nullptr;
  this->server_addr = server_addr;
  this->server_port = server_port;
  connections = 0;
}

int Server::handle_connect_request(struct rdma_cm_event *event) {
  int ret = rdma_ack_cm_event(event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the cm event %d\n", -errno);
    return -errno;
  }
  // create client session
  if (!event->id) {
    std::cerr << "Client id is still NULL\n";
    return -EINVAL;
  }
  auto new_client = std::make_unique<ClientSession>(event->id);
  if (ret) {
    std::cerr << "failed to ack cm event errno: " << -errno << std::endl;
    return ret;
  }
  // set id in context
  int *id = new int{connections++};
  new_client->cm_client_id->context = id;
  // accept connection
  struct rdma_conn_param conn_param;
  struct sockaddr_in remote_sockaddr;
  memset(&conn_param, 0, sizeof(conn_param));
  /* this tell how many outstanding requests can we handle */
  conn_param.initiator_depth =
      3; /* For this exercise, we put a small number here */
  /* This tell how many outstanding requests we expect other side to handle */
  conn_param.responder_resources =
      3; /* For this exercise, we put a small number */
  ret = rdma_accept(new_client->cm_client_id, &conn_param);
  if (ret) {
    std::fprintf(stderr, "Failed to accept the connection, errno: %d \n",
                 -errno);
    return -errno;
  }
  // add new_client to sessions
  sessions[*id] = std::move(new_client);
  std::puts("Going to wait for : RDMA_CM_EVENT_ESTABLISHED event");
  return 0;
}

int Server::handle_connection_established(struct rdma_cm_event *event) {
  int ret = rdma_ack_cm_event(event);
  struct sockaddr_in remote_sockaddr;
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the cm event %d\n", -errno);
    return -errno;
  }
  std::memcpy(&remote_sockaddr /* where to save */,
              rdma_get_peer_addr(event->id) /* gives you remote sockaddr */,
              sizeof(struct sockaddr_in) /* max size */);
  std::printf("A new connection is accepted from %s \n",
              inet_ntoa(remote_sockaddr.sin_addr));
  // TODO exchange metadata
  sessions[*(int*)event->id->context]->exchange_metadata();
  return ret;
}

int Server::handle_disconnect(struct rdma_cm_event *event) {
  return 0;
}

int Server::run() {
  int ret = -1;
  // sockaddr structure for binding the connection manager
  memset(&(server_sockaddr), 0, sizeof this->server_sockaddr);
  server_sockaddr.sin_family = AF_INET;
  server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  // translate network address
  ret = get_addr(server_addr.c_str(), (struct sockaddr *)&server_sockaddr);
  if (ret) {
    std::cerr << "Invalid IP" << std::endl;
    return ret;
  }
  server_sockaddr.sin_port = htons(std::stol(server_port));

  struct rdma_cm_event *cm_event = NULL;
  //  Open a channel used to report asynchronous communication event
  cm_event_channel = rdma_create_event_channel();
  if (!cm_event_channel) {
    std::fprintf(stderr, "Creating cm event channel failed with errno : (%d)",
                 -errno);
    return -errno;
  }
  std::printf("RDMA CM event channel is created successfully at %p \n",
              cm_event_channel);
  /* rdma_cm_id is the connection identifier (like socket) which is used
   * to define an RDMA connection. */
  ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
  if (ret) {
    std::fprintf(stderr, "Creating server cm id failed with errno: %d ",
                 -errno);
    return -errno;
  }
  std::puts("A RDMA connection id for the server is created");
  /* Explicit binding of rdma cm id to the socket credentials */
  ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)&server_sockaddr);
  if (ret) {
    std::fprintf(stderr, "Failed to bind server address, errno: %d \n", -errno);
    return -errno;
  }
  std::puts("Server RDMA CM id is successfully binded");
  ret = rdma_listen(cm_server_id, 8);
  if (ret) {
    std::cerr << "rdma_listen failed to listen on server address, errno:"
              << -errno << std::endl;
  }
  std::printf("Server is listening successfully at: %s , port: %d \n",
          inet_ntoa(server_sockaddr.sin_addr), ntohs(server_sockaddr.sin_port));
  while (!ret) {
    ret = rdma_get_cm_event(cm_event_channel, &cm_event);
    if (ret) {
      std::fprintf(stderr, "Failed to retrieve a cm event, errno: %d\n", -errno);
      return -errno;
    }
    if (0 != cm_event->status) { // check if it is a good event
      std::fprintf(stderr, "CM event has non zero status: %d\n",
                   cm_event->status);
      ret = -(cm_event->status);
      rdma_ack_cm_event(cm_event); // release event
      return ret;
    }
    switch (cm_event->event) { // check event type
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            ret = handle_connect_request(cm_event);
            if (ret) {
              std::cerr << "handle_connect_request error: " << ret << std::endl;
            }
            break;
        case RDMA_CM_EVENT_ESTABLISHED:
            ret = handle_connection_established(cm_event);
            if (ret) {
              std::cerr << "handle_connection_established error: " << ret
                        << std::endl;
            }
            break;
        case RDMA_CM_EVENT_DISCONNECTED:
            ret = handle_disconnect(cm_event);
            if (ret) {
              std::cerr << "handle_disconnect error: " << ret << std::endl;
            }
            break;
        default: // TODO handle default
          break;
    }
  }
  return ret;
}

int main() {
  return 0;
}