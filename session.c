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
#include "common.h"
#include "session.h"

int session_init(session *s) {
	if (mtx_init(&s->mutex, mtx_plain) != thrd_success) {
		return 1;
	}

	s->connected = 1;

	s->offset = 0;
	s->enqueued = 0;
	s->watch = 0;
	s->live = 0;

	return connection_init(&s->conn, MAX_MESSAGE_SIZE);
}

void session_destroy(session *s) {
	mtx_destroy(&s->mutex);
	connection_destroy(&s->conn);
}

void session_reset(session *s) {
	s->connected = 0;

	s->offset = 0;
	s->enqueued = 0;
	s->watch = 0;
	s->live = 0;

	connection_reset(&s->conn);
}

void session_lock(session *s) {
	mtx_lock(&s->mutex);
}

void session_unlock(session *s) {
	mtx_unlock(&s->mutex);
}

