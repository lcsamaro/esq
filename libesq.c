/* MIT License
 * 
 * Copyright (c) 2021 Lucas Amaro
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "libesq.h"

#include "common.h"
#include "ring.c"
#include "connection.h"
#include "connection.c"
#define EV_API_STATIC 1
#include "ev.h"
#include "ev.c"
#include "sock.c"
#include "sock.h"

#include <stdlib.h>

struct session {
	connection conn;

	char *topic;
	u8 topic_len;
	int delayed;

	struct session *next;
};

int session_init(session *s) {
	s->next = NULL;
	return connection_init(&s->conn, MAX_MESSAGE_SIZE);
}

void session_destroy(session *s) {
	connection_destroy(&s->conn);
}

typedef struct loop_userdata {
	esq *q;
	void *ctx;
	esq_event_cb cb;
} loop_userdata;

static void sock_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);
	connection* conn = (connection*)w;
	session* s = (session*)w;
	if (revents & EV_WRITE) {
		if (connection_onwrite(conn, loop) < 0) {
			// TODO
			return;
		}

		if (connection_empty_send(conn)) {
			connection_disable_write(conn, loop);
		}

	}
	if (!(revents & EV_READ)) return;

	if (connection_onread(conn) < 0) {
		// TODO
		return;
	}

	for (;;) {
		connection_iovec parts[2];
		parts[0].buf = NULL;
		parts[0].len = sizeof(u32);
		parts[1].buf = NULL;
		parts[1].len = 0;
		if (connection_peek_multi(conn, parts, 1)) {
			return;
		}
		memcpy(&parts[1].len, parts[0].buf, sizeof(u32));
		if (connection_peek_multi(conn, parts, 2)) {
			return;
		}

		char *buf = parts[1].buf;
		u32 len = parts[1].len;

		u64 offset;
		memcpy(&offset, parts[1].buf, sizeof(u64));

		char *str = buf + sizeof(u64);
		int str_len = (int)(len - sizeof(u64));

		if (!u->cb(offset, s->topic, s->topic_len, str, str_len, u->ctx)) { // not handled
			session *n = s->next;
			if (!n) n = u->q->next;
			ev_feed_event(loop, (ev_io*)n, EV_READ);
			return;
		}

		connection_consume_multi(conn, parts, 2);
	}
}

int esq_init(esq *q, const char *host, const char *port) {
	unsigned int evflags = ev_recommended_backends() | EVBACKEND_KQUEUE | EVBACKEND_EPOLL;
	q->loop = ev_default_loop(evflags);
	q->next = NULL;
	q->host = host;
	q->port = port;
	return 0;
}

void esq_destroy(esq *q) {
	session *n = q->next;
	while (n) {
		session *next = n->next;
		session_destroy(n);
		free(n);
		n = next;
	}
	ev_default_destroy();
}

int esq_tail(esq *q, const char *topic, u8 topic_len, i64 offset) {
	int fd = socket_connect(q->host, q->port);
	if (fd < 0) return 1;

	session *s = malloc(sizeof(session));
	if (!s) return 1;
	if (session_init(s)) {
		free(s);
		return 1;
	}

	s->topic = topic;
	s->topic_len = topic_len;

	ev_io_init((ev_io*)s, sock_cb, fd, EV_READ);
	ev_io_start(q->loop, (ev_io*)s);

	s->next = q->next;
	q->next = s;

	// send watch request
	u32 total_len = sizeof(char) + sizeof(i64) + topic_len;
	connection_iovec parts[4];
	parts[0].buf = &total_len;
	parts[0].len = sizeof(u32);
	parts[1].buf = "w";
	parts[1].len = sizeof(char);
	parts[2].buf = &offset;
	parts[2].len = sizeof(i64);
	parts[3].buf = topic;
	parts[3].len = topic_len;

	connection_send_multi((connection*)s, parts, 4);
	connection_enable_write((connection*)s, q->loop);

	return 0;
}

int esq_write(esq *q, const char *topic, u8 topic_len, const char *data, u32 data_len) {
	// TODO
	return 0;
}

void esq_loop(esq *q, esq_event_cb cb, void *ctx) {
	loop_userdata u;
	u.q = q;
	u.ctx = ctx;
	u.cb = cb;
	ev_set_userdata(q->loop, &u);
	ev_loop(q->loop, 0);
}

