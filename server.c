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
#if __linux__
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "command.h"
#include "common.h"
#include "ev.h"
#include "la.h"
#include "lib/liblmdb/lmdb.h"
#include "pool.h"
#include "queue.h"
#include "session.h"
#include "sock.h"
#include "store.h"
#include "threads.h"
#include "udata.h"
#include "watchers.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKLOG_SZ 20

#define N_READ_TRDS 4
#define MAX_READ_TRDS 32

#define READER_WORKER_QUEUE_SIZE ((sizeof(session*)+sizeof(u32))*maxconn * 2)
#define NOTIFY_READER_WORKER_QUEUE_SIZE (READER_WORKER_QUEUE_SIZE)
#define WRITER_WORKER_QUEUE_SIZE (MAX_MESSAGE_SIZE * 256)
#define STORE_WORKER_QUEUE_SIZE (WRITER_WORKER_QUEUE_SIZE * 4)

static int store_visitor(u64 offset, char *buf, u32 len, void *ctx) {
	session *s = (session*)ctx;
	connection *conn = (connection*)s;

	u32 total_len = sizeof(u64) + len;
	connection_iovec parts[3];
	parts[0].buf = &total_len;
	parts[0].len = sizeof(u32);
	parts[1].buf = &offset;
	parts[1].len = sizeof(u64);
	parts[2].buf = buf;
	parts[2].len = len;
	if (connection_send_multi(conn, parts, 3)) { // full send buffer
		return 1;
	}

	s->offset = offset+1; // next offset to read

	return 0;
}

int reader_worker(void *arg) {
	struct ev_loop *loop = (struct ev_loop *)arg;
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);

	MDB_txn *txn;
	if (mdb_txn_begin(u->s.env, NULL, MDB_RDONLY, &txn)) {
		return 1;
	}

	MDB_cursor *mc;
	if (mdb_cursor_open(txn, u->s.dbi, &mc)) {
		return 1;
	}

	mdb_txn_reset(txn);

	for (;;) {
		u8 *buf;
		u32 len;

		// > rqueue
		queue_peek(&u->reader_worker_queue, (void**)&buf, &len, 1);
		if (!len) { // close signal
			queue_drop(&u->reader_worker_queue);
			break;
		}

		session *s;
		memcpy(&s, buf, sizeof(session*));

		queue_pop(&u->reader_worker_queue);
		// < rqueue

		// > session
		session_lock(s);

		s->enqueued = 0;
		if (s->live || !s->watch) {
			session_unlock(s);
			continue;
		}

		if (mdb_txn_renew(txn)) {
			// err
		}
		if (mdb_cursor_renew(txn, mc)) {
			// err
		}

		int should_write = 0;
		switch(store_read_some(mc, s->watch, s->offset, store_visitor, s)) {
		case 1: // done - write
		case 3: // more - write
			should_write = 1;
			break;
		case 2: // done - nothing to write
			if (queue_push(&u->notify_worker_queue, &s, sizeof(session*), 0)) {
				// should never happen
			}
			break;
		case 0: // full buffer
			break;
		case -1: // error
			break;
		}

		mdb_txn_reset(txn);

		if (should_write) {
			mtx_lock(&u->mutex);
			connection_enable_write((connection*)s, loop);
			mtx_unlock(&u->mutex);
		}

		session_unlock(s);

		if (should_write) {
			ev_async_send(loop, &u->async_w);
		}
	}

	mdb_cursor_close(mc);

	mdb_txn_abort(txn);

	return 0;
}

int writer_worker(void *arg) {
	struct ev_loop *loop = (struct ev_loop *)arg;
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);

	char msg[MAX_MESSAGE_SIZE];
	for (;;) {
		char *buf;
		u32 len;

		// > wqueue
		queue_peek(&u->writer_worker_queue, (void**)&buf, &len, 1);

		if (!len) { // close signal
			queue_drop(&u->writer_worker_queue);
			// << wqueue
			break;
		}

		session *s;
		memcpy(&s, buf, sizeof(session*));

		len -= sizeof(session*);
		buf += sizeof(session*);

		memcpy(msg, buf, len);

		queue_pop(&u->writer_worker_queue);
		// < wqueue

		process_command(loop, s, msg, len);
	}

	return 0;
}

