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
#include "connection.h"

#include <errno.h>
#include <unistd.h>

int connection_init(connection *o, u32 size) {
	if (ring_buffer_init(&o->r, size)) return 1;
	if (ring_buffer_init(&o->w, size)) {
		ring_buffer_destroy(&o->r);
		return 1;
	}
	return 0;
}

void connection_destroy(connection *o) {
	ring_buffer_destroy(&o->r);
	ring_buffer_destroy(&o->w);
}

void connection_reset(connection *o) {
	ring_buffer_clear(&o->r);
	ring_buffer_clear(&o->w);
}

void connection_enable_write(connection *c, struct ev_loop *loop) {
	ev_io_stop(loop, (ev_io*)c);
	ev_io_modify((ev_io*)c, EV_READ|EV_WRITE);
	ev_io_start(loop, (ev_io*)c);
}

void connection_disable_write(connection *c, struct ev_loop *loop) {
	ev_io_stop(loop, (ev_io*)c);
	ev_io_modify((ev_io*)c, EV_READ);
	ev_io_start(loop, (ev_io*)c);
}

int connection_send_multi(connection *o, connection_iovec *parts, u32 n) {
	u32 total = 0;
	for (u32 i = 0; i < n; i++) total += parts[i].len;
	if (!ring_buffer_canwrite(&o->w, total)) return -1;
	for (u32 i = 0; i < n; i++) {
		ring_buffer_write(&o->w, parts[i].buf, parts[i].len);
	}
	return 0;
}
int connection_send(connection *o, char *buf, u32 len) {
	connection_iovec parts[1];
	parts[0].buf = buf;
	parts[0].len = len;
	return connection_send_multi(o, parts, 1);
}
int connection_peek(connection *o, char **buf, u32 len) {
	u32 rbsize = ring_buffer_size(&o->r);
	if (rbsize >= len) {
		*buf = (char*)ring_buffer_data(&o->r);
		return 0;
	}
	return -1;
}
int connection_peek_multi(connection *o, connection_iovec *parts, u32 n) {
	u32 total = 0;
	for (u32 i = 0; i < n; i++) total += parts[i].len;
	if (!ring_buffer_canread(&o->r, total)) return -1;
	u32 offset = 0;
	for (u32 i = 0; i < n; i++) {
		parts[i].buf = (u8*)ring_buffer_data(&o->r) + offset;
		offset += parts[i].len;
	}
	return 0;
}
u32 connection_peek_all(connection *o, char **buf) {
	u32 rbsize = ring_buffer_size(&o->r);
	*buf = (char*)ring_buffer_data(&o->r);
	return rbsize;
}
void connection_consume(connection *o, u32 len) {
	ring_buffer_consume(&o->r, len);
}
void connection_consume_multi(connection *o, connection_iovec *parts, u32 n) {
	for (u32 i = 0; i < n; i++) ring_buffer_consume(&o->r, parts[i].len);
}
int connection_onread(connection *c) {
	int fd = c->io.fd;

	u32 len = ring_buffer_space(&c->r);
	if (!len) return 0; // no space left on buffer to read

	errno = 0;
#ifdef CONNECTION_USE_SEND_RECV
	int bytes = recv(fd, ring_buffer_curw(&c->r), len, 0);
#else
	int bytes = read(fd, ring_buffer_curw(&c->r), len);
#endif

	if (bytes <= 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK ) {
			return 0;
		}
		return -1;
	}

	ring_buffer_addw(&c->r, bytes);

	return bytes;
}

int connection_onwrite(connection *c, struct ev_loop *loop) {
	void *data = ring_buffer_data(&c->w);
	u32 len = ring_buffer_size(&c->w);

	errno = 0;
#ifdef CONNECTION_USE_SEND_RECV
	int bytes = send(c->io.fd, data, len, 0);
#else
	int bytes = write(c->io.fd, data, len);
#endif
	if (bytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK ) {
			return 0;
		}
		return -1;
	}

	ring_buffer_consume(&c->w, bytes);
	/*if (!ring_buffer_size(&c->w)) {
		ev_io_stop(loop, (ev_io*)c);
		ev_io_modify((ev_io*)c, EV_READ);
		ev_io_start(loop, (ev_io*)c);
	}*/
	return bytes;
}

int connection_empty_read(connection *c) {
	return !ring_buffer_size(&c->r);
}

int connection_empty_send(connection *c) {
	return !ring_buffer_size(&c->w);
}

