#include <cstring>
#include <getopt.h>
#include <iostream>
#include <string>

#include "common.hpp"

class Client {
public:
  Client(std::string server_addr, std::string server_port);
  int init();
  int cleanup();
  int send_write();
  int send_read();
  int exchange_metadata_with_server();
  int set_src(std::string str);
  int alloc_dst(std::string str);
  int cmp_data();

private:
  struct sockaddr_in server_sockaddr;
  std::string server_addr;
  std::string server_port;
  /* These are RDMA connection related resources */
  struct rdma_event_channel *cm_event_channel;
  struct rdma_cm_id *cm_client_id;
  struct ibv_pd *pd;
  struct ibv_comp_channel *io_completion_channel;
  struct ibv_cq *client_cq;
  struct ibv_qp_init_attr qp_init_attr;
  struct ibv_qp *client_qp;
  /* These are memory buffers related resources */
  struct ibv_mr *client_metadata_mr, *client_src_mr, *client_dst_mr,
      *server_metadata_mr;
  struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
  struct ibv_send_wr client_send_wr, *bad_client_send_wr;
  struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr;
  struct ibv_sge client_send_sge, server_recv_sge;
  // buffers for rdma operations
  char *src;
  char *dst;
};

int Client::cmp_data() {
  return std::strcmp(src, dst);
}

int Client::set_src(std::string str) {
  src = (char *)std::calloc(str.length() + 1, 1);
  if (!src) {
    free(src);
    return -ENOMEM;
  }
  std::strncpy(src, str.c_str(), str.length() + 1);
  return 0;
}

int Client::alloc_dst(std::string str) {
  dst = (char *)std::calloc(str.length() + 1, 1);
  if (!dst) {
    free(dst);
    return -ENOMEM;
  }
  return 0;
}

Client::Client(std::string server_addr, std::string server_port) {
  cm_event_channel = NULL;
  cm_client_id = NULL;
  pd = nullptr;
  io_completion_channel = NULL;
  client_cq = NULL;
  client_qp = NULL;
  client_metadata_mr = NULL;
  client_src_mr = NULL;
  client_dst_mr = NULL;
  server_metadata_mr = NULL;
  bad_client_send_wr = NULL;
  bad_server_recv_wr = NULL;
  this->server_addr = server_addr;
  this->server_port = server_port;
  src = NULL;
  dst = NULL;
}

