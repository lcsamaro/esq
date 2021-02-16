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
#ifndef QUEUE_H
#define QUEUE_H

#include "ring.h"

#include "threads.h"

typedef struct queue_buffer_part {
	void *buf;
	u32   len;
} queue_buffer_part;

typedef struct queue {
	mtx_t mutex;
	cnd_t not_empty;
	cnd_t not_full;

	ring_buffer buffer;
} queue;

int queue_init(queue *q, u32 size);
void queue_destroy(queue *q);
void queue_clear(queue *q);
int queue_push_multi(queue *q, queue_buffer_part *parts, u32 n, int block);
int queue_push(queue *q, void *buf, u32 len, int block);
void queue_peek(queue *q, void **buf, u32 *len);
int queue_peek_next(queue *q, void **buf, u32 *len);
void queue_pop(queue *q);
void queue_drop(queue *q);

int queue_size(queue *q);

#endif /* QUEUE_H */
