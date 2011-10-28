/*
 * modelnet  libipaddr.c
 *
 *     An LD_PRELOAD shared library to transparently set source and
 *     destitation IP addresses used by applications.
 *
 * author Kevin Walsh
 * Modified by Ovidiu Mara, 2011
 *
Copyright (c) 2003, Duke University  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

* Neither the name of the University nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

/*
 * Modelnet IP address control
 *
 * This binary allows modelnet apps to transparently
 * set the source and destination IP address.
 *
 * Our goals:
 *  - set the default source ip address (using env var SRCIP)
 *  - force all packets to go out to forwarder (disable by setting KEEP_DSTIP)
 *
 *  - set the local host name (using env var MN_LOCALHOST).  not required.
 *  - emulate a private dns (using env var HOSTS).    not required.
 *
 * We need to allow some traffic to external network services, like yp,
 * dns, etc., or Java, chord, ps, ls -l, and other programs will not work.
 * In these cases, the source ip address must be the real address.
 *
 * It works like this:
 * 1 - Connection-oriented listeners must call bind(). We assume that
 *   these listeners are only listening for modelnet traffic. So
 *   during the bind() we go ahead and assume that the local address
 *   should be 10.x.x.x (tentatively).
 * 2 - Connection-oriented senders might call bind(), but will always
 *   call connect(). The bind has already assumed a 10.x.x.x local
 *   address. If the connect detirmines that the assumption was wrong,
 *   then we try must try to rebind to the regular host address.
 * 3 - Connectionless recievers must call bind() before
 *   calling recv/recvfrom/recvmsg. In this case, we assume that
 *   they are only listening for modelnet traffic, and that the
 *   bind assumption of a 10.x.x.x local address is okay.
 * 4 - Connectionless senders may skip the bind step, and just call
 *   sendto/sendmsg. We check the destination address, and if
 *   the bind() assumption was wrong, we try to fix it here.
 * 5 - Connectionless senders may want to call send/write, in which
 *   case they must do a connect() first. This case is already
 *   covered in the same way as the connection-oriented connect() case.
 *
 * To do all this, we keep around 2 state bits for each socket, saying
 * if the socket is bound to 10.x.x.x (M) or the regular host addr (S) or
 * nothing (U).
 * 1 - bind() always uses 10.x.x.x as the local addr and sets M.
 * 2 - connect() distinguishes between modelnet and external destinations.
 *   If modelnet dest and U, then bind to 10.x.x.x
 *   If modelnet dest and S, then try to unbind and rebind to 10.x.x.x
 *   If external dest and U, then bind to the regular address
 *   If modelnet dest and M, then try to unbind and rebind to the regular addr
 * 3 - recv()/recvfrom()/recvmsg() don't do anything
 * 4 - sendto()/sendmsg() do same thing as connect() did
 * 5 - send() doesn't do anything.
 *
 * When we unbind and rebind, we may not be able to get the same port
 * number again. We just have to live with that possibility.
 */


#define _GNU_SOURCE  /* to enable gcc magic in dlfcn.h for shared libs */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>

/* addresses of original system routines */

/* sockets */

static int (*f_socket)(int, int, int);
static int (*f_socketpair)(int, int, int, int[2]);
static int (*f_bind)(int, const struct sockaddr *, socklen_t);
static int (*f_getsockname)(int, struct sockaddr *, socklen_t *);
static int (*f_connect)(int, const struct sockaddr *, socklen_t);
static int (*f_getpeername)(int, struct sockaddr *, socklen_t *);
static int (*f_send)(int, const void *, size_t, int);
static int (*f_recv)(int, void *, size_t, int);
static ssize_t (*f_sendto)(int, const void *, size_t, int,
						   const struct sockaddr *, socklen_t);
static ssize_t (*f_recvfrom)(int, void *, size_t, int,
							 struct sockaddr *, socklen_t *);
static ssize_t (*f_sendmsg)(int, const struct msghdr *, int);
static ssize_t (*f_recvmsg)(int, struct msghdr *, int);
static int (*f_getsockopt)(int, int, int, void *, socklen_t *);
static int (*f_setsockopt)(int, int, int, const void *, socklen_t);
static int (*f_listen)(int, int);
static int (*f_accept)(int, struct sockaddr *, socklen_t *);
static int (*f_shutdown)(int, int);