int Client::init() {
  int ret = -1;
  // todo client_prepare_connection etc
  // sockaddr
  std::memset(&(server_sockaddr), 0, sizeof this->server_sockaddr);
  server_sockaddr.sin_family = AF_INET;
  server_sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ret = get_addr(server_addr.c_str(), (struct sockaddr *)&server_sockaddr);
  if (ret) {
    std::cerr << "Invalid IP" << std::endl;
    return ret;
  }
  server_sockaddr.sin_port = htons(std::stol(server_port));

  // prepare connection
  struct rdma_cm_event *cm_event = NULL;
  cm_event_channel = rdma_create_event_channel();
  if (!cm_event_channel) {
    std::fprintf(stderr, "Creating cm event channel failed, errno: %d \n",
                 -errno);
    return -errno;
  }
  std::printf("RDMA CM event channel is created at : %p \n", cm_event_channel);
  ret = rdma_create_id(cm_event_channel, &cm_client_id, NULL, RDMA_PS_TCP);
  if (ret) {
    std::fprintf(stderr, "Creating cm id failed with errno: %d \n", -errno);
    return -errno;
  }
  /* Resolve destination and optional source addresses from IP addresses  to
   * an RDMA address.  If successful, the specified rdma_cm_id will be bound
   * to a local device. */
  ret = rdma_resolve_addr(cm_client_id, NULL,
                          (struct sockaddr *)&server_sockaddr, 2000);
  if (ret) {
    std::fprintf(stderr, "Failed to resolve address, errno: %d \n", -errno);
    return -errno;
  }
  std::printf("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ADDR_RESOLVED,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to receive a valid event, ret = %d \n", ret);
    return ret;
  }
  /* we ack the event */
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the CM event, errno: %d\n",
                 -errno);
    return -errno;
  }
  std::puts("RDMA address is resolved");
  ret = rdma_resolve_route(cm_client_id, 2000);
  if (ret) {
    std::fprintf(stderr, "Failed to resolve route, erno: %d \n", -errno);
    return -errno;
  }
  std::puts("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED");
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ROUTE_RESOLVED,
                              &cm_event);
  if (ret) {
    fprintf(stderr, "Failed to receive a valid event, ret = %d \n", ret);
    return ret;
  }
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    fprintf(stderr, "Failed to acknowledge the CM event, errno: %d \n", -errno);
    return -errno;
  }
  printf("Trying to connect to server at : %s port: %d \n",
         inet_ntoa(server_sockaddr.sin_addr), ntohs(server_sockaddr.sin_port));
  pd = ibv_alloc_pd(cm_client_id->verbs);
  if (!pd) {
    fprintf(stderr, "Failed to alloc pd, errno: %d \n", -errno);
    return -errno;
  }
  std::printf("pd allocated at %p \n", pd);
  io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
  if (!io_completion_channel) {
    std::fprintf(stderr,
                 "Failed to create IO completion event channel, errno: %d\n",
                 -errno);
    return -errno;
  }
  std::printf("completion event channel created at : %p \n",
              io_completion_channel);
  client_cq = ibv_create_cq(
      cm_client_id->verbs /* which device*/, CQ_CAPACITY /* maximum capacity*/,
      NULL /* user context, not used here */,
      io_completion_channel /* which IO completion channel */,
      0 /* signaling vector, not used here*/);
  if (!client_cq) {
    std::fprintf(stderr, "Failed to create CQ, errno: %d \n", -errno);
    return -errno;
  }
  std::printf("CQ created at %p with %d elements \n", client_cq,
              client_cq->cqe);
  ret = ibv_req_notify_cq(client_cq, 0);
  if (ret) {
    std::fprintf(stderr, "Failed to request notifications, errno: %d\n",
                 -errno);
    return -errno;
  }
  bzero(&qp_init_attr, sizeof qp_init_attr);
  qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
  qp_init_attr.cap.max_recv_wr = MAX_WR; /* Maximum receive posting capacity */
  qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
  qp_init_attr.cap.max_send_wr = MAX_WR;   /* Maximum send posting capacity */
  qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
  qp_init_attr.recv_cq =
      client_cq; /* Where should I notify for receive completion operations */
  qp_init_attr.send_cq =
      client_cq; /* Where should I notify for send completion operations */
  ret = rdma_create_qp(cm_client_id /* which connection id */,
                       pd /* which protection domain*/,
                       &qp_init_attr /* Initial attributes */);
  if (ret) {
    std::fprintf(stderr, "Failed to create QP, errno: %d \n", -errno);
    return -errno;
  }
  client_qp = cm_client_id->qp;
  std::printf("QP created at %p \n", client_qp);

  // pre-post receive buffer before rdma-connect
  server_metadata_mr = rdma_buffer_register(pd, &server_metadata_attr,
                                            sizeof(server_metadata_attr),
                                            (IBV_ACCESS_LOCAL_WRITE));
  if (!server_metadata_mr) {
    std::fprintf(stderr, "Failed to setup the server metadata mr ,-ENOMEM\n");
    return -ENOMEM;
  }
  server_recv_sge.addr = (uint64_t)server_metadata_mr->addr;
  server_recv_sge.length = (uint32_t)server_metadata_mr->length;
  server_recv_sge.lkey = (uint32_t)server_metadata_mr->lkey;
  /* now we link it to the request */
  std::memset(&server_recv_wr, 0, sizeof(server_recv_wr));
  server_recv_wr.sg_list = &server_recv_sge;
  server_recv_wr.num_sge = 1;
  ret = ibv_post_recv(client_qp /* which QP */,
                      &server_recv_wr /* receive work request*/,
                      &bad_server_recv_wr /* error WRs */);
  if (ret) {
    std::fprintf(stderr, "Failed to pre-post the receive buffer, errno: %d \n",
                 ret);
    return ret;
  }
  std::puts("Receive buffer pre-posting is successful \n");

  // connects to the RDMA server
  struct rdma_conn_param conn_param;
  std::memset(&conn_param, 0, sizeof(conn_param));
  conn_param.initiator_depth = 3;
  conn_param.responder_resources = 3;
  conn_param.retry_count = 3; // if fail, then how many times to retry
  ret = rdma_connect(cm_client_id, &conn_param);
  if (ret) {
    std::fprintf(stderr, "Failed to connect to remote host , errno: %d\n",
                 -errno);
    return -errno;
  }
  std::puts("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED");
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to get cm event, ret = %d \n", ret);
    return ret;
  }
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge cm event, errno: %d\n", -errno);
    return -errno;
  }
  printf("The client is connected successfully \n");

  return 0;
}

