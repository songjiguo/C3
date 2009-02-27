#include <string.h>
#include <stdlib.h>

#include <stdio.h>

inline static int is_whitespace(char s)
{
	if (' ' == s) return 1;
	return 0;
}

inline static char *remove_whitespace(char *s, int len)
{
	while (is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static char *find_whitespace(char *s, int len)
{
	while (!is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static int memcmp_fail_loc(char *s, char *c, int s_len,
				  int c_len, char **fail_loc)
{
	int min = (c_len < s_len) ? c_len : s_len;

	if (memcmp(s, c, min)) {
		*fail_loc = s;
		return 1;
	}
	*fail_loc = s + min;
	if (c_len > s_len) return 1;
	return 0;
}

static inline char *http_end_line(char *s, int len)
{
	if (len < 2) return NULL;
	if ('\r' == s[0] && '\n' == s[1]) return s+2;
	return NULL;
}

inline static int get_http_version(char *s, int len, char **ret, int *minor_version)
{
	char *c;
	char http_str[] = "HTTP/1.";
	int http_sz = sizeof(http_str)-1; // -1 for \n

	c = remove_whitespace(s, len);
	if (memcmp_fail_loc(c, http_str, len, http_sz, ret) ||
	    0 == (len - (*ret-s))) {
		*minor_version = -1;
		return 1;
	}
	c += http_sz;
	*minor_version = ((int)*c) - (int)'0';
	c++;
	if (http_end_line(c, len - (c-s))) c += 2;
	*ret = c;
	return 0;
}

static int http_find_next_line(char *s, int len, char **ret)
{
	char *r, *e;

	for (e = s;
	     len >= 2 && NULL == (r = http_end_line(e, len));
	     e++, len--);

	if (len >= 2) {
		*ret = r;
		return 0;
	} else {
		//assert(len == 1);
		*ret = e+len;
		return 1;
	}
}

static int http_find_header_end(char *s, int len, char **curr)
{
	char *p, *c;

	c = s;
	do {
		p = c;
		if (http_find_next_line(c, len-(int)(c-s), &c)) {
			/* Malformed: last characters not cr lf */
			*curr = c;
			return 1;
		}
	} while((int)(c - p) > 2);

	*curr = c;
	return 0;
}

typedef struct {
	char *end;
	char *path;
	int path_len;
} get_ret_t;

#define HERE(s) //printf(s "\n")
static int http_get_parse(char *req, int len, get_ret_t *ret)
{
	char *s = req, *e, *end;
	char *path;
	int path_len, minor_version;

	end = req + len;

	if (memcmp_fail_loc(s, "GET ", len, 4, &e)) {HERE("a"); goto malformed_request;}

	path = remove_whitespace(e, len);
	e = find_whitespace(path, len - (path-s));
	path_len = e-path;
	e = remove_whitespace(e, len - (e-s));

	if (get_http_version(e, len - (e-s), &e, &minor_version)) {HERE("b"); goto malformed_request;}
	if (minor_version != 0 && minor_version != 1) {
		/* This should be seen as an error: */
		e = s;
		HERE("c");
		goto malformed_request;
	}

	if (http_find_header_end(e, len - (e-s), &e)) {HERE("d"); goto malformed_request;}

	ret->end = e;
	ret->path = path;
	ret->path_len = path_len;
	return 0;

malformed_request:
//	printf("++parse error (len %d) for ++\n%s\n++ @ ++\n%s\n", len, s, e);
	ret->path = NULL;
	ret->path_len = -1;
	ret->end = e;
	return -1;
}

static const char success_head[] =
	"HTTP/1.1 200 OK\r\n"
	"Date: Sat, 14 Feb 2008 14:59:00 GMT\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: ";
static const char resp[] = "all your base are belong to us\r\n";

#define MAX_SUPPORTED_DIGITS 20

/* Must prefix data by "content_length\r\n\r\n" */
static char *http_get_request(char *path, int path_len, int *resp_len)
{
	int resp_sz = sizeof(resp)-1, head_sz = sizeof(success_head)-1;
	int tot_sz, len_sz = 0;
	char *r, *curr;
	char len_str[MAX_SUPPORTED_DIGITS];

	len_sz = snprintf(len_str, MAX_SUPPORTED_DIGITS, "%d\r\n\r\n", resp_sz);
	if (MAX_SUPPORTED_DIGITS == len_sz || len_sz < 1) {
		printf("length of response body too large");
		*resp_len = 0;
		return NULL;
	}

	tot_sz = head_sz + len_sz + resp_sz;
	/* +1 for \0 so we can print the string */
	curr = r = malloc(tot_sz + 1);
	memcpy(curr, success_head, head_sz);
	curr += head_sz;
	memcpy(curr, len_str, len_sz);
	curr += len_sz;
	memcpy(curr, resp, resp_sz);
	*resp_len = tot_sz;
	r[tot_sz] = '\0';

	return r;
}

struct http_response {
	char *resp;
	int resp_len;
};

struct http_request;
struct connection {
	int refcnt;
	long conn_id;
	struct http_request *pending_reqs;
};

/*
 * The queue of requests should look like so:
 *
 * c
 * |
 * V
 * p<->p<->p<->.<->.<->.<->P
 *
 * c: connection, P: request with pending data, .: requests that have
 * not been processed (requests made to content containers), p:
 * requests that have been processed (sent to content managers), but
 * where the reply has not been transferred yet.  In most cases, P
 * will also have the malloc flag set.
 */
#define HTTP_REQ_PENDING      0x1 /* incomplete request, pending more
				   * data */
#define HTTP_REQ_MALLOC       0x2 /* the buffer has been malloced */
#define HTTP_REQ_PROCESSED    0x4 /* request not yet made */

enum {HTTP_TYPE_TOP, 
      HTTP_TYPE_GET};

struct http_request {
	long id;
	int flags, type;
	struct connection *c;
	struct http_response resp;

	char *req, *path;
	int req_len, path_len;

	struct http_request *next, *prev;
};

static int http_get(struct http_request *r)
{
	get_ret_t ret;
	char *s = r->req, *p;
	int len = r->req_len;

	if (http_get_parse(s, len, &ret)) {
		r->req_len = (int)(ret.end - s);
		return -1;
	}
	p = malloc(ret.path_len+1);
	if (!p) {
		printf("path could not be allocated\n");
		r->req_len = 0;
		return -1;
	}
	memcpy(p, ret.path, ret.path_len);
	p[ret.path_len] = '\0';

	r->path = p;
	r->path_len = ret.path_len;
	r->req_len = (int)(ret.end - s);
	return 0;
}

static int http_parse_request(struct http_request *r)
{
	if (!memcmp("GET", r->req, 3)) {
		r->type = HTTP_TYPE_GET;
		return http_get(r);
	}
	printf("unknown request type for message\n<<%s>>\n", r->req);
	return -1;
}

static int http_make_request(struct http_request *r)
{
	struct http_response *hr;

	hr = &r->resp;
	switch (r->type) {
	case HTTP_TYPE_GET:
		hr->resp = http_get_request(r->path, r->path_len, &hr->resp_len);
		break;
	default:
		printf("unknown request type\n");
		return -1;
	}

	return 0;
}

static struct connection *http_new_connection(int conn_id)
{
	struct connection *c = malloc(sizeof(struct connection));

	if (NULL == c) return c;
	c->conn_id = conn_id;
	c->pending_reqs = NULL;
	c->refcnt = 1;

	return c;
}

static void http_free_request(struct http_request *r);

static inline void conn_refcnt_dec(struct connection *c)
{
	c->refcnt--;
	if (0 == c->refcnt) {
		struct http_request *r, *first, *next;

		if (NULL != c->pending_reqs) {
			first = r = c->pending_reqs;
			do {
				next = r->next;
				/* FIXME: don't do this if the request
				 * has been made external to this
				 * component. */
				if (!(r->flags & HTTP_REQ_PROCESSED)) {
					http_free_request(r);
				}
				
				r = next;
			} while (first != r);
		}
		free(c);
	}
}

static void http_free_connection(struct connection *c)
{
	conn_refcnt_dec(c);
}

static inline void http_init_request(struct http_request *r, int buff_len, struct connection *c, char *req)
{
	static long id = 0;

	memset(r, 0, sizeof(struct http_request));
	r->id = id++;
	r->type = HTTP_TYPE_TOP;
	r->c = c;
	c->refcnt++;
	r->req = req;
	r->req_len = buff_len;
	if (c->pending_reqs) {
		struct http_request *head = c->pending_reqs, *tail = head->prev;

		/* Add the new request to the end of the queue */
		r->next = head;
		r->prev = tail;
		tail->next = r;
		head->prev = r;
	} else {
		r->next = r->prev = r;
		c->pending_reqs = r;
	}
}

static struct http_request *http_new_request_flags(struct connection *c, char *req, int buff_len, int flags)
{
	struct http_request *r = malloc(sizeof(struct http_request));

	if (NULL == r) return r;
	http_init_request(r, buff_len, c, req);
	r->flags = flags;

	return r;
}

static struct http_request *http_new_request(struct connection *c, char *req, int buff_len)
{
	return http_new_request_flags(c, req, buff_len, 0);
}

static void __http_free_request(struct http_request *r)
{
	/* FIXME: don't free response if in arg. reg. */
	if (r->resp.resp) free(r->resp.resp);
	if (r->path) free(r->path);
	//assert(r->req);
	if (r->flags & HTTP_REQ_MALLOC) free(r->req);
	free(r);
}

static void http_free_request(struct http_request *r)
{
	struct connection *c = r->c;
	struct http_request *next = r->next, *prev = r->prev;

	//assert(c->pending_reqs);
	if (r->next != r) {
		next->prev = r->prev;
		prev->next = r->next;
	} //else assert(r->prev == r && c->pending_reqs == r);
	r->next = r->prev = NULL;
	//assert(c->pending_reqs == r)
	if (c->pending_reqs == r) {
		c->pending_reqs = (r == next) ? NULL : next;
	}

	conn_refcnt_dec(c);
	__http_free_request(r);
}

/* 
 * This is annoying, but the caller needs to know the buffer size of
 * the request that we are using.  In most cases, the passed in buffer
 * is what we use, so it isn't an issue, but if we malloc our own
 * buffer (see the HTTP_REQ_PENDING case), then the total buffer size
 * can grow.  Thus the buff_sz argument.
 */
static struct http_request *connection_handle_request(struct connection *c, char *buff,
						      int amnt, int *buff_sz)
{
	struct http_request *r, *last;

	*buff_sz = amnt;
	r = http_new_request(c, buff, amnt);
	if (NULL == r) return NULL;
	last = r->prev;

	/* 
	 * If a previous request required more data to parse correctly
	 * (was pending), then we need to combine its buffer with the
	 * current one and try to parse again.
	 */
	if (last != r && last->flags & HTTP_REQ_PENDING) {
		char *new_buff;
		int new_len;

		if (last->prev != last && last->prev->flags & HTTP_REQ_PENDING) {
			http_free_request(r);
			return NULL;
		}
		new_len = amnt + last->req_len;
		new_buff = malloc(new_len + 1);
		if (NULL == new_buff) {
			printf("malloc fail 1\n"); fflush(stdout);
			http_free_request(r);
			return NULL;
		}
		memcpy(new_buff, last->req, last->req_len);
		memcpy(new_buff + last->req_len, buff, amnt);
		buff = new_buff;
		*buff_sz = amnt = new_len;
		new_buff[new_len] = '\0';
		http_free_request(last);

		if (r->flags & HTTP_REQ_MALLOC) free(r->req);
		r->req_len = new_len;
		r->req = new_buff;
		r->flags |= HTTP_REQ_MALLOC;
	}

	/*
	 * Process the list of requests first, then go through and
	 * actually make the requests.
	 */
	if (http_parse_request(r)) {
		char *save_buff;

		/* parse error?! Parsing broke somewhere _in_ the
		 * message, so we need to report this as an error */
		if (r->req_len != amnt) {
			/* FIXME: kill connection */
			http_free_request(r);
			return NULL;
		}		
		//assert(r->req_len == amnt && r->req == buff);
		/*
		 * If we failed because we simply don't have a whole
		 * message (more data is pending), then store it away
		 * appropriately.
		 */
		save_buff = malloc(amnt + 1);
		if (NULL == save_buff) {
			printf("malloc fail 2\n"); fflush(stdout);
			/* FIXME: kill connection */
			http_free_request(r);
			return NULL;
		}
		memcpy(save_buff, buff, amnt);
		save_buff[amnt] = '\0';
		if (r->flags & HTTP_REQ_MALLOC) free(r->req);
		r->req = save_buff;
		r->flags |= (HTTP_REQ_PENDING | HTTP_REQ_MALLOC);
	}
	return r;
}

int http_write(long connection_id, char *reqs, int sz)
{
	return 0;
}

int http_read(long connection_id, char *buff, int sz)
{
	return 0;
}

int http_read_write(long connection_id, char *reqs, int req_sz, char *resp, int resp_sz)
{
	return 0;
}

long http_open_connection(long evt_id)
{
	return 0;
}

int http_close_connection(long conn_id)
{
	return 0;
}

#define LINUX_ENV
#ifdef LINUX_ENV

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>


#define BUFF_SZ 128//1024

static int connection_event(struct connection *c)
{
	int amnt;
	char buff[BUFF_SZ+1], *curr;
	struct http_request *first, *r;

	amnt = read(c->conn_id, buff, BUFF_SZ);
	if (0 == amnt) return 1;
	if (amnt < 0) {
		printf("read from fd %ld, ret %d\n", c->conn_id, amnt);
		perror("read from session");
		return -1;
	}
	buff[amnt] = '\0';
	curr = buff;

	while (amnt > 0) {
		int start_amnt;

		r = connection_handle_request(c, curr, amnt, &start_amnt);
		if (NULL == r) return 1;
		curr = r->req + r->req_len;
		amnt = start_amnt - r->req_len;
	}

	//assert(c->pending_reqs);
	first =	r = c->pending_reqs;
	do {
		if (!(r->flags & (HTTP_REQ_PENDING | HTTP_REQ_PROCESSED))) {
			if (http_make_request(r)) {
				printf("Could not process response.\n");
				return -1;
			}
			r->flags |= HTTP_REQ_PROCESSED;
		}
		r = r->next;
	} while (r != first);

	first = r = c->pending_reqs;
	while (c->pending_reqs != NULL) {
		struct http_request *next;

		if (!(r->flags & HTTP_REQ_PROCESSED)) break;
		if (write(c->conn_id, r->resp.resp, r->resp.resp_len) !=
		    r->resp.resp_len) {
			http_free_request(r);
			perror("writing");
			return -1;
		}
		next = r->next;
		http_free_request(r);
		r = next;
	}
	//http_free_request(r);

	return 0;
}

int connmgr_create_server(short int port)
{
	int fd;
	struct sockaddr_in server;

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	server.sin_family      = AF_INET;
	server.sin_port        = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&server, sizeof(server))) {
		perror("binding receive socket");
		return -1;
	}
	listen(fd, 10);

	return fd;
}

struct connection *connmgr_accept(int fd)
{
	struct sockaddr_in sai;
	int new_fd;
	unsigned int len = sizeof(sai);

	new_fd = accept(fd, (struct sockaddr *)&sai, &len);
	if (-1 == new_fd) {
		perror("accept");
		return NULL;
	}
	return http_new_connection(new_fd);
}

#define MAX_CONNECTIONS 100
struct epoll_event evts[MAX_CONNECTIONS];

static void event_new(int evt_fd, struct connection *c)
{
	struct epoll_event e;

	e.events = EPOLLIN;//|EPOLLOUT;
	e.data.ptr = c;
	if (epoll_ctl(evt_fd, EPOLL_CTL_ADD, c->conn_id, &e)) {
		perror("epoll create event");
		exit(-1);
	}

	return;
}

static void event_delete(int evt_fd, struct connection *c)
{
	if (epoll_ctl(evt_fd, EPOLL_CTL_DEL, c->conn_id, NULL)) {
		perror("epoll delete event");
		exit(-1);
	}
}

#include <signal.h>
void prep_signals(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL)) {
		perror("sigaction");
		exit(-1);
	}
}