/* modelnet variables */

static unsigned long mn_localip;
static int mn_debug;
static int mn_keep_dstip;

/* NOTE: the use of fd_set limits the number of sockets to FD_SETSIZE-1 */
fd_set mn_unbound_sockets;	/* not yet bound */
fd_set mn_modelnet_sockets;	/* bound to 10.x.x.x */
fd_set mn_hasport_sockets;	/* bound to a port (e.g. not "any") */

#define SOCK_UNBOUND(s) (FD_ISSET((s), &mn_unbound_sockets))
#define SOCK_MNET(s)	(FD_ISSET((s), &mn_modelnet_sockets))
#define SOCK_SYS(s)	(!SOCK_UNBOUND(s) && !SOCK_MNET(s))
#define SOCK_HASPORT(s)	(FD_ISSET((s), &mn_hasport_sockets))

/* some helpful routines */

#define MAKE_IP(a,b,c,d) \
	(((a)&0xff)<<24)|(((b)&0xff)<<16)|(((c)&0xff)<<8)|((d)&0xff)
#define IP_ADDR_QUAD_N(x) IP_ADDR_QUAD_H(ntohl(x))
#define IP_ADDR_QUAD_H(x) (unsigned long int)(((x)>>24)&0xff), \
	(unsigned long int)(((x)>>16)&0xff), \
	(unsigned long int)(((x)>>8)&0xff), \
	(unsigned long int)((x)&0xff)
#define S_ADDR_QUAD_PORT(sa) \
	IP_ADDR_QUAD_N((sa)->sin_addr.s_addr), (int)ntohs((sa)->sin_port)

#define IS_INET_ADDR(sock_addr,len) \
	((sock_addr) != NULL && \
	(len) >= sizeof(struct sockaddr_in) && \
	(sock_addr)->sa_family == AF_INET)

#define IS_MNET_IP(ip) (((ip) & htonl(0xff000000)) == htonl(MAKE_IP(10,0,0,0)))

#define IS_MNET_ADDR(sin) (IS_MNET_IP((sin)->sin_addr.s_addr))

#define MNET_MAGIC_BIT (htonl(0x00800000))

/* we can't use inet_addr/inet_aton because they are part of sockets */
static unsigned long dots_to_ip(const char *s)
{
	unsigned int a,b,c,d;
	char *rest = NULL;

	if (!s) return 0;

	a = strtoul(s, &rest, 10);
	if (rest == s || rest[0] != '.') return 0;
	b = strtoul(s = rest+1, &rest, 10);
	if (rest == s || !rest || rest[0] != '.') return 0;
	c = strtoul(s = rest+1, &rest, 10);
	if (rest == s || rest[0] != '.') return 0;
	d = strtoul(s = rest+1, &rest, 10);
	if (rest == s) return 0;
	return ((a&0xff)<<24)|((b&0xff)<<16)|((c&0xff)<<8)|(d&0xff);
}

/* initialize library */
static void __attribute__ ((constructor)) init(void)
{
	const char *s;

	s = getenv("MN_DEBUG");
	if (s) mn_debug = atoi(s);

	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: libipaddr\n"
							   "libipaddr: compiled on: %s, %s\n", __DATE__, __TIME__);
	}

	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: loading system socket routines\n");
	}

#define GET_SYM(sym)                                        \
	f_##sym = dlsym(RTLD_NEXT, #sym);                       \
	if (f_##sym == NULL) {                                  \
	if (mn_debug > 0) { fprintf(stderr, "libipaddr:"        \
	"dlsym(%s) failed: %s\n", #sym, dlerror()); };      \
}

	GET_SYM(socket);
	GET_SYM(socketpair);
	GET_SYM(bind);
	GET_SYM(getsockname);
	GET_SYM(connect);
	GET_SYM(getpeername);
	GET_SYM(send);
	GET_SYM(recv);
	GET_SYM(sendto);
	GET_SYM(recvfrom);
	GET_SYM(sendmsg);
	GET_SYM(recvmsg);
	GET_SYM(getsockopt);
	GET_SYM(setsockopt);
	GET_SYM(listen);
	GET_SYM(accept);
	GET_SYM(shutdown);

#undef GET_SYM

	/* get the modelnet environment variables */

	mn_localip = htonl(dots_to_ip(getenv("SRCIP")));
	mn_keep_dstip = getenv("KEEP_DSTIP") ? 1 : 0;

	if (mn_debug > 0) {
		if (mn_localip)
			fprintf(stderr, "libipaddr: using local ip: %lu.%lu.%lu.%lu\n", IP_ADDR_QUAD_N(mn_localip));
		if (mn_keep_dstip) {
			fprintf(stderr, "libipaddr: keep dst IP as set by app. Do not force to forwarder\n");
		} else {
			fprintf(stderr, "libipaddr: force packets to forwarder (default)\n");
		}
	};

	/* init rest of library */

	FD_ZERO(&mn_unbound_sockets);
	FD_ZERO(&mn_modelnet_sockets);
	FD_ZERO(&mn_hasport_sockets);

	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: exit init\n");
	}
}


