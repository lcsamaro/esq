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
#include "command.h"
#include "queue.h"
#include "udata.h"
#include "threads.h"

int validate_command(char *buf, u32 len) {
	if (!len) return -1; // invalid command

	switch(*buf) {
	case 'e': // new event -> writer -> store
	case 'd': // drop topic -> writer?
	case 'w': // watch topic -> writer -> store? -> reader
	case 'u': // unwatch topic -> writer
	case 'p': // ping
		return 1;
	}

	return -1;
}

typedef struct bcast_context {
	i64 offset;
	connection_iovec *parts;

	session *bcast_next;
	queue *reader_worker_queue;
} bcast_context;

static void bcast(session *s, void *ctx) {
	connection *conn = (connection*)s;

	bcast_context *bctx = (bcast_context*)ctx;

	session_lock(s);

	if (!s->live) {
		if (bctx->offset == s->offset) { // live now
			s->live = 1;
		} else {
			goto done;
		}
	}

	if (bctx->offset != s->offset) {
		s->live = 0;
		if (!s->enqueued) {
			// add to reader queue
			queue_push(bctx->reader_worker_queue, &s, sizeof(session*), 1);
			s->enqueued = 1;
		}
		goto done;
	}

	if (connection_send_multi(conn, bctx->parts, 3)) { // full send buffer
		s->live = 0;
		if (!s->enqueued) {
			// add to reader queue
			s->enqueued = 1;
			queue_push(bctx->reader_worker_queue, &s, sizeof(session*), 1);
		}
		goto done;
	}

	if (!connection_is_writable((connection*)s)) {
		s->offset = bctx->offset+1; // next offset to read
	}

	// add to bcast write list
	s->bcast_next = bctx->bcast_next;
	bctx->bcast_next = s;

done:
	session_unlock(s);
}

int process_command(struct ev_loop *loop, session *s, char *buf, u32 len) {
	loop_userdata *u = (loop_userdata*)ev_userdata(loop);

	switch (*buf) {
	case 'e': // new event
		{
		buf++;
		len--;

		u8 topic_len;
		memcpy(&topic_len, buf, sizeof(u8));
		char *topic = buf+1;

		if (topic_len+1 >= len) {
			break; // err
		}

		int nt;
		int itopic = store_get_topic(&u->s, topic, topic_len, 1, &nt);
		if (itopic < 0) break;

		if (nt) { // create topic
			queue_buffer_part qparts[3];
			qparts[0].buf = "c";
			qparts[0].len = 1;
			qparts[1].buf = &itopic;
			qparts[1].len = sizeof(int);
			qparts[2].buf = topic;
			qparts[2].len = topic_len;
			queue_push_multi(&u->store_worker_queue, qparts, 3, 1);
		}

		char *data = buf + (topic_len+1);
		u32 data_len = len - (topic_len+1);

		// store
		queue_buffer_part qparts[3];
		qparts[0].buf = "e";
		qparts[0].len = 1;
		qparts[1].buf = &itopic;
		qparts[1].len = sizeof(int);
		qparts[2].buf = data;
		qparts[2].len = data_len;
		queue_push_multi(&u->store_worker_queue, qparts, 3, 1);

		// bcast
		u64 offset = u->write_offsets[itopic]++;
		u32 total_len = sizeof(u64) + data_len;
		connection_iovec parts[3];
		parts[0].buf = &total_len;
		parts[0].len = sizeof(u32);
		parts[1].buf = &offset;
		parts[1].len = sizeof(u64);
		parts[2].buf = data;
		parts[2].len = data_len;

		bcast_context bctx;
		bctx.offset = offset; // update offset
		bctx.parts = parts;
		bctx.bcast_next = NULL;
		bctx.reader_worker_queue = &u->reader_worker_queue;

		// > watchers_mutex > session_mutex > r_queue_mutex
		watchers_lock(&u->ws);
		watchers_foreach(&u->ws, itopic, bcast, &bctx);
		watchers_unlock(&u->ws);
		// < r_queue_mutex < session_mutex < watchers_mutex


		// > loop
		mtx_lock(&u->mutex);
		session *n = bctx.bcast_next;
		while (n) {
			connection_make_writable((connection*)n, loop);
			n = n->bcast_next;
		}
		mtx_unlock(&u->mutex);
		// < loop

		if (bctx.bcast_next) {
			ev_async_send(loop, &u->async_w);
		}

		}
		break;
	case 'd': // drop topic
		{
		int nt;
		int itopic = store_get_topic(&u->s, buf+1, len-1, 0, &nt);
		if (itopic < 0) break;

		// drop topic
		queue_buffer_part qparts[2];
		qparts[0].buf = "d";
		qparts[0].len = 1;
		qparts[1].buf = &itopic;
		qparts[1].len = sizeof(int);
		queue_push_multi(&u->store_worker_queue, qparts, 2, 1);

		// update offset
		u->write_offsets[itopic] = 0;

		}
		break;
	case 'w': // watch topic
		{
		if (len <= 1 + sizeof(i64)) {
			return 1;
		}

		i64 offset;
		memcpy(&offset, buf+1, sizeof(i64));

		char *topic = buf + 1 + sizeof(i64);
		u32 topic_len = len-(1 + sizeof(i64));

		int nt;
		int itopic = store_get_topic(&u->s, buf+9, len-9, 1, &nt);
		if (itopic < 0) break;

		if (nt) { // create topic
			queue_buffer_part qparts[3];
			qparts[0].buf = "c";
			qparts[0].len = 1;
			qparts[1].buf = &itopic;
			qparts[1].len = sizeof(int);
			qparts[2].buf = topic;
			qparts[2].len = topic_len;
			queue_push_multi(&u->store_worker_queue, qparts, 3, 1);
		}

		int live = 0;
		i64 abs_offset = 0;
		i64 wo = u->write_offsets[itopic];
		if (offset < 0) {
			if ((-offset) <= wo) {
				abs_offset = wo + offset;
			} else {
				abs_offset = 0;
			}
		} else if (offset > 0) {
			abs_offset = offset-1;
			if (abs_offset >= wo) {
				live = 1;
				abs_offset = wo;
			}
		} else {
			abs_offset = wo;
			live = 1;
		}

		// > watchers_mutex > session_mutex > r_queue_mutex
		watchers_lock(&u->ws);
		session_lock(s);
		watchers_update_watcher(&u->ws, itopic, abs_offset, live, s);

		if (!live && !s->enqueued) {
			queue_push(&u->reader_worker_queue, &s, sizeof(session*), 1);
			s->enqueued = 1;
		}

		session_unlock(s);
		watchers_unlock(&u->ws);
		// < rqueue_mutex < session_mutex < watchers_mutex

		}
		break;
	case 'u': // unwatch topic
		watchers_update_watcher(&u->ws, 0, 0, 0, s);
		break;
	case 'p':
		{
		char *pong = "p";
		u32 len = sizeof(u32) + sizeof(char);

		connection_iovec parts[2];
		parts[0].buf = &len;
		parts[0].len = sizeof(u32);
		parts[1].buf = &pong;
		parts[1].len = sizeof(char);

		session_lock(s);
		if (connection_send_multi((connection*)s, parts, 2)) { // full send buffer
			session_unlock(s);
			return 1;
		}
		session_unlock(s);

		mtx_lock(&u->mutex);
		connection_make_writable((connection*)s, loop);
		mtx_unlock(&u->mutex);

		ev_async_send(loop, &u->async_w);
		}
		break;
	}

	return 0;
}

