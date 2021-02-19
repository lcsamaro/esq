#include "munit/munit.h"

#include "../queue.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static MunitResult test_rw(const MunitParameter params[], void* data) {

	return MUNIT_OK;
}

static MunitResult test_full(const MunitParameter params[], void* data) {

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

static const MunitSuite test_suite = { "store", test_suite_tests, NULL, 1, 0 };

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
	return munit_suite_main(&test_suite, NULL, argc, argv);
}