/* helper to do implicit binding of source address  */
static int bind_to(int s, unsigned long addr, int port_n)
{
	struct sockaddr_in sa;
	int ret;

	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: bind_to(%d, %lu.%lu.%lu.%lu, %d)\n", s, IP_ADDR_QUAD_N(addr), ntohs(port_n));
	}

	/* memset(sa.sin_zero, 0, sizeof (sa.sin_zero)); */
	sa.sin_family = AF_INET;
	sa.sin_port = port_n;
	sa.sin_addr.s_addr = addr;
	ret = f_bind(s, (struct sockaddr *)&sa, sizeof(sa));
	if (ret == 0) {
		/* good, update state for socket */
		FD_CLR(s, &mn_unbound_sockets);
		if (IS_MNET_IP(addr)) {
			FD_SET(s, &mn_modelnet_sockets);
		} else {
			FD_CLR(s, &mn_modelnet_sockets);
		}
		if (port_n != 0) {
			FD_SET(s, &mn_hasport_sockets);
		} else {
			FD_CLR(s, &mn_hasport_sockets);
		}
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: bind_to: socket %d bound to %s, %s port\n",
									s, IS_MNET_IP(addr)?"10.*":"external",
									port_n?"chosen":"any");
		}
	} else {
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: bind_to(%d) failed\n", s);
		}
	}
	return ret;
}

/* helper to unbind and rebind a socket */
static int rebind_to(int s, unsigned long addr)
{
	struct sockaddr_in old_sa;
	int s_new, sock_type;
	socklen_t len;
	int err;

	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: rebind_to(%d, %lu.%lu.%lu.%lu)\n", s, IP_ADDR_QUAD_N(addr));
	};

	len = sizeof(old_sa);
	if (f_getsockname(s, (struct sockaddr *)&old_sa, &len)) {
		fprintf(stderr, "libipaddr: rebind_to(%d) failed: getsockname()\n", s);
		return -1;
	}
	len = sizeof(sock_type);
	if (f_getsockopt(s, SOL_SOCKET, SO_TYPE, &sock_type, &len)) {
		fprintf(stderr, "libipaddr: rebind_to(%d) failed: getsockopt()\n", s);
		return -1;
	}
	s_new = f_socket(PF_INET, sock_type, 0);
	if (s_new < 0) {
		fprintf(stderr, "libipaddr: rebind_to(%d) failed: socket()\n", s);
		return -1;
	}
	if (dup2(s_new, s) == -1) {
		fprintf(stderr, "libipaddr: rebind_to(%d) failed: dup2(%d, %d)\n",s,s_new,s);
		close(s_new);
		return -1;
	}
	close(s_new);
	err = bind_to(s, addr, old_sa.sin_port);
	if (err && !SOCK_HASPORT(s)) {
		/* socket was originally bound to port 0 ("any") */
		/* but we can't get same port number... maybe the app doesn't care */
		fprintf(stderr,
				"libipaddr: rebind_to(%d) trouble: Application originally requested\n"
				"    a socket on any port, and was assigned port %d as a\n"
				"    \"10.*\" socket. It is now clear that a \"system\"\n"
				"    socket is needed instead, but the same port is no\n"
				"    longer available. Requesting any available port, and\n"
				"    hoping the application does not care...\n", s,
				ntohs(old_sa.sin_port));
		return bind_to(s, addr, 0);
	}
	return err;
}

