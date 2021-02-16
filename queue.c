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
#include "queue.h"

int queue_init(queue *q, u32 size) {
	if (mtx_init(&q->mutex, mtx_plain) != thrd_success) return 1;
	if (cnd_init(&q->not_empty) != thrd_success) goto err0;
	if (cnd_init(&q->not_full) != thrd_success) goto err1;
	if (!ring_buffer_init(&q->buffer, size)) return 0;
	cnd_destroy(&q->not_full);
err1:
	cnd_destroy(&q->not_empty);
err0:
	mtx_destroy(&q->mutex);
	return 1;
}

void queue_destroy(queue *q) {
	mtx_destroy(&q->mutex);
	cnd_destroy(&q->not_empty);
	cnd_destroy(&q->not_full);
	ring_buffer_destroy(&q->buffer);
}

void queue_clear(queue *q) {
	mtx_lock(&q->mutex);
	ring_buffer_clear(&q->buffer);
	mtx_unlock(&q->mutex);
}

int queue_push_multi(queue *q, queue_buffer_part *parts, u32 n, int block) {
	u32 total = 0;
	for (u32 i = 0; i < n; i++) total += parts[i].len;

	mtx_lock(&q->mutex);

	while (!ring_buffer_canwrite(&q->buffer, total + sizeof(u32))) {
		if (!block) {
			mtx_unlock(&q->mutex);
			return 1;
		}
		cnd_wait(&q->not_full, &q->mutex);
	}

	ring_buffer_write(&q->buffer, &total, sizeof(u32));
	for (u32 i = 0; i < n; i++) {
		ring_buffer_write(&q->buffer, parts[i].buf, parts[i].len);
	}

	cnd_signal(&q->not_empty);

	mtx_unlock(&q->mutex);

	return 0;
}

int queue_push(queue *q, void *buf, u32 len, int block) {
	queue_buffer_part parts;
	parts.buf = buf;
	parts.len = len;
	return queue_push_multi(q, &parts, 1, block);
}

static int queue_empty(queue *q) {
	return ring_buffer_size(&q->buffer) ? 0 : 1;
}

static void queue_consume(queue *q) {
	if (queue_empty(q)) return;

	u32 len;
	memcpy(&len, ring_buffer_data(&q->buffer), sizeof(u32));
	ring_buffer_consume(&q->buffer, len + sizeof(u32));
}

void queue_peek(queue *q, void **buf, u32 *len) {
	mtx_lock(&q->mutex);
	while (queue_empty(q)) {
		cnd_wait(&q->not_empty, &q->mutex);
	}
	memcpy(len, ring_buffer_data(&q->buffer), sizeof(u32));
	*buf = (u8*)ring_buffer_data(&q->buffer) + sizeof(u32);
}

int queue_peek_next(queue *q, void **buf, u32 *len) {
	queue_consume(q);
	if (queue_empty(q)) return 1;
	memcpy(len, ring_buffer_data(&q->buffer), sizeof(u32));
	*buf = (u8*)ring_buffer_data(&q->buffer) + sizeof(u32);
	return 0;
}

void queue_pop(queue *q) {
	queue_consume(q);

	cnd_signal(&q->not_full);
	mtx_unlock(&q->mutex);
}

void queue_drop(queue *q) {
	cnd_signal(&q->not_empty);
	mtx_unlock(&q->mutex);
}

int queue_size(queue *q) {
	return ring_buffer_size(&q->buffer);
}