int store_worker(void *arg) {
	struct ev_loop *loop = (struct ev_loop *)arg;
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);

	for (;;) {
		char *buf;
		u32 len;

		queue_peek(&u->store_worker_queue, (void**)&buf, &len, 1);

		if (store_write_txn_begin(&u->s)) {
			goto write_err_drop;
		}

		do {
			if (!len) {
				queue_drop(&u->store_worker_queue);
				if (store_write_txn_end(&u->s)) {
					goto write_err;
				}
				goto done;
			}

			if (*buf != 'e') goto create_drop;
			buf++;
			len--;

			int itopic;
			memcpy(&itopic, buf, sizeof(int));
			buf += sizeof(int);
			len -= sizeof(int);

			if (store_write_event(&u->s, itopic, buf, len)) {
				goto write_err_drop;
			}

			continue;
create_drop:
			switch (*buf) {
			case 'c':
				{
				buf++;
				len--;

				int itopic;
				memcpy(&itopic, buf, sizeof(int));
				buf += sizeof(int);
				len -= sizeof(int);

				store_create_topic(&u->s, buf, len, itopic);
				}
				break;
			case 'd':
				// TODO
				break;
			}


		} while (!queue_peek_next(&u->store_worker_queue, (void**)&buf, &len));

		queue_pop(&u->store_worker_queue);

		if (store_write_txn_end(&u->s)) {
			goto write_err;
		}

		// notify readers
		if (queue_peek(&u->notify_worker_queue, (void**)&buf, &len, 0)) continue;
		do {
			if (queue_push(&u->reader_worker_queue, buf, len, 0)) {
				// should not happen
			}
		} while (!queue_peek_next(&u->notify_worker_queue, (void**)&buf, &len));
		queue_pop(&u->notify_worker_queue);
	}
done:
	return 0;

write_err_drop:
	queue_drop(&u->store_worker_queue);
write_err:
	ev_async_send(loop, &u->async_close_w);
	return 1;
}

// > loop > session > rqueue
// > loop > session > wqueue
void io_cb(struct ev_loop *loop, struct ev_io* watcher, int revents) {
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);

	mtx_unlock(&u->mutex);
	// > loop
	session *s = (session*)watcher;
	connection *conn = (connection*)s;

	if (revents & EV_WRITE) {
		// > session
		session_lock(s);

		//mtx_lock(&u->mutex);
		if (connection_onwrite(conn, loop) < 0) { // err
			//mtx_unlock(&u->mutex);
			goto disconnect;
		}
		//mtx_unlock(&u->mutex);

		if (connection_empty_send(conn)) {
			mtx_lock(&u->mutex);
			connection_disable_write(conn, loop);
			mtx_unlock(&u->mutex);
		}

		if (s->watch && !s->live) {
			if (s->enqueued) {
				goto unlock;
			}

			// read some
			// > rqueue
			if (queue_push(&u->reader_worker_queue, &s, sizeof(session*), 0)) {
				// < rqueue
				// should never happen with a reader queue big enough
				// but if it happens, disconnect
				goto disconnect;
			}
			// < rqueue

			s->enqueued = 1;
		}
unlock:
		session_unlock(s);
		// < session
	}

	if (!(revents & EV_READ)) {
		goto done;
	}

	session_lock(s);
	//mtx_lock(&u->mutex);
	if (connection_onread(conn) < 0) {
		//mtx_unlock(&u->mutex);
		goto disconnect;
	}
	//mtx_unlock(&u->mutex);

	for (;;) {
		connection_iovec parts[2];
		parts[0].buf = NULL;
		parts[0].len = sizeof(u32);
		parts[1].buf = NULL;
		parts[1].len = 0;

		// > session
		if (connection_peek_multi(conn, parts, 1)) {
			break;
		}
		memcpy(&parts[1].len, parts[0].buf, sizeof(u32));

		if (parts[1].len + sizeof(u32) > MAX_MESSAGE_SIZE) { // behave
			goto disconnect;
		}

		if (connection_peek_multi(conn, parts, 2)) {
			break;
		}

		queue_buffer_part qparts[2];
		qparts[0].buf = &conn;
		qparts[0].len = sizeof(connection*);
		qparts[1].buf = parts[1].buf;
		qparts[1].len = parts[1].len;

		if (validate_command((char*)parts[1].buf, parts[1].len) == 1) {
			// writer
			// > wqueue
			queue_push_multi(&u->writer_worker_queue, qparts, 2, 1);
		}

		connection_consume_multi(conn, parts, 2);
	}
	session_unlock(s);
