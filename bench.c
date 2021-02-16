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
#define LA_IMPLEMENTATION
#include "la.h"

#include "sock.h"

#include "ev.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "connection.h"

#define MAX_MESSAGE_SIZE 16384

#define MAX_CLIENTS 64

connection clients[MAX_CLIENTS];

int snd(connection* conn) {
	static int init = 0;
	static char bufs[10][100];
	static int cur = 0;
	if (!init) {
		for (int i = 0; i < 10; i++) {
			memset(bufs[i], 'a'+i, 100);
			bufs[i][0] = 'e';
			bufs[i][1] = 1;
		}
		init = 1;
	}

	char *buf = bufs[cur++];
	cur = cur % 10;

	//printf("send %s\n", buf);

	u32 n = 20;
	connection_iovec parts[2];
	parts[0].buf = &n;
	parts[0].len = sizeof(u32);
	parts[1].buf = buf;
	parts[1].len = n;

	return connection_send_multi(conn, parts, 2);
}

static int total = 0;
void sock_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
	//puts("cb");
	connection* conn = (connection*)watcher;
	if (revents & EV_WRITE) {
		//puts("onwrite");
		connection_onwrite(conn, loop);
		//puts("write");
		while (!snd(conn)) {
			total++;
		}
		connection_make_writable(conn, loop);
	}
	if (revents & EV_READ) {
		//puts("READ");
		if (connection_onread(conn) < 0) {
			//puts("disconnect");
			// TODO
		}
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

		//printf("recv (%d): %.*s\n", parts[1].len, parts[1].len, parts[1].buf);

		connection_consume_multi(conn, parts, 2);
	}
	//puts("cb - end");
}

int newsock(struct ev_loop *loop, int i, char *addr, int port) {
	int sock = 0;
	struct sockaddr_in serv_addr; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 1;
	if (socket_setnonblock(sock) < 0) return 1;
	if (socket_setnodelay(sock) < 0) return 1;

	connection_init(clients+i, MAX_MESSAGE_SIZE);
	ev_io_init(&clients[i].io, sock_cb, sock, EV_READ|EV_WRITE);
	ev_io_start(loop, &clients[i].io);

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(port); 
	if (inet_pton(AF_INET, addr, &serv_addr.sin_addr)<=0) { 
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		if (errno != EINPROGRESS) {
			printf("\nConnection Failed \n"); 
			puts(strerror(errno));
			return -1;
		}
	}
	return 0;
}

double start;
static void clock_cb (struct ev_loop *loop, ev_periodic *w, int revents) {
	double n = ev_now(loop) - start;
	printf("%d events %lf s => %lf e/s\n", total, n, total/n);
}

void usage() {
	fprintf(stderr, "Usage: esq-bench [-h host] [-p port]\n");
	exit(1);
}

int main(int argc, char **argv) {
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
			usage();
		}
	}

	struct ev_loop *loop = EV_DEFAULT;
	for (int i = 0; i < MAX_CLIENTS; i++) {
		newsock(loop, i, host, atoi(port));
	}

	start = ev_now(loop);
	ev_periodic tick;
	ev_periodic_init(&tick, clock_cb, 0., 0.100, 0);
	ev_periodic_start(loop, &tick);

	ev_loop(loop, 0);

	puts("done");

	return 0;
}

