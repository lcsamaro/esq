#ifndef UDATA_H
#define UDATA_H

#include "pool.h"
#include "queue.h"
#include "store.h"
#include "watchers.h"

typedef struct loop_userdata {
	ev_async async_w;
	ev_async async_close_w;
	watchers ws;
	store s;

	session_pool pool;

	i64 *write_offsets; //[MAX_TOPICS]; // copy of store->write_offsets

	queue reader_worker_queue;
	queue writer_worker_queue;
	queue store_worker_queue;

	mtx_t mutex;
} loop_userdata;

#endif /* UDATA_H */

