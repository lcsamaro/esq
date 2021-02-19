#include "munit/munit.h"

#include "../ring.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static MunitResult test_rw(const MunitParameter params[], void* data) {
	ring_buffer rb;

	const int cap = 1<<14;
	munit_assert(0 == ring_buffer_init(&rb, cap));
	munit_assert(NULL != ring_buffer_data(&rb));

	munit_assert(0 == ring_buffer_size(&rb));
	munit_assert(cap == ring_buffer_space(&rb));

	munit_assert(1 == ring_buffer_canwrite(&rb, 1));
	munit_assert(0 == ring_buffer_canread(&rb, 1));

	ring_buffer_write(&rb, "test", 4);
	munit_assert(1 == ring_buffer_canread(&rb, 1));
	munit_assert(4 == ring_buffer_size(&rb));
	munit_assert(cap-4 == ring_buffer_space(&rb));

	ring_buffer_write(&rb, "1234", 4);
	munit_assert(1 == ring_buffer_canread(&rb, 1));
	munit_assert(8 == ring_buffer_size(&rb));
	munit_assert(cap-8 == ring_buffer_space(&rb));

	munit_assert(0 == memcmp(ring_buffer_consume(&rb, 4), "test", 4));
	munit_assert(4 == ring_buffer_size(&rb));
	munit_assert(0 == memcmp(ring_buffer_consume(&rb, 4), "1234", 4));
	munit_assert(0 == ring_buffer_size(&rb));

	munit_assert(ring_buffer_data(&rb) == ring_buffer_curw(&rb));
	memcpy(ring_buffer_curw(&rb), "hello", 5);
	munit_assert(0 == ring_buffer_size(&rb));
	ring_buffer_addw(&rb, 5);
	munit_assert(5 == ring_buffer_size(&rb));
	munit_assert(0 == memcmp(ring_buffer_consume(&rb, 5), "hello", 5));

	ring_buffer_clear(&rb);
	munit_assert(0 == ring_buffer_size(&rb));

	ring_buffer_destroy(&rb);

	return MUNIT_OK;
}

static MunitResult test_full(const MunitParameter params[], void* data) {
	ring_buffer rb;

	const int cap = 1<<14;
	munit_assert(0 == ring_buffer_init(&rb, cap));
	munit_assert(NULL != ring_buffer_data(&rb));

	ring_buffer_addw(&rb, cap);

	munit_assert(0 == ring_buffer_canwrite(&rb, 1));
	munit_assert(1 == ring_buffer_canread(&rb, 1));

	ring_buffer_consume(&rb, 4);

	munit_assert(1 == ring_buffer_canwrite(&rb, 1));
	munit_assert(1 == ring_buffer_canread(&rb, 1));

	int sz = ring_buffer_size(&rb);
	int space = ring_buffer_space(&rb);

	munit_assert(4 == space);

	ring_buffer_write(&rb, "a", 1);
	ring_buffer_write(&rb, "bcd", 3);
	munit_assert(0 == ring_buffer_canwrite(&rb, 1));
	munit_assert(1 == ring_buffer_canread(&rb, 1));

	ring_buffer_consume(&rb, sz);

	munit_assert(0 == memcmp(ring_buffer_consume(&rb, 2), "ab", 2));
	munit_assert(0 == memcmp(ring_buffer_consume(&rb, 2), "cd", 2));

	ring_buffer_destroy(&rb);

	return MUNIT_OK;
}

static void* setup(const MunitParameter params[], void* user_data) {
	return NULL;
}

static void tear_down(void* fixture) {
}

static MunitTest test_suite_tests[] = {
	{ "/test-rw", test_rw, setup, tear_down, 0, NULL },
	{ "/test-full", test_full, setup, tear_down, 0, NULL },
	{ NULL, NULL, NULL, NULL, 0, NULL }
};

static const MunitSuite test_suite = { "ring", test_suite_tests, NULL, 1, 0 };

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
	return munit_suite_main(&test_suite, NULL, argc, argv);
}