done:
	mtx_lock(&u->mutex);
	return;
disconnect:
	session_unlock(s);

	close(((ev_io*)s)->fd);

	watchers_lock(&u->ws);
	session_lock(s);
	watchers_update_watcher(&u->ws, 0, 0, 0, s);
	session_unlock(s);
	watchers_unlock(&u->ws);

	session_pool_free(&u->pool, s);

	mtx_lock(&u->mutex);
	ev_io_stop(loop, (ev_io*)s);
	return;
}

void accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	if (revents & EV_ERROR) return;

	int client_fd = accept(w->fd, (struct sockaddr*) &client_addr, &client_len);
	if (client_fd < 0) return;
	if (socket_setnonblock(client_fd) < 0 ||
			socket_setnodelay(client_fd) < 0) {
		close(client_fd);
		return;
	}

	loop_userdata *u = (loop_userdata*)ev_userdata(loop);
	session *s = session_pool_alloc(&u->pool);
	if (!s) {
		close(client_fd);
		return;
	}

	ev_io_init((ev_io*)s, io_cb, client_fd, EV_READ);
	ev_io_start(loop, (ev_io*)s);
}

static void l_release(struct ev_loop *loop) {
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);
	mtx_unlock(&u->mutex);
}

static void l_acquire(struct ev_loop *loop) {
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);
	mtx_lock(&u->mutex);
}

static void async_cb(struct ev_loop *loop, ev_async *w, int revents) {
}

static void async_close_cb(struct ev_loop *loop, ev_async *w, int revents) {
	ev_break(loop, EVBREAK_ALL);
}

void sig_cb(struct ev_loop *loop, ev_signal *w, int revents) {
	ev_break(loop, EVBREAK_ALL);
}

int setlimits(u64 maxconn) {
	struct rlimit limit;
	if (getrlimit(RLIMIT_NOFILE, &limit)) {
		return 1;
	}

	limit.rlim_cur = maxconn * 3 + 128;

#ifdef __APPLE__
	// TODO: OPEN_MAX
#endif

	if (setrlimit(RLIMIT_NOFILE, &limit)) {
		return 1;
	}

	return 0;
}

void usage() {
	fprintf(stderr, "Usage: esq-server [-h host] [-p port] [-s size] [-n dbname] [-c maxconnections]\n");
	exit(1);
}

