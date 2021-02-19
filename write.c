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
#include "la.h"

#include "sock.h"

#include "common.h"
#include "connection.h"

#include "ev.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


connection sock_watcher;
connection stdin_watcher;

char *topic = NULL;
u8 topic_len = 0;
int done = 0;
int bp = 0;

void sock_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection* conn = (connection*)w;
	if (revents & EV_WRITE) {
		if (connection_onwrite(conn, loop) < 0) {
			exit(1);
		}

		if (connection_empty_send(conn)) {
			connection_disable_write(conn, loop);
		}

		if (bp) {
			ev_feed_event(loop, &stdin_watcher, EV_READ);
			bp = 0;
		}

		if (done && connection_empty_send(conn)) {
			ev_io_stop(loop, w);
			return;
		}

		if (!done) ev_feed_event(loop, &stdin_watcher, EV_READ);

	}
	if (revents & EV_READ) {
		if (connection_onread(conn) < 0) {
			exit(1);
		}
	}
}

static void stdin_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection* conn = (connection*)w;
	if (!(revents & EV_READ)) return;

	if (connection_onread(conn) < 0) {
		done = 1;
		ev_io_stop(loop, w);
		ev_feed_event(loop, &sock_watcher, EV_WRITE);
	}

	static int skip_next = 0;
again:
	for (;;) {
		char *buf;
		u32 len = connection_peek_all(conn, &buf);
		char *end = memchr(buf, '\n', len);
		if (!end) {
			if (len + sizeof(u32) > MAX_MESSAGE_SIZE) {
				skip_next = 1;
				connection_reset(conn);
			}
			break;
		}

		if (skip_next) {
			skip_next = 0;
			fprintf(stderr, "skipping message\n");
			goto skip;
		}

		// send event
		u32 total_len = sizeof(char) + sizeof(u8) + topic_len + (end-buf);

		if (total_len + sizeof(u32) > MAX_MESSAGE_SIZE) {
			fprintf(stderr, "skipping message\n");
			goto skip;
		}

		connection_iovec parts[5];
		parts[0].buf = &total_len;
		parts[0].len = sizeof(u32);
		parts[1].buf = "e";
		parts[1].len = sizeof(char);
		parts[2].buf = &topic_len;
		parts[2].len = sizeof(u8);
		parts[3].buf = topic;
		parts[3].len = topic_len;
		parts[4].buf = buf;
		parts[4].len = end-buf;

		if (connection_send_multi(&sock_watcher, parts, 5)) {
			bp = 1;
			return; // backpressure
		}
		connection_enable_write(&sock_watcher, loop);

skip:
		connection_consume(conn, (end-buf)+1);
	}
}

void usage() {
	fprintf(stderr, "Usage: esq-write [-h host] [-p port] topic\n");
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 2) usage();

	char *host = "127.0.0.1";
	char *port = "4000";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) {
			if (++i >= argc) usage();
			host = argv[i];
		} else if (!strcmp(argv[i], "-p")) {
			if (++i >= argc) usage();
			port = argv[i];
		} else if (!strcmp(argv[i], "--")) {
			break;
		} else {
			if (topic) usage();
			topic = argv[i];
		}
	}

	if (!topic) usage();

	topic_len = strlen(topic);

	unsigned int evflags = ev_recommended_backends() | EVBACKEND_KQUEUE | EVBACKEND_EPOLL;
	struct ev_loop *loop = ev_default_loop (evflags);

	// connect
	int sock = 0;
	struct sockaddr_in serv_addr; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 1;
	if (socket_setnonblock(sock) < 0) return 1;
	if (socket_setnodelay(sock) < 0) return 1;

	connection_init(&sock_watcher, MAX_MESSAGE_SIZE);
	ev_io_init(&sock_watcher.io, sock_cb, sock, EV_READ);
	ev_io_start(loop, &sock_watcher.io);

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(atoi(port)); 
	if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) { 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		if (errno != EINPROGRESS) {
			return -1;
		}
	}

	// stdin
	socket_setnonblock(0);
	connection_init(&stdin_watcher, MAX_MESSAGE_SIZE);
	ev_io_init(&stdin_watcher.io, stdin_cb, 0, EV_READ);
	ev_io_start(loop, &stdin_watcher.io);

	signal(SIGPIPE, SIG_IGN);

	ev_loop(loop, 0);

	return 0;
}

