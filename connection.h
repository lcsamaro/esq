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
#ifndef CONNECTION_H
#define CONNECTION_H

#include "ev.h"
#include "la.h"
#include "ring.h"

typedef struct connection_iovec {
	void *buf;
	u32 len;
} connection_iovec;

typedef struct connection {
	ev_io io;

	ring_buffer r;
	ring_buffer w;
} connection;

int connection_init(connection *o, u32 size);
void connection_destroy(connection *o);
void connection_reset(connection *o);
int connection_is_writable(connection *o);
void connection_enable_write(connection *o, struct ev_loop *loop);
void connection_disable_write(connection *o, struct ev_loop *loop);
int connection_send_multi(connection *o, connection_iovec *parts, u32 n);
int connection_send(connection *o, char *buf, u32 len);
int connection_peek(connection *o, char **buf, u32 len);
int connection_peek_multi(connection *o, connection_iovec *parts, u32 n);
u32 connection_peek_all(connection *o, char **buf);
void connection_consume(connection *o, u32 len);
void connection_consume_multi(connection *o, connection_iovec *parts, u32 n);
int connection_onread(connection *c);
int connection_onwrite(connection *c, struct ev_loop *loop);

int connection_empty_send(connection *c);

#endif /* CONNECTION_H */