int ut_num = 0;

enum {UT_SUCCESS, UT_FAIL, UT_PENDING};
static int print_ut(char *s, int len, get_ret_t *ret, int type, int print_success)
{
	int r;

	switch(type) {
	case UT_SUCCESS:
		r = http_get_parse(s, len, ret);
		break;
	case UT_FAIL:
		r = !http_get_parse(s, len, ret);
		break;
	case UT_PENDING:
		r = (!http_get_parse(s, len, ret) && ret->end != s+len);
		break;
	}

	if (r) {
		printf("******************\nUnit test %d failed:\n"
		       "String (@%p):\n%s\nResult(@%p):\n<%s>\n******************\n\n",
		       ut_num, s, s, ret->end, ret->end);
		ut_num++;
		return 1;
	} else if (print_success) {
		printf("Unit test %d successful(s@%p-%p, e@%p).\n", ut_num, s, s+len, ret->end);
	}
	ut_num++;
	return 0;
}

static int unittest_http_parse(int print_success)
{
	int success = 1, i;
	char *ut_successes[] = {
		"GET / HTTP/1.1\r\nUser-Agent: httperf/0.9.0\r\nHost: localhost\r\n\r\n",
		NULL
	};
	char *ut_pend[] = {
		"G",		/* 1 */
		"GET",
		"GET ",
		"GET /",
		"GET / ",	/* 5 */
		"GET / HT",
		"GET / HTTP/1.",
		"GET / HTTP/1.1",
		"GET / HTTP/1.1\r",
		"GET / HTTP/1.1\r\n", /* 10 */
		"GET / HTTP/1.1\r\nUser-",
		"GET / HTTP/1.1\r\nUser-blah:blah\r",
		NULL
	};
	char *ut_fail[] = {
		"GET / HTTP/1.2",
		"GET / HTTP/1.2\r\nUser-blah:blah\r", /* 14 */
		NULL
	};
	get_ret_t ret;

	for (i = 0 ; ut_successes[i] ; i++) {
		if (print_ut(ut_successes[i], strlen(ut_successes[i]), &ret, UT_SUCCESS, print_success)) {
			success = 0;
		}
	}
	for (i = 0 ; ut_pend[i] ; i++) {
		if (print_ut(ut_pend[i], strlen(ut_pend[i]), &ret, UT_PENDING, print_success)) {
			success = 0;
		}
	}
	for (i = 0 ; ut_fail[i] ; i++) {
		if (print_ut(ut_fail[i], strlen(ut_fail[i]), &ret, UT_FAIL, print_success)) {
			success = 0;
		}
	}

	if (success) return 0;
	return 1;
}

