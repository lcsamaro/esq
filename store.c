#include "store.h"

#define LA_IMPLEMENTATION
#include "la.h"

#include "lib/liblmdb/lmdb.h"

#include "lib/lz4/lz4.h"

#include <ctype.h>
#include <stdio.h>

static u64 fnv1a(const void *buf) {
	const u8 *p = (const u8*)buf;
	u64 hash = 2166136261;
	while (*p) {
		hash = hash ^ *p++;
 		hash = hash * 16777619;
	}
	return hash;
}

static char *la_strdupn(const char *s, u64 len) {
	char *n = (char*)malloc(len+1);
	if (!n) return NULL;
	memcpy(n, s, len);
	n[len] = 0;
	return n;
}

la_hashmap_def(map_str_int, char*, int, fnv1a, la_eq_cstr, -1);

#define STORE_COMPRESSION

static u64 store_get_offset(store *s, MDB_cursor *mc, u64 itopic) {
	i64 offset = s->write_offsets[itopic];
	if (offset == -1) {
		u64 max_key = (itopic << 48) | 0xFFFFFFFFFFFF;
		MDB_val k, v;
		k.mv_data = &max_key;
		k.mv_size = sizeof(u64);

		if (mdb_cursor_get(mc, &k, &v, MDB_SET_RANGE)) {
			if (mdb_cursor_get(mc, &k, &v, MDB_LAST)) {
				goto err;
			}
			goto last;
		}
		if (mdb_cursor_get(mc, &k, &v, MDB_PREV)) goto err;
last:
		memcpy(&offset, k.mv_data, sizeof(u64));
		if ((offset >> 48) != itopic) {
err:
			offset = (itopic << 48);
		} else {
			offset++;
		}
		return offset;
	}
	return offset;
}

static int store_load_topics(store *s) {
	MDB_txn *txn;
	if (mdb_txn_begin(s->env, NULL, MDB_RDONLY, &txn)) {
		return 1;
	}

	MDB_cursor *mc;
	if (mdb_cursor_open(txn, s->dbi, &mc)) {
		mdb_txn_abort(txn);
		return 1;
	}

	int ret = 0;
	MDB_val k, v;
	while (mdb_cursor_get(mc, &k, &v, MDB_NEXT) == MDB_SUCCESS) {
		u64 itopic;
		memcpy(&itopic, k.mv_data, sizeof(u64));
		if (itopic > MAX_TOPICS) break;

		//printf("%llu -> %.*s\n", itopic, (int)v.mv_size, v.mv_data);

		if (map_str_int_set(&s->topics,
				la_strdupn(v.mv_data, v.mv_size), (int)itopic)) {
			ret = 1;
			goto err;
		}
	}

	// load offsets
	for (u64 i = map_str_int_begin(&s->topics);
			i != map_str_int_end(&s->topics);
			i = map_str_int_next(&s->topics, i)) {
		int itopic = map_str_int_value(&s->topics, i);
		s->write_offsets[itopic] = store_get_offset(s, mc, itopic);
		printf("offset of %d -> %lld\n", itopic, s->write_offsets[itopic]&0xffffffffffffull);
	}

err:
	mdb_cursor_close(mc);
	mdb_txn_abort(txn);
	return ret;
}


int store_init(store *s, char *name, u32 maxdbs, u64 mapsz) {
	int maxsz = LZ4_compressBound(MAX_MESSAGE_SIZE);
	s->compressed = malloc(maxsz);
	s->max_compressed = maxsz;
	if (!s->compressed) {
		return 1;
	}

	MDB_env *env;
	if (mdb_env_create(&env)) {
		return 1;
	}

	if (mdb_env_set_mapsize(env, mapsz)) {
		mdb_env_close(env);
		return 1;
	}

	if (mdb_env_open(env, name, MDB_NOSUBDIR | MDB_WRITEMAP | MDB_NOMEMINIT, 0664)) {
		mdb_env_close(env);
		return 1;
	}

	MDB_txn *txn;
	if (mdb_txn_begin(env, NULL, 0, &txn)) {
		mdb_env_close(env);
		return 1;
	}

	MDB_dbi dbi;
	if (mdb_dbi_open(txn, NULL, MDB_INTEGERKEY, &dbi)) {
		mdb_txn_abort(txn);
		mdb_env_close(env);
		return 1;
	}

	if (mdb_txn_commit(txn)) {
		mdb_txn_abort(txn);
		mdb_env_close(env);
		return 1;
	}

	s->env = env;
	s->dbi = dbi;

	for (int i = 0; i < MAX_TOPICS; i++) {
		s->write_offsets[i] = -1;
	}

	if (map_str_int_init(&s->topics)) {
		mdb_env_close(env);
		return 1;
	}

	if (store_load_topics(s)) {
		store_destroy(s);
		return 1;
	}

	return 0;
}

void store_destroy(store *s) {
	mdb_dbi_close(s->env, s->dbi);
	mdb_env_close(s->env);

	for (u64 i = map_str_int_begin(&s->topics);
			i != map_str_int_end(&s->topics);
			i = map_str_int_next(&s->topics, i)) {
		free(map_str_int_key(&s->topics, i));
	}
	map_str_int_destroy(&s->topics);

	free(s->compressed);
}

