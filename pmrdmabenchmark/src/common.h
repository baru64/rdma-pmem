#include <stdlib.h>
#include <sys/types.h>
#include <endian.h>
#include <poll.h>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>
#include <infiniband/ib.h>

/* Defined in common.c; used in all rsocket demos to determine whether to use
 * rsocket calls or standard socket calls.
 */
extern int use_rs;

static inline int rs_socket(int f, int t, int p)
{
	int fd;

	if (!use_rs)
		return socket(f, t, p);

	fd = rsocket(f, t, p);
	if (fd < 0) {
		if (t == SOCK_STREAM && errno == ENODEV)
			fprintf(stderr, "No RDMA devices were detected\n");
		else
			perror("rsocket failed");
	}
	return fd;
}

#define rs_bind(s,a,l)    use_rs ? rbind(s,a,l)    : bind(s,a,l)
#define rs_listen(s,b)    use_rs ? rlisten(s,b)    : listen(s,b)
#define rs_connect(s,a,l) use_rs ? rconnect(s,a,l) : connect(s,a,l)
#define rs_accept(s,a,l)  use_rs ? raccept(s,a,l)  : accept(s,a,l)
#define rs_shutdown(s,h)  use_rs ? rshutdown(s,h)  : shutdown(s,h)
#define rs_close(s)       use_rs ? rclose(s)       : close(s)
#define rs_recv(s,b,l,f)  use_rs ? rrecv(s,b,l,f)  : recv(s,b,l,f)
#define rs_send(s,b,l,f)  use_rs ? rsend(s,b,l,f)  : send(s,b,l,f)
#define rs_recvfrom(s,b,l,f,a,al) \
	use_rs ? rrecvfrom(s,b,l,f,a,al) : recvfrom(s,b,l,f,a,al)
#define rs_sendto(s,b,l,f,a,al) \
	use_rs ? rsendto(s,b,l,f,a,al)   : sendto(s,b,l,f,a,al)
#define rs_poll(f,n,t)	  use_rs ? rpoll(f,n,t)	   : poll(f,n,t)
#define rs_fcntl(s,c,p)   use_rs ? rfcntl(s,c,p)   : fcntl(s,c,p)
#define rs_setsockopt(s,l,n,v,ol) \
	use_rs ? rsetsockopt(s,l,n,v,ol) : setsockopt(s,l,n,v,ol)
#define rs_getsockopt(s,l,n,v,ol) \
	use_rs ? rgetsockopt(s,l,n,v,ol) : getsockopt(s,l,n,v,ol)

union socket_addr {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};

enum rs_optimization {
	opt_mixed,
	opt_latency,
	opt_bandwidth
};

int get_rdma_addr(const char *src, const char *dst, const char *port,
		  struct rdma_addrinfo *hints, struct rdma_addrinfo **rai);

void size_str(char *str, size_t ssize, long long size);
void cnt_str(char *str, size_t ssize, long long cnt);
int size_to_count(int size);
void format_buf(void *buf, int size);
int verify_buf(void *buf, int size);
int do_poll(struct pollfd *fds, int timeout);
struct rdma_event_channel *create_first_event_channel(void);
