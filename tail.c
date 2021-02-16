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

#include "ev.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"

#define MAX_MESSAGE_SIZE 16384

connection sock_watcher;
connection stdout_watcher;

void sock_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	connection* conn = (connection*)w;
	if (revents & EV_WRITE) {
		connection_onwrite(conn, loop);
	}
	if (!(revents & EV_READ)) return;

	if (connection_onread(conn) < 0) {
		// TODO
		exit(0);
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

		// write stdout
		connection_iovec wparts[2];
		wparts[0].buf = str;
		wparts[0].len = str_len;
		wparts[1].buf = "\n";
		wparts[1].len = sizeof(char);

		if (connection_send_multi(&stdout_watcher, wparts, 2)) {
			return; // backpressure
		}
		connection_make_writable(&stdout_watcher, loop);

		connection_consume_multi(conn, parts, 2);
	}
}

static void stdout_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	if (revents & EV_WRITE) {
		connection_onwrite((connection*)w, loop);
	}
}

void usage() {
	fprintf(stderr, "Usage: esq-tail [-h host] [-p port] [-n number] topic\n");
	exit(1);
}

int main(int argc, char **argv) {
	if (argc < 2) usage();

	char *host = "127.0.0.1";
	char *port = "4000";
	char *topic = NULL;
	char *off = "0";
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) {
			if (++i >= argc) usage();
			host = argv[i];
		} else if (!strcmp(argv[i], "-p")) {
			if (++i >= argc) usage();
			port = argv[i];
		} else if (!strcmp(argv[i], "-n")) {
			if (++i >= argc) usage();
			off = argv[i];
		} else if (!strcmp(argv[i], "--")) {
			break;
		} else {
			if (topic) usage();
			topic = argv[i];
		}
	}

	if (!topic) usage();

	int begin = 0;
	if (*off == '+') {
		begin = 1;
		off++;
	}
	char *endptr;
	errno = 0;
	i64 offset = strtoll(off, &endptr, 10);
	if ((errno == ERANGE && (offset == LLONG_MAX || offset == LLONG_MIN))
			|| (errno != 0 && offset == 0)) {
		usage();
	}
	if (begin) offset++;
	else if (offset > 0) offset = -offset;


	unsigned int evflags = ev_recommended_backends() | EVBACKEND_KQUEUE | EVBACKEND_EPOLL;
	struct ev_loop *loop = ev_default_loop (evflags);

	// connect
	int sock = 0;
	struct sockaddr_in serv_addr; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 1;
	if (socket_setnonblock(sock) < 0) return 1;
	if (socket_setnodelay(sock) < 0) return 1;

	connection_init(&sock_watcher, MAX_MESSAGE_SIZE);
	ev_io_init(&sock_watcher.io, sock_cb, sock, EV_WRITE|EV_READ);
	ev_io_start(loop, &sock_watcher.io);

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(atoi(port)); 
	if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) { 
		return 1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		if (errno != EINPROGRESS) {
			return 1;
		}
	}

	// stdout
	socket_setnonblock(1);
	connection_init(&stdout_watcher, MAX_MESSAGE_SIZE);
	ev_io_init(&stdout_watcher.io, stdout_cb, 1, 0);
	ev_io_start(loop, &stdout_watcher.io);

	// send watch request
	u32 total_len = sizeof(char) + sizeof(i64) + strlen(topic);
	connection_iovec parts[4];
	parts[0].buf = &total_len;
	parts[0].len = sizeof(u32);
	parts[1].buf = "w";
	parts[1].len = sizeof(char);
	parts[2].buf = &offset;
	parts[2].len = sizeof(i64);
	parts[3].buf = topic;
	parts[3].len = strlen(topic);

	connection_send_multi(&sock_watcher, parts, 4);
	connection_make_writable(&sock_watcher, loop);

	signal(SIGPIPE, SIG_IGN);

	ev_loop(loop, 0);

	return 0;
}

