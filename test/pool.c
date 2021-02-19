#include "munit/munit.h"

#include "../pool.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define sz 16

static MunitResult test_basic(const MunitParameter params[], void* data) {
	session_pool p;

	session *ss[sz];
	munit_assert(0 == session_pool_init(&p, 16));

	for (int i = 0; i < sz; i++) {
		munit_assert(NULL != (ss[i] = session_pool_alloc(&p)));
	}
	munit_assert(NULL == session_pool_alloc(&p));

	for (int i = 0; i < sz; i+=2) {
		session_pool_free(&p, ss[i]);
	}

	for (int i = 0; i < sz; i+=2) {
		munit_assert(NULL != session_pool_alloc(&p));
	}
	munit_assert(NULL == session_pool_alloc(&p));

	session_pool_destroy(&p);
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

static const MunitSuite test_suite = { "pool", test_suite_tests, NULL, 1, 0 };

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
	return munit_suite_main(&test_suite, NULL, argc, argv);
}

