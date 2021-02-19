#include "munit/munit.h"

#include "../session.h"
#include "../watchers.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct context {
	int visited;
	void *first;
	void *last;
} context;

static void visitor(session *s, void *ctx) {
	context *c = (context*)ctx;
	c->visited++;

	c->last = s;
	if (!c->first) c->first = s;
}

static MunitResult test_basic(const MunitParameter params[], void* data) {
	session ss[4];
	for (int i = 0; i < 4; i++) munit_assert(0 == session_init(ss+i));
	session *a = ss;
	session *b = ss+1;
	session *c = ss+2;
	session *d = ss+3;

	watchers w;
	munit_assert(0 == watchers_init(&w));

	watchers_update_watcher(&w, 1, 0, 0, a);
	watchers_update_watcher(&w, 2, 0, 0, b);
	watchers_update_watcher(&w, 1, 0, 0, c);
	watchers_update_watcher(&w, 3, 0, 0, d);

	context ctx = {0};
	watchers_foreach(&w, 0, visitor, &ctx);
	munit_assert(0 == ctx.visited);
	munit_assert(0 == ctx.first);
	munit_assert(0 == ctx.last);

	memset(&ctx, 0, sizeof(context));
	watchers_foreach(&w, 1, visitor, &ctx);
	munit_assert(2 == ctx.visited);
	munit_assert(a == ctx.first);
	munit_assert(c == ctx.last);

	memset(&ctx, 0, sizeof(context));
	watchers_foreach(&w, 2, visitor, &ctx);
	munit_assert(1 == ctx.visited);
	munit_assert(b == ctx.first);
	munit_assert(b == ctx.last);

	memset(&ctx, 0, sizeof(context));
	watchers_foreach(&w, 3, visitor, &ctx);
	munit_assert(1 == ctx.visited);
	munit_assert(d == ctx.first);
	munit_assert(d == ctx.last);

	memset(&ctx, 0, sizeof(context));
	watchers_foreach(&w, 4, visitor, &ctx);
	munit_assert(0 == ctx.visited);
	munit_assert(0 == ctx.first);
	munit_assert(0 == ctx.last);

	watchers_destroy(&w);

	for (int i = 0; i < 4; i++) session_destroy(ss+i);

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

static const MunitSuite test_suite = { "watchers", test_suite_tests, NULL, 1, 0 };

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
	return munit_suite_main(&test_suite, NULL, argc, argv);
}

