#include "munit/munit.h"

#define LA_IMPLEMENTATION
#include "../la.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static u64 fnv1a(const void *buf) {
	const u8 *p = (const u8*)buf;
	u64 hash = 2166136261;
	while (*p) {
		hash = hash ^ *p++;
 		hash = hash * 16777619;
	}
	return hash;
}

la_hashmap_dec(map, char*, int);
la_hashmap_def(map, char*, int, fnv1a, la_eq_cstr, -1);

static MunitResult test_basic(const MunitParameter params[], void* data) {
	map h;
	munit_assert(0 == map_init(&h));

	munit_assert(0 == map_size(&h));

	int sz = 80000;

	for (int i = 0; i < sz; i++) {
		char k[16];
		sprintf(k, "%d", i);
		munit_assert(-1 == map_get(&h, k));
	}

	for (int i = 0; i < sz; i++) {
		char k[16];
		sprintf(k, "%d", i);
		munit_assert(0 == map_set(&h, strdup(k), i));
		munit_assert(i == map_get(&h, k));
		munit_assert(i+1 == map_size(&h));
	}

	for (int i = 0; i < sz; i++) {
		char k[16];
		sprintf(k, "%d", i);
		munit_assert(i == map_get(&h, k));
	}

	for (int i = 0; i < sz; i++) {
		char k[16];
		sprintf(k, "%d", i+sz);
		munit_assert(-1 == map_get(&h, k));
	}

	map_destroy(&h);
	return MUNIT_OK;
}

static void* setup(const MunitParameter params[], void* user_data) {
	return NULL;
}

static void tear_down(void* fixture) {
}

static MunitTest test_suite_tests[] = {
	{ "/basic", test_basic, setup, tear_down, 0, NULL },
	{ NULL, NULL, NULL, NULL, 0, NULL }
};

static const MunitSuite test_suite = { "hashmap", test_suite_tests, NULL, 1, 0 };

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
	return munit_suite_main(&test_suite, NULL, argc, argv);
}