int Client::exchange_metadata_with_server() {
  struct ibv_wc wc[2];
  int ret = -1;
  client_src_mr = rdma_buffer_register(
      pd, src, strlen(src),
      (ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                         IBV_ACCESS_REMOTE_WRITE));
  if (!client_src_mr) {
    std::fprintf(stderr, "Failed to register the first buffer, ret = %d \n",
                 ret);
    return ret;
  }
  /* we prepare metadata for the first buffer */
  client_metadata_attr.address = (uint64_t)client_src_mr->addr;
  client_metadata_attr.length = client_src_mr->length;
  client_metadata_attr.stag.local_stag = client_src_mr->lkey;
  /* now we register the metadata memory */
  client_metadata_mr = rdma_buffer_register(pd, &client_metadata_attr,
                                            sizeof(client_metadata_attr),
                                            IBV_ACCESS_LOCAL_WRITE);
  if (!client_metadata_mr) {
    std::fprintf(stderr,
                 "Failed to register the client metadata buffer, ret = %d \n",
                 ret);
    return ret;
  }
  /* now we fill up SGE */
  client_send_sge.addr = (uint64_t)client_metadata_mr->addr;
  client_send_sge.length = (uint32_t)client_metadata_mr->length;
  client_send_sge.lkey = client_metadata_mr->lkey;
  /* now we link to the send work request */
  std::memset(&client_send_wr, 0, sizeof(client_send_wr));
  client_send_wr.sg_list = &client_send_sge;
  client_send_wr.num_sge = 1;
  client_send_wr.opcode = IBV_WR_SEND;
  client_send_wr.send_flags = IBV_SEND_SIGNALED;
  /* Now we post it */
  ret = ibv_post_send(client_qp, &client_send_wr, &bad_client_send_wr);
  if (ret) {
    std::fprintf(stderr, "Failed to send client metadata, errno: %d \n",
                 -errno);
    return -errno;
  }
  /* at this point we are expecting 2 work completion. One for our
   * send and one for recv that we will get from the server for
   * its buffer information */
  ret = process_work_completion_events(io_completion_channel, wc, 2);
  if (ret != 2) {
    std::fprintf(stderr, "We failed to get 2 work completions , ret = %d \n",
                 ret);
    return ret;
  }
  std::puts("Server sent us its buffer location and credentials, showing \n");
  show_rdma_buffer_attr(&server_metadata_attr);
  return 0;
}

int Client::send_write() {
  struct ibv_wc wc;
  int ret = -1;
  /* Step 1: is to copy the local buffer into the remote buffer. We will
   * reuse the previous variables. */
  /* now we fill up SGE */
  client_send_sge.addr = (uint64_t)client_src_mr->addr;
  client_send_sge.length = (uint32_t)client_src_mr->length;
  client_send_sge.lkey = client_src_mr->lkey;
  /* now we link to the send work request */
  bzero(&client_send_wr, sizeof(client_send_wr));
  client_send_wr.sg_list = &client_send_sge;
  client_send_wr.num_sge = 1;
  client_send_wr.opcode = IBV_WR_RDMA_WRITE;
  client_send_wr.send_flags = IBV_SEND_SIGNALED;
  /* we have to tell server side info for RDMA */
  client_send_wr.wr.rdma.rkey = server_metadata_attr.stag.remote_stag;
  client_send_wr.wr.rdma.remote_addr = server_metadata_attr.address;
  /* Now we post it */
  ret = ibv_post_send(client_qp, &client_send_wr, &bad_client_send_wr);
  if (ret) {
    std::fprintf(stderr, "Failed to write client src buffer, errno: %d \n",
                 -errno);
    return -errno;
  }
  /* at this point we are expecting 1 work completion for the write */
  ret = process_work_completion_events(io_completion_channel, &wc, 1);
  if (ret != 1) {
    std::fprintf(stderr, "We failed to get 1 work completions , ret = %d \n",
                 ret);
    return ret;
  }
  std::puts("Client side WRITE is complete");
  return 0;
}

int Client::send_read() {
  struct ibv_wc wc;
  int ret = -1;
  std::cout << "strlen src: " << strlen(src) << std::endl;
  client_dst_mr = rdma_buffer_register(
      pd, dst, strlen(src),
      (ibv_access_flags)(IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE |
                         IBV_ACCESS_REMOTE_READ));
  if (!client_dst_mr) {
    std::fprintf(stderr,
                 "We failed to create the destination buffer, -ENOMEM\n");
    return -ENOMEM;
  }
  client_send_sge.addr = (uint64_t)client_dst_mr->addr;
  client_send_sge.length = (uint32_t)client_dst_mr->length;
  client_send_sge.lkey = client_dst_mr->lkey;
  /* now we link to the send work request */
  bzero(&client_send_wr, sizeof(client_send_wr));
  client_send_wr.sg_list = &client_send_sge;
  client_send_wr.num_sge = 1;
  client_send_wr.opcode = IBV_WR_RDMA_READ;
  client_send_wr.send_flags = IBV_SEND_SIGNALED;
  /* we have to tell server side info for RDMA */
  client_send_wr.wr.rdma.rkey = server_metadata_attr.stag.remote_stag;
  client_send_wr.wr.rdma.remote_addr = server_metadata_attr.address;
  /* Now we post it */
  ret = ibv_post_send(client_qp, &client_send_wr, &bad_client_send_wr);
  if (ret) {
    std::fprintf(
        stderr,
        "Failed to read client dst buffer from the master, errno: %d \n",
        -errno);
    return -errno;
  }
  /* at this point we are expecting 1 work completion for the write */
  ret = process_work_completion_events(io_completion_channel, &wc, 1);
  if (ret != 1) {
    std::fprintf(stderr, "We failed to get 1 work completions , ret = %d \n",
                 ret);
    return ret;
  }
  std::puts("Client side READ is complete");
  return 0;
}