int store_get_topic(store *s, char *topic, u32 topic_len, int create, int *newtopic) {
	if (topic_len > MAX_TOPIC_NAME_LEN) {
		return -1;
	}

	*newtopic = 0;

	char key[MAX_TOPIC_NAME_LEN+1];
	memcpy(key, topic, topic_len);
	key[topic_len] = '\0';

	//printf("get topic: %s\n", key);

	int itopic = map_str_int_get(&s->topics, key);

	if (itopic > 0) return itopic;

	if (!create) return -1;

	itopic = map_str_int_size(&s->topics) + 1;

	if (itopic > MAX_TOPICS) {
		return -1;
	}

	// validate topic name // TODO: validate before?
	for (int i = 0; i < topic_len; i++) {
		if (!isgraph(topic[i])) {
			return -1;
		}
	}

	if (map_str_int_set(&s->topics, la_strdupn(topic, topic_len), itopic)) {
		return -1; // err
	}

	*newtopic = 1;

	return itopic;
}

int store_create_topic(store *s, char *topic, u32 topic_len, int itopic) {
	printf("creating topic %.*s (%d)\n", (int)topic_len, topic, itopic);
	u64 key = itopic;
	MDB_val k, v;
	k.mv_data = &key;
	k.mv_size = sizeof(u64);
	v.mv_data = topic;
	v.mv_size = topic_len;
	return (mdb_put(s->wtxn, s->dbi, &k, &v, 0) ? 1 : 0);
}

int store_write_txn_begin(store *s) {
	if (mdb_txn_begin(s->env, NULL, 0, &s->wtxn)) return 1;
	if (mdb_cursor_open(s->wtxn, s->dbi, &s->wmc)) {
		mdb_txn_abort(s->wtxn);
		return 1;
	}
	return 0;
}

int store_write_txn_end(store *s) {
	mdb_cursor_close(s->wmc);
	return mdb_txn_commit(s->wtxn);
}

int store_write_event(store *s, int itopic, char *buf, u32 len) {
	MDB_txn *txn = s->wtxn;

	MDB_cursor *mc = s->wmc;

	u64 offset = store_get_offset(s, mc, itopic);

	//printf("> topic: %d offset: %llu\n", itopic, offset&0xffffffffffffull);
	/*if (!(offset&0xffffffffffffull)) {
		store_create_topic(s, topic, itopic);
	}*/

	// compress
#ifdef STORE_COMPRESSION
	int csize = LZ4_compress_default(buf, s->compressed, len, s->max_compressed);
	// Check return_value to determine what happened.
	if (csize <= 0) {
		puts("compress error");
		return 1;
		//run_screaming("A 0 or negative result from LZ4_compress_default() indicates a failure trying to compress the data. ", 1);
	}
	if (csize > 0) {
		//printf("We successfully compressed some data! Ratio: %.2f\n", (float) csize/len);
	}
#endif


	// write to database
	MDB_val k, v;
	k.mv_data = &offset;
	k.mv_size = sizeof(u64);
#ifdef STORE_COMPRESSION
	v.mv_data = s->compressed;
	v.mv_size = csize;
#else
	v.mv_data = buf;
	v.mv_size = len;
#endif
	if (mdb_cursor_put(mc, &k, &v, 0)) {
		return 1;
	}

	s->write_offsets[itopic] = offset+1;

	return 0;
}

int store_read_some(store *s, MDB_cursor *mc, int itopic, u64 offset, event_visitor fn, void *ctx) {
	MDB_val k, v;

	offset = offset | (((u64)itopic) << 48);

	k.mv_data = &offset;
	k.mv_size = sizeof(u64);

	int some = 0;
	if (mdb_cursor_get(mc, &k, &v, MDB_SET_KEY) != MDB_SUCCESS) {
		return some ? 1 : 2; // 1 done - write, 2 done - nothing to write
	}

	do {
		u64 u;
		memcpy(&u, k.mv_data, sizeof(u64));

		if ((u>>48) != itopic) {
			return some ? 1 : 2; // 1 done - write, 2 done - nothing to write
		}

#ifdef STORE_COMPRESSION
		void *buf = v.mv_data;
		u32 len = v.mv_size;

		char msg[MAX_MESSAGE_SIZE];

		int dsize = LZ4_decompress_safe(buf, msg, len, MAX_MESSAGE_SIZE);
		if (dsize < 0) return -1;
#else
		char *msg = v.mv_data;
		u32 dsize = v.mv_size;
#endif
		if (fn(u&0xffffffffffffULL, msg, dsize, ctx)) {
			break;
		}
		some = 1;

		/*u64 itopic;
		memcpy(&itopic, k.mv_data, sizeof(u64));
		if (itopic > MAX_TOPICS) break;*/
	} while (mdb_cursor_get(mc, &k, &v, MDB_NEXT) == MDB_SUCCESS);

	return 0; // more - write
}

