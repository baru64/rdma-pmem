#include <cstring>
#include <getopt.h>
#include <iostream>

#include "common.hpp"

class Server {
public:
  Server(std::string server_addr, std::string server_port);
  int init();
  int cleanup();
  int exchange_metadata_with_client();
  void print_buffer();

private:
  struct sockaddr_in server_sockaddr;
  std::string server_addr;
  std::string server_port;
  struct rdma_event_channel *cm_event_channel;
  struct rdma_cm_id *cm_server_id, *cm_client_id;
  struct ibv_pd *pd;
  struct ibv_comp_channel *io_completion_channel;
  struct ibv_cq *cq;
  struct ibv_qp_init_attr qp_init_attr;
  struct ibv_qp *client_qp;
  /* RDMA memory resources */
  struct ibv_mr *client_metadata_mr, *server_buffer_mr, *server_metadata_mr;
  struct rdma_buffer_attr client_metadata_attr, server_metadata_attr;
  struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr;
  struct ibv_send_wr server_send_wr, *bad_server_send_wr;
  struct ibv_sge client_recv_sge, server_send_sge;
};

Server::Server(std::string server_addr, std::string server_port) {
  cm_event_channel = NULL;
  cm_client_id = NULL;
  cm_server_id = NULL;
  pd = NULL;
  io_completion_channel = NULL;
  cq = NULL;
  client_qp = NULL;
  client_metadata_mr = NULL;
  server_metadata_mr = NULL;
  server_buffer_mr = NULL;
  bad_client_recv_wr = NULL;
  bad_server_send_wr = NULL;
  this->server_addr = server_addr;
  this->server_port = server_port;
}

void Server::print_buffer() {
  std::string buffstr((char *)server_buffer_mr->addr, server_buffer_mr->length);
  std::cout << "my string: " << buffstr << std::endl;
}