static __inline
int rebind_if_needed(int s, unsigned long dest)
{
	if (IS_MNET_IP(dest)) {
		if (SOCK_UNBOUND(s)) {
			return bind_to(s, mn_localip, 0);
		} else if (SOCK_SYS(s)) {
			return rebind_to(s, mn_localip);
		} else {
			return 0; /* not needed */
		}
	} else {
		if (SOCK_UNBOUND(s)) {
			return bind_to(s, INADDR_ANY, 0);
		} else if (SOCK_MNET(s)) {
			return rebind_to(s, INADDR_ANY);
		} else {
			return 0; /* not needed */
		}
	}
}

int socket(int domain, int type, int protocol)
{
	int s;
	s = f_socket(domain, type, protocol);
	/* very strange: we do not seem to intercept some socket() calls. ugh. */
	if (mn_debug > 0) {
		fprintf(stderr, "libipaddr: socket(%s, %s, %s) = %d\n",
								(domain == PF_INET)?"PF_INET":"PF_??",
								(type == SOCK_STREAM)?"SOCK_STREAM":"SOCK_DGRAM",
								(protocol == IPPROTO_TCP)?"IPPROTO_TCP":
							  (protocol == IPPROTO_UDP)?"IPPROTO_UDP":"IPPROTO_ANY", s);
	}
	if (s >= 0 && domain == PF_INET) {
		assert(s < FD_SETSIZE); /* limitation of library state */
		FD_SET(s, &mn_unbound_sockets);
		FD_CLR(s, &mn_modelnet_sockets);
		FD_CLR(s, &mn_hasport_sockets);
	}
	return s;
}

/* only used for AF_UNIX sockets */
/* int socketpair(int d, int type, int protocol, int sv[2]) */

int bind(int s, const struct sockaddr *my_addr, socklen_t addrlen)
{
	struct sockaddr_in *sa;

	if (IS_INET_ADDR(my_addr, addrlen)) {
		sa = (struct sockaddr_in *)my_addr;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: bind(%d, %lu.%lu.%lu.%lu, %d)\n", s, S_ADDR_QUAD_PORT(sa));
		}
		if (mn_localip) {
			return bind_to(s, mn_localip, sa->sin_port);
		}
	}
	return f_bind(s, my_addr, addrlen);
}

int connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	struct sockaddr_in *sa;
	struct sockaddr_in sa_magic;

	if (IS_INET_ADDR(serv_addr, addrlen)) {
		sa = (struct sockaddr_in *)serv_addr;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: connect(%d, %lu.%lu.%lu.%lu:%d)\n", s, S_ADDR_QUAD_PORT(sa));
		}

		rebind_if_needed(s, sa->sin_addr.s_addr);

		if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
			if (sa->sin_addr.s_addr != mn_localip) {
				/* we should not modify the application memory space */
				sa_magic = *sa;
				sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
				if (mn_debug > 0) {
					fprintf(stderr, "libipaddr: changed dst to %lu.%lu.%lu.%lu:%d\n", S_ADDR_QUAD_PORT(&sa_magic)); }
				serv_addr = (struct sockaddr *)&sa_magic;
				addrlen = sizeof(sa_magic);
			}
		}
	}
	return f_connect(s, serv_addr, addrlen);
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
	int ret;

	if (mn_debug > 0) { fprintf(stderr, "libipaddr: getsockname(%d, ...)\n", s); }

	ret = f_getsockname(s, name, namelen);
	return ret;
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen)
{
	struct sockaddr_in *sa;
	int ret;

	ret = f_getpeername(s, name, namelen);
	if (IS_INET_ADDR(name, *namelen)) {
		sa = (struct sockaddr_in *)name;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: getpeername(%d, %lu.%lu.%lu.%lu:%d)\n", s, S_ADDR_QUAD_PORT(sa));
		}
		if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
			if (mn_debug > 0) {
				fprintf(stderr, "libipaddr: getpeername(%d) unset magic bit\n", s);
			}
			sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
		}
	}
	return ret;
}

/* sendto() -- this COULD actually be TCP, which is supposed to
 * disconnect, connect, send, ack, disconnect, and reconnect. That
 * is pure crazyness. We just assume that this is UDP, in which case
 * we need to bind if it is unbound. */