int Client::cleanup() {
  struct rdma_cm_event *cm_event = NULL;
  int ret = -1;
  /* active disconnect from the client side */
  ret = rdma_disconnect(cm_client_id);
  if (ret) {
    std::fprintf(stderr, "Failed to disconnect, errno: %d \n", -errno);
    // continuing anyways
  }
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr,
                 "Failed to get RDMA_CM_EVENT_DISCONNECTED event, ret = %d\n",
                 ret);
    // continuing anyways
  }
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge cm event, errno: %d\n", -errno);
    // continuing anyways
  }
  /* Destroy QP */
  rdma_destroy_qp(cm_client_id);
  /* Destroy client cm id */
  ret = rdma_destroy_id(cm_client_id);
  if (ret) {
    std::fprintf(stderr, "Failed to destroy client id cleanly, %d \n", -errno);
    // we continue anyways;
  }
  /* Destroy CQ */
  ret = ibv_destroy_cq(client_cq);
  if (ret) {
    std::fprintf(stderr, "Failed to destroy completion queue cleanly, %d \n",
                 -errno);
    // we continue anyways;
  }
  /* Destroy completion channel */
  ret = ibv_destroy_comp_channel(io_completion_channel);
  if (ret) {
    std::fprintf(stderr, "Failed to destroy completion channel cleanly, %d \n",
                 -errno);
    // we continue anyways;
  }
  /* Destroy memory buffers */
  rdma_buffer_deregister(server_metadata_mr);
  rdma_buffer_deregister(client_metadata_mr);
  rdma_buffer_deregister(client_src_mr);
  rdma_buffer_deregister(client_dst_mr);
  /* We free the buffers */
  free(src);
  free(dst);
  /* Destroy protection domain */
  ret = ibv_dealloc_pd(pd);
  if (ret) {
    std::fprintf(stderr,
                 "Failed to destroy client protection domain cleanly, %d \n",
                 -errno);
    // we continue anyways;
  }
  rdma_destroy_event_channel(cm_event_channel);
  printf("Client resource clean up is complete \n");
  return 0;
}

int main(int argc, char *argv[]) {
  std::string server_addr = "127.0.0.1";
  std::string server_port = "2000";
  std::string data = "hello";
  std::cout << "client starts" << std::endl;
  option opts[]{{"serveraddr", required_argument, nullptr, 'a'},
                {"serverport", required_argument, nullptr, 'p'},
                {"data", required_argument, nullptr, 'd'},
                {/**/}};
  char c;
  while (-1 != (c = getopt_long(argc, argv, "a:p:d:", opts, nullptr))) {
    switch (c) {
    case 'a':
      server_addr = optarg;
      break;
    case 'p':
      server_port = optarg;
      break;
    case 'd':
      data = optarg;
      break;
    default:
      std::puts("Supported options:");
      for (option *o = opts; o->name != nullptr; ++o)
        std::printf(" -%c --%s\n", o->val, o->name);
      return 0;
    }
  }
  std::cout << server_addr << " " << server_port << std::endl;
  auto client = new Client(server_addr, server_port);
  int ret = 0;
  ret = client->set_src(data);
  ret = client->alloc_dst(data);
  if (ret) {
    std::cerr << "Failed to set source buffer, ret = " << ret << std::endl;
    return ret;
  }
  ret = client->init();
  if (ret) {
    std::cerr << "Failed to initialise client, ret = " << ret << std::endl;
    return ret;
  }
  ret = client->exchange_metadata_with_server();
  if (ret) {
    std::cerr << "Failed to exchange metadata, ret = " << ret << std::endl;
    return ret;
  }
  ret = client->send_write();
  if (ret) {
    std::cerr << "Failed to write, ret = " << ret << std::endl;
    return ret;
  }
  ret = client->send_read();
  if (ret) {
    std::cerr << "Failed to read, ret = " << ret << std::endl;
    return ret;
  }
  if (ret) {
    std::cerr << "Buffers are not equal, ret = " << ret << std::endl;
  } else {
    std::cout << "BUFFERS ARE EQUAL!!" << std::endl;
  }
  ret = client->cleanup();
  if (ret) {
    std::cerr << "Failed to disconnect and clean up, ret =" << ret << std::endl;
    return ret;
  }
  return 0;
}