int Server::init() {
  int ret = -1;
  // sockaddr
  std::memset(&(server_sockaddr), 0, sizeof this->server_sockaddr);
  server_sockaddr.sin_family = AF_INET;
  server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  ret = get_addr(server_addr.c_str(), (struct sockaddr *)&server_sockaddr);
  if (ret) {
    std::cerr << "Invalid IP" << std::endl;
    return ret;
  }
  server_sockaddr.sin_port = htons(std::stol(server_port));

  // start server
  struct rdma_cm_event *cm_event = NULL;
  /*  Open a channel used to report asynchronous communication event */
  cm_event_channel = rdma_create_event_channel();
  if (!cm_event_channel) {
    std::fprintf(stderr, "Creating cm event channel failed with errno : (%d)",
                 -errno);
    return -errno;
  }
  std::printf("RDMA CM event channel is created successfully at %p \n",
              cm_event_channel);
  /* rdma_cm_id is the connection identifier (like socket) which is used
   * to define an RDMA connection.
   */
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
  ret = rdma_listen(cm_server_id,
                    8); /* backlog = 8 clients, same as TCP, see man listen*/
  if (ret) {
    std::fprintf(stderr,
                 "rdma_listen failed to listen on server address, errno: %d ",
                 -errno);
    return -errno;
  }
  printf("Server is listening successfully at: %s , port: %d \n",
         inet_ntoa(server_sockaddr.sin_addr), ntohs(server_sockaddr.sin_port));
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_CONNECT_REQUEST,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to get cm event, ret = %d \n", ret);
    return ret;
  }
  cm_client_id = cm_event->id;
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the cm event errno: %d \n",
                 -errno);
    return -errno;
  }
  std::printf("A new RDMA client connection id is stored at %p\n",
              cm_client_id);

  // setup client resources
  if (!cm_client_id) {
    std::cerr << "Client id is still NULL \n";
    return -EINVAL;
  }
  pd = ibv_alloc_pd(cm_client_id->verbs 
            /* verbs defines a verb's provider, 
             * i.e an RDMA device where the incoming 
             * client connection came */);
  if (!pd) {
    std::cerr << "Failed to allocate a protection domain errno: "
              << errno << std::endl;
    return -errno;
  }
  std::cout << "A new protection domain is allocated at " << pd << std::endl;
  /* Now we need a completion channel, were the I/O completion 
   * notifications are sent. Remember, this is different from connection 
   * management (CM) event notifications. 
   * A completion channel is also tied to an RDMA device, hence we will 
   * use cm_client_id->verbs. 
   */
  io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
  if (!io_completion_channel) {
    std::cerr << "Failed to create an I/O completion event channel, "
              << errno << std::endl;
    return -errno;
  }
  std::cout << "An I/O completion event channel is created at " << 
          io_completion_channel << std::endl;
  /* Now we create a completion queue (CQ) where actual I/O 
   * completion metadata is placed. The metadata is packed into a structure 
   * called struct ibv_wc (wc = work completion). ibv_wc has detailed 
   * information about the work completion. An I/O request in RDMA world 
   * is called "work" ;) 
   */
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
  /* Save the reference for handy typing but is not required */
  client_qp = cm_client_id->qp;
  std::cout << "Client QP created at " << client_qp << std::endl;

  // accept connection
  struct rdma_conn_param conn_param;
  struct sockaddr_in remote_sockaddr;
  if (!cm_client_id || !client_qp) {
    std::fprintf(stderr, "Client resources are not properly setup\n");
    return -EINVAL;
  }
  /* we prepare the receive buffer in which we will receive the client
   * metadata*/
  client_metadata_mr = rdma_buffer_register(
      pd /* which protection domain */, &client_metadata_attr /* what memory */,
      sizeof(client_metadata_attr) /* what length */,
      (IBV_ACCESS_LOCAL_WRITE) /* access permissions */);
  if (!client_metadata_mr) {
    std::fprintf(stderr, "Failed to register client attr buffer\n");
    // we assume ENOMEM
    return -ENOMEM;
  }
  /* We pre-post this receive buffer on the QP. SGE credentials is where we
   * receive the metadata from the client */
  client_recv_sge.addr =
      (uint64_t)client_metadata_mr->addr; // same as &client_buffer_attr
  client_recv_sge.length = client_metadata_mr->length;
  client_recv_sge.lkey = client_metadata_mr->lkey;
  /* Now we link this SGE to the work request (WR) */
  bzero(&client_recv_wr, sizeof(client_recv_wr));
  client_recv_wr.sg_list = &client_recv_sge;
  client_recv_wr.num_sge = 1; // only one SGE
  ret = ibv_post_recv(client_qp /* which QP */,
                      &client_recv_wr /* receive work request*/,
                      &bad_client_recv_wr /* error WRs */);
  if (ret) {
    std::fprintf(stderr, "Failed to pre-post the receive buffer, errno: %d \n",
                 ret);
    return ret;
  }
  std::puts("Receive buffer pre-posting is successful");
  memset(&conn_param, 0, sizeof(conn_param));
  /* this tell how many outstanding requests can we handle */
  conn_param.initiator_depth =
      3; /* For this exercise, we put a small number here */
  /* This tell how many outstanding requests we expect other side to handle */
  conn_param.responder_resources =
      3; /* For this exercise, we put a small number */
  ret = rdma_accept(cm_client_id, &conn_param);
  if (ret) {
    std::fprintf(stderr, "Failed to accept the connection, errno: %d \n",
                 -errno);
    return -errno;
  }
  std::puts("Going to wait for : RDMA_CM_EVENT_ESTABLISHED event");
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_ESTABLISHED,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to get the cm event, errnp: %d \n", -errno);
    return -errno;
  }
  /* We acknowledge the event */
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the cm event %d\n", -errno);
    return -errno;
  }
  /* Just FYI: How to extract connection information */
  std::memcpy(&remote_sockaddr /* where to save */,
              rdma_get_peer_addr(cm_client_id) /* gives you remote sockaddr */,
              sizeof(struct sockaddr_in) /* max size */);
  std::printf("A new connection is accepted from %s \n",
              inet_ntoa(remote_sockaddr.sin_addr));
  return 0;
}