ssize_t sendto(int s, const void *msg, size_t len, int flags,
			   const struct sockaddr *to, socklen_t tolen)
{
	struct sockaddr_in *sa;
	struct sockaddr_in sa_magic;

	if (IS_INET_ADDR(to, tolen)) {
		sa = (struct sockaddr_in *)to;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: sendto(%d, %lu.%lu.%lu.%lu:%d, ...)\n", s, S_ADDR_QUAD_PORT(sa));
		}

		rebind_if_needed(s, sa->sin_addr.s_addr);

		if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
			if (sa->sin_addr.s_addr != mn_localip) {
				/* we should not modify the application memory space */
				sa_magic = *sa;
				sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
				if (mn_debug > 0) { fprintf(stderr, "libipaddr: changed dst to %lu.%lu.%lu.%lu:%d\n", S_ADDR_QUAD_PORT(&sa_magic)); }
				to = (struct sockaddr *)&sa_magic;
				tolen = sizeof(sa_magic);
			}
		}
	}
	return f_sendto(s, msg, len, flags, to, tolen);
}

ssize_t recvfrom(int s, void *buf, size_t len, int flags,
				 struct sockaddr *from, socklen_t *fromlen)
{
	struct sockaddr_in *sa;
	ssize_t ret;

	ret = f_recvfrom(s, buf, len, flags, from, fromlen);
	if (IS_INET_ADDR(from, *fromlen)) {
		sa = (struct sockaddr_in *)from;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: recvfrom(%d, ..., %lu.%lu.%lu.%lu:%d)\n", s, S_ADDR_QUAD_PORT(sa));
		}
		if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
			if (mn_debug > 0) {
				fprintf(stderr, "libipaddr: recvfrom(%d) unset magic bit\n", s);
			}
			sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
		}
	} else {
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: recvfrom with NULL address (maybe raw socket?)\n");
		}
	}
	return ret;
}

/* sendmsg() -- same deal as sendto() */
ssize_t sendmsg(int s, const struct msghdr *msg, int flags)
{
	struct sockaddr_in *sa;
	struct msghdr msg_magic;
	struct sockaddr_in sa_magic;

	if (IS_INET_ADDR((struct sockaddr *)msg->msg_name, msg->msg_namelen)) {
		sa = (struct sockaddr_in *)msg->msg_name;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: sendmsg(%d, %lu.%lu.%lu.%lu:%d, ...)\n", s, S_ADDR_QUAD_PORT(sa));
		}

		rebind_if_needed(s, sa->sin_addr.s_addr);

		if (!mn_keep_dstip && IS_MNET_ADDR(sa) && mn_localip) {
			if (sa->sin_addr.s_addr != mn_localip) {
				/* we should not modify the application memory space */
				msg_magic = *msg;
				sa_magic = *sa;
				sa_magic.sin_addr.s_addr |= MNET_MAGIC_BIT;
				if (mn_debug > 0) {
					fprintf(stderr, "libipaddr: changed dst to %lu.%lu.%lu.%lu:%d\n", S_ADDR_QUAD_PORT(&sa_magic));
				}
				msg_magic.msg_name = (struct sockaddr *)&sa_magic;
				msg_magic.msg_namelen = sizeof(sa_magic);
				msg = &msg_magic;
			}
		}
	}
	return f_sendmsg(s, msg, flags);
}

ssize_t recvmsg(int s, struct msghdr *msg, int flags)
{
	struct sockaddr_in *sa;
	ssize_t ret;

	ret = f_recvmsg(s, msg, flags);
	if (IS_INET_ADDR((struct sockaddr *)msg->msg_name, msg->msg_namelen)) {
		sa = (struct sockaddr_in *)msg->msg_name;
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: recvmsg(%d, ..., %lu.%lu.%lu.%lu:%d)\n", s, S_ADDR_QUAD_PORT(sa));
		}
		if (!mn_keep_dstip && IS_MNET_ADDR(sa)) {
			if (mn_debug > 0) {
				fprintf(stderr, "libipaddr: recvmsg(%d) unset magic bit\n", s);
			}
			sa->sin_addr.s_addr &= ~MNET_MAGIC_BIT;
		}
	} else {
		if (mn_debug > 0) {
			fprintf(stderr, "libipaddr: recvmsg (maybe raw socket?)\n");
		}
	}
	return ret;
}
