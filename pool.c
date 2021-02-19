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
#include "pool.h"

#include <stdlib.h>

int session_pool_init(session_pool *p, u32 n) {
	p->pool = (session*)malloc(n * sizeof(session));
	if (!p->pool) {
		return 1;
	}

	p->n = n;
	for (u32 i = 0; i < n; i++) {
		if (session_init(p->pool+i)) {
			p->n = i;
			session_pool_destroy(p);
			return 1;
		}
	}

	for (u32 i = 1; i < n; i++) {
		p->pool[i-1].next = p->pool+i;
	}
	p->pool[n-1].next = NULL;
	p->next = p->pool;

	return 0;
}
void session_pool_destroy(session_pool *p) {
	for (u32 i = 0; i < p->n; i++) {
		session_destroy(p->pool+i);
	}
	free(p->pool);
}

session *session_pool_alloc(session_pool *p) {
	session *s = p->next;
	if (s) p->next = s->next;
	return s;
}

void session_pool_free(session_pool *p, session *s) {
	session_reset(s);
	s->next = p->next;
	p->next = s;
}

