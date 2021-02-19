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
#ifndef SESSION_H
#define SESSION_H

#include "lib/tailq.h"

#include "connection.h"
#include "threads.h"

// session
typedef struct session {
	connection conn;

	mtx_t mutex;

	// watch
	i64 offset; // next offset to read
	int enqueued;
	int watch;
	int live;

	// pool
	struct session *next;

	// bcast
	struct session *bcast_next;

	// watcher tailq
	SM_TAILQ_ENTRY(session) entries;
} session;

int session_init(session *s);
void session_destroy(session *s);
void session_reset(session *s);
void session_lock(session *s);
void session_unlock(session *s);

#endif /* SESSION_H */