int main(int argc, char **argv) {
	// parse args
	char *host = "127.0.0.1";
	char *port = "4000";
	u64 dbsize = 1ULL<<30;
	char *dbname = "db";
	u64 maxconn = 1024;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) {
			if (++i >= argc) usage();
			host = argv[i];
		} else if (!strcmp(argv[i], "-p")) {
			if (++i >= argc) usage();
			port = argv[i];
		} else if (!strcmp(argv[i], "-s")) {
			if (++i >= argc) usage();
			long v = atol(argv[i]);
			if (v > 0) dbsize *= v;
		} else if (!strcmp(argv[i], "-n")) {
			if (++i >= argc) usage();
			dbname = argv[i];
		} else if (!strcmp(argv[i], "-c")) {
			if (++i >= argc) usage();
			long v = atol(argv[i]);
			maxconn = v;
		} else if (!strcmp(argv[i], "--")) {
			break;
		} else {
			usage();
		}
	}

	if (setlimits(maxconn)) {
		puts("Error setting rlimits");
		return 1;
	}

	unsigned int evflags = ev_recommended_backends() | EVBACKEND_KQUEUE | EVBACKEND_EPOLL;
	struct ev_loop *loop = ev_default_loop (evflags);
	loop_userdata u;
	if (mtx_init(&u.mutex, mtx_plain) != thrd_success) {
		puts("Error creating loop mutex");
		return 1;
	}

	if (watchers_init(&u.ws)) {
		puts("Error creating topic manager");
		return 1;
	}

	if (store_init(&u.s, dbname, MAX_TOPICS, dbsize)) {
		puts("Error creating store");
		return 1;
	}

	// pool
	if (session_pool_init(&u.pool, maxconn)) {
		puts("Error creating connection pool");
		return 1;
	}

	// local write offsets copy - begin
	u.write_offsets = malloc(sizeof(i64) * MAX_TOPICS);
	if (!u.write_offsets) {
		return 1;
	}
	memcpy(u.write_offsets, u.s.write_offsets, sizeof(i64) * MAX_TOPICS);
	for (int i = 0; i < MAX_TOPICS; i++) {
		if (u.write_offsets[i] < 0) {
			u.write_offsets[i] = 0;
		} else {
			u.write_offsets[i] &= 0xffffffffffffull;
		}
	}
	// local write offsets copy - end

	ev_set_userdata(loop, &u);

	ev_async_init(&u.async_w, async_cb);
	ev_async_start(loop, &u.async_w);

	ev_async_init(&u.async_close_w, async_close_cb);
	ev_async_start(loop, &u.async_close_w);

	if (queue_init(&u.reader_worker_queue, READER_WORKER_QUEUE_SIZE)) {
		puts("Error creating worker queue");
		return 1;
	}

	if (queue_init(&u.notify_worker_queue, NOTIFY_READER_WORKER_QUEUE_SIZE)) {
		puts("Error creating notify worker queue");
		return 1;
	}

	if (queue_init(&u.writer_worker_queue, WRITER_WORKER_QUEUE_SIZE)) {
		puts("Error creating writer worker queue");
		return 1;
	}

	if (queue_init(&u.store_worker_queue, STORE_WORKER_QUEUE_SIZE)) {
		puts("Error creating store worker queue");
		return 1;
	}

	thrd_t read_worker_trds[MAX_READ_TRDS];
	for (int i = 0; i < N_READ_TRDS; i++) {
		if (thrd_create(read_worker_trds+i, reader_worker, loop) != thrd_success) {
			puts("Error creating worker thread");
			return 1; // TODO: cleanup
		}
	}

	thrd_t writer_worker_trd;
	if (thrd_create(&writer_worker_trd, writer_worker, loop) != thrd_success) {
		puts("Error creating writer worker thread");
		return 1;
	}

	thrd_t store_worker_trd;
	if (thrd_create(&store_worker_trd, store_worker, loop) != thrd_success) {
		puts("Error creating store worker thread");
		return 1;
	}

	int listen_fd = socket_bindlisten(host, port, BACKLOG_SZ);
	if (listen_fd < 0) {
		puts("listen failed");
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	ev_signal sigint_watcher;
	ev_signal_init(&sigint_watcher, sig_cb, SIGINT);
	ev_signal_start(loop, &sigint_watcher);

	ev_signal sigterm_watcher;
	ev_signal_init(&sigterm_watcher, sig_cb, SIGTERM);
	ev_signal_start(loop, &sigterm_watcher);

	struct ev_io w_accept;
	ev_io_init(&w_accept, accept_cb, listen_fd, EV_READ);
	ev_io_start(loop, &w_accept);

	ev_set_loop_release_cb(loop, l_release, l_acquire);

	mtx_lock(&u.mutex);

	ev_run(loop, 0);

	mtx_unlock(&u.mutex);

	ev_default_destroy();

	int res;
	queue_clear(&u.reader_worker_queue);
	queue_push(&u.reader_worker_queue, NULL, 0, 1);
	for (int i = 0; i < N_READ_TRDS; i++) {
		if (thrd_join(read_worker_trds[i], &res) == thrd_success) {
		}
	}

	queue_push(&u.writer_worker_queue, NULL, 0, 1);
	if (thrd_join(writer_worker_trd, &res) == thrd_success) {
	}

	queue_push(&u.store_worker_queue, NULL, 0, 1);
	if (thrd_join(store_worker_trd, &res) == thrd_success) {
	}
	if (res) {
		fprintf(stderr, "Database full\n");
	}

	queue_destroy(&u.reader_worker_queue);
	queue_destroy(&u.notify_worker_queue);
	queue_destroy(&u.writer_worker_queue);
	queue_destroy(&u.store_worker_queue);

	store_destroy(&u.s);
	watchers_destroy(&u.ws);
	mtx_destroy(&u.mutex);

	free(u.write_offsets);

	session_pool_destroy(&u.pool);

	puts("bye");

	return 0;
}