int Server::exchange_metadata_with_client() {
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
  /* We need to setup requested memory buffer. This is where the client will
   * do RDMA READs and WRITEs. */
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
  /* We need to transmit this buffer. So we create a send request.
   * A send request consists of multiple SGE elements. In our case, we only
   * have one
   */
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
      client_qp /* which QP */,
      &server_send_wr /* Send request that we prepared before */, &bad_server_send_wr /* In case of error, this will contain failed requests */);
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

int Server::cleanup() {
  struct rdma_cm_event *cm_event = NULL;
  int ret = -1;
  /* Now we wait for the client to send us disconnect event */
  std::puts("Waiting for cm event: RDMA_CM_EVENT_DISCONNECTED");
  ret = process_rdma_cm_event(cm_event_channel, RDMA_CM_EVENT_DISCONNECTED,
                              &cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to get disconnect event, ret = %d \n", ret);
    return ret;
  }
  /* We acknowledge the event */
  ret = rdma_ack_cm_event(cm_event);
  if (ret) {
    std::fprintf(stderr, "Failed to acknowledge the cm event %d\n", -errno);
    return -errno;
  }
  printf("A disconnect event is received from the client...\n");
  /* We free all the resources */
  /* Destroy QP */
  rdma_destroy_qp(cm_client_id);
  /* Destroy client cm id */
  ret = rdma_destroy_id(cm_client_id);
  if (ret) {
    std::fprintf(stderr, "Failed to destroy client id cleanly, %d \n", -errno);
    // we continue anyways;
  }
  /* Destroy CQ */
  ret = ibv_destroy_cq(cq);
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
  rdma_buffer_free(server_buffer_mr);
  rdma_buffer_deregister(server_metadata_mr);
  rdma_buffer_deregister(client_metadata_mr);
  /* Destroy protection domain */
  ret = ibv_dealloc_pd(pd);
  if (ret) {
    std::fprintf(stderr,
                 "Failed to destroy client protection domain cleanly, %d \n",
                 -errno);
    // we continue anyways;
  }
  /* Destroy rdma server id */
  ret = rdma_destroy_id(cm_server_id);
  if (ret) {
    std::fprintf(stderr, "Failed to destroy server id cleanly, %d \n", -errno);
    // we continue anyways;
  }
  rdma_destroy_event_channel(cm_event_channel);
  std::puts("Server shut-down is complete");
  return 0;
}

int main(int argc, char *argv[]) {
  std::string server_addr;
  std::string server_port;
  option opts[]{{"serveraddr", required_argument, nullptr, 'a'},
                {"serverport", required_argument, nullptr, 'p'},
                {/**/}};
  char c;
  while (-1 != (c = getopt_long(argc, argv, "a:p:", opts, nullptr))) {
    switch (c) {
    case 'a':
      server_addr = optarg;
      break;
    case 'p':
      server_port = optarg;
      break;
    default:
      std::puts("Supported options:");
      for (option *o = opts; o->name != nullptr; ++o)
        std::printf(" -%c --%s\n", o->val, o->name);
      return 0;
    }
  }
  std::cout << "server starts" << std::endl;
  std::cout << server_addr << " " << server_port << std::endl;
  auto server = new Server(server_addr, server_port);
  int ret = 0;
  ret = server->init();
  if (ret) {
    std::cerr << "Failed to initialise server, ret = " << ret << std::endl;
    return ret;
  }
  ret = server->exchange_metadata_with_client();
  if (ret) {
    std::cerr << "Failed to exchange metadata, ret = " << ret << std::endl;
    return ret;
  }
  server->print_buffer();
  ret = server->cleanup();
  if (ret) {
    std::cerr << "Failed to exchange metadata, ret = " << ret << std::endl;
    return ret;
  }
  return 0;
}
