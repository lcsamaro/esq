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
#include "watchers.h"

int watchers_init(watchers *m) {
	if (mtx_init(&m->mutex, mtx_plain) != thrd_success) {
		return 1;
	}

	for (u32 i = 0; i < MAX_TOPICS+1; i++) {
		SM_TAILQ_INIT(m->watchers+i);
	}

	return 0;
}

void watchers_destroy(watchers *m) {
	mtx_destroy(&m->mutex);
}

void watchers_lock(watchers *m) {
	mtx_lock(&m->mutex);
}

void watchers_unlock(watchers *m) {
	mtx_unlock(&m->mutex);
}

void watchers_foreach(watchers *m, int itopic, session_visitor fn, void *ctx) {
	int res = 0;
	session *s;
	SM_TAILQ_FOREACH(s, m->watchers + itopic, entries) {
		fn(s, ctx);
	}
}

void watchers_update_watcher(watchers *m, int itopic, i64 offset, int live, session *s) {
	if (!itopic) {
		if (s->watch) {
			SM_TAILQ_REMOVE(m->watchers + s->watch, s, entries);
			s->offset = 0;
			s->live = 0;
			s->watch = 0;
		}
		return;
	}

	s->offset = offset;
	s->live = live;
	s->watch = itopic;
	SM_TAILQ_INSERT_TAIL(m->watchers + itopic, s, entries);
}