int main(void)
{
	int sfd, epfd;
	struct connection main_c;
	struct epoll_event new_evts[MAX_CONNECTIONS];

	if (unittest_http_parse(0)) return -1;

	prep_signals();

	epfd = epoll_create(MAX_CONNECTIONS);
	sfd = connmgr_create_server(8000);
	main_c.conn_id = sfd;
	event_new(epfd, &main_c);

	while (1) {
		int nevts, i;

		nevts = epoll_wait(epfd, new_evts, MAX_CONNECTIONS, -1);
		if (nevts < 0) {
			perror("waiting for events");
			return -1;
		}
		for (i = 0 ; i < nevts ; i++) {
			struct epoll_event *e = &new_evts[i];
			struct connection *c = (struct connection *)e->data.ptr;

			if (c == &main_c) {
				if (e->events & (EPOLLERR | EPOLLHUP)) {
					printf("errors on the listen fd\n");
					return -1;
				}
				c = connmgr_accept(main_c.conn_id);
				event_new(epfd, c);
			} else if (e->events & (EPOLLERR | EPOLLHUP)) {
				event_delete(epfd, c);
				close(c->conn_id);
				http_free_connection(c);
				/* FIXME: free requests for connection */
			} else {
				int ret;

				ret = connection_event(c);
				if (ret > 0) {
					event_delete(epfd, c);
					close(c->conn_id);
					http_free_connection(c);
				} else if (ret < 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

#endif