#ifndef COMMON_H
#define COMMON_H

#include <cstdio>
#include <iostream>
#include <thread>
#include <atomic>

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

int get_addr(const char *dst, struct sockaddr *addr);

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr);

int process_rdma_cm_event(struct rdma_event_channel *echannel,
                          enum rdma_cm_event_type expected_event,
                          struct rdma_cm_event **cm_event);

struct ibv_mr *rdma_buffer_alloc(struct ibv_pd *pd, uint32_t length,
                                 enum ibv_access_flags permission);

void rdma_buffer_free(struct ibv_mr *mr);

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr,
                                    uint32_t length,
                                    enum ibv_access_flags permission);

void rdma_buffer_deregister(struct ibv_mr *mr);

int process_work_completion_events(struct ibv_comp_channel *comp_channel,
                                   struct ibv_wc *wc, int max_wc);

void show_rdma_cmid(struct rdma_cm_id *id);

struct Statistics {
     int thread_id;

     uint64_t ops = 0;
     uint64_t latency = 0;
     uint64_t latency_ticks = 0;
     uint64_t elapsed_nanoseconds = 0;

     explicit Statistics(int thread_id) : thread_id(thread_id) {}
     static std::string GetHeader();
     std::string ToString() const;
};

#endif /* COMMON_H */
