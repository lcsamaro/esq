#ifndef STORE_H
#define STORE_H

#include "lib/liblmdb/lmdb.h"

#include "common.h"
#include "la.h"

la_hashmap_dec(map_str_int, char*, int);

typedef struct store {
	MDB_env *env;
	MDB_dbi  dbi;
	MDB_txn *wtxn;
	MDB_cursor *wmc;

	map_str_int topics;

	char *compressed;
	int max_compressed;

	i64 write_offsets[MAX_TOPICS];
} store;

int store_init(store *s, char *name, u32 maxdbs, u64 mapsz);
void store_destroy(store *s);

int store_get_topic(store *s, char *topic, u32 topic_len, int create, int *newtopic);
int store_create_topic(store *s, char *topic, u32 topic_len, int itopic);

int store_write_txn_begin(store *s);
int store_write_txn_end(store *s);
int store_write_event(store *s, int itopic, char *buf, u32 len);

int store_drop(store *s, char *topic);

typedef int (*event_visitor)(u64 offset, char *buf, u32 len, void *ctx);
int store_read_some(MDB_cursor *mc, int itopic, u64 offset, event_visitor fn, void *ctx);

#endif /* STORE_H */

