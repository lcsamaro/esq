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
#include "ring.h"

#if __linux__
# define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *ring_buffer_malloc(size_t sz);
static void ring_buffer_free(void *ptr, size_t sz);

int ring_buffer_init(ring_buffer *b, int sz) {
	b->cap = sz;
	b->r = b->w = b->buf = (u8*)ring_buffer_malloc(sz);
	return b->buf ? 0 : 1;
}

void ring_buffer_destroy(ring_buffer *b) {
	ring_buffer_free(b->buf, b->cap);
}

void ring_buffer_clear(ring_buffer *b) {
	b->r = b->w = b->buf;
}

int ring_buffer_size(ring_buffer *b) {
	return b->w - b->r;
}

int ring_buffer_space(ring_buffer *b) {
	return b->cap - ring_buffer_size(b);
}

int ring_buffer_canwrite(ring_buffer *b, int len) {
	return ring_buffer_space(b) >= len;
}
int ring_buffer_canread(ring_buffer *b, int len) {
	return ring_buffer_size(b) >= len;
}

void ring_buffer_write(ring_buffer *b, void *buf, int len) {
	memcpy(b->w, buf, len);
	b->w += len;
}

void *ring_buffer_data(ring_buffer *b) {
	return b->r;
}

void *ring_buffer_curw(ring_buffer *b) {
	return b->w;
}
void ring_buffer_addw(ring_buffer *b, u32 len) {
	b->w += len;
}

void *ring_buffer_consume(ring_buffer *b, u32 len) {
	void *cur = b->r;
	b->r += len;
	if (b->r - b->buf >= b->cap) {
		b->r -= b->cap;
		b->w -= b->cap;
	}

	//printf("read head: %d\n", b->r - b->buf);

	return cur;
}

#if __linux__
//#include <sys/memfd.h>
//#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

static void *ring_buffer_malloc(size_t sz) {
#if __linux__
	int fd = syscall(SYS_memfd_create, "rbuf", 0);
	//int fd = memfd_create("rbuf", 0);
	//int fd = fileno(tmpfile());
#elif __APPLE__
	FILE *f = tmpfile();
	if (!f) return NULL;
	int fd = fileno(f);
	if (fd < 0) return NULL;
#endif
	ftruncate(fd, sz); // TODO: read return
	void *buf = mmap(NULL, sz*2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) return NULL;
	mmap(buf, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	mmap((u8*)buf+sz, sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
	return buf;
}

static void ring_buffer_free(void *ptr, size_t sz) {
	munmap((u8*)ptr+sz, sz);
	munmap(ptr, sz*2);
}

