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
#ifndef LA_H
#define LA_H

/* typedefs */
#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

#include <stdlib.h>

#ifndef la_malloc
#define la_malloc malloc
#endif

#ifndef la_free
#define la_free free
#endif

#ifndef la_sz
#define la_sz u64
#endif

#ifndef la_initial_cap
#define la_initial_cap 16
#endif

#include <string.h>

#define la_lt(l, r) ((l) < (r) ? 1 : 0)
#define la_le(l, r) ((l) <= (r) ? 1 : 0)
#define la_eq(l, r) ((l) == (r) ? 1 : 0)
#define la_eq_cstr(l, r) (!strcmp((l), (r)))

#endif /* LA_H */

/* vector */
#define la_vector(Name, T)                                      \
typedef struct Name {                                           \
	T *data;                                                \
	la_sz size;                                             \
	la_sz capacity;                                         \
} Name;                                                         \
void Name##_reserve(Name *o, la_sz sz) {                        \
	o->capacity = la_initial_cap;                           \
	while (o->capacity < sz) o->capacity *= 2;              \
}                                                               \
int Name##_resize(Name *o, la_sz sz) {                          \
	return 1;                                               \
}                                                               \
int Name##_init(Name *o) {                                      \
	o->capacity = la_initial_cap;                           \
	o->size = 0;                                            \
	o->data = (T*)la_malloc(la_initial_cap * sizeof(T));    \
	return o->data == NULL;                                 \
}                                                               \
void Name##_destroy(Name *o) {                                  \
	if (o->capacity) la_free(o->data);                      \
}                                                               \
int Name##_dup(Name *o, Name *dup) {                            \
	T *data = (T*)la_malloc(o->capacity * sizeof(T));       \
	if (!data) return 1;                                    \
	dup->size = o->size;                                    \
	dup->capacity = o->capacity;                            \
	memcpy(data, o->data, o->size * sizeof(T));             \
	dup->data = data;                                       \
	return 0;                                               \
}                                                               \
la_sz Name##_size(Name *o) { return o->size; }                  \
la_sz Name##_capacity(Name *o) { return o->capacity; }          \
int Name##_empty(Name *o) { return o->size == 0; }              \
void Name##_clear(Name *o) { o->size = 0; }                     \
inline int Name##_maybegrow(Name *o, la_sz a) {                 \
	if (o->size+a >= o->capacity) {                         \
		la_sz n = o->capacity * 2;                      \
		T *data = (T*)la_malloc(n * sizeof(T));         \
		if (!data) return 1;                            \
		memcpy(data, o->data, o->size * sizeof(T));     \
		la_free(o->data);                               \
		o->data = data;                                 \
		o->capacity = n;                                \
	}                                                       \
	return 0;                                               \
}                                                               \
T Name##_get(Name *o, la_sz i) { return o->data[i]; }           \
void Name##_set(Name *o, la_sz i, T v) { o->data[i] = v; }      \
int Name##_push(Name *o, T v) {                                 \
	if (Name##_maybegrow(o, 1)) return 1;                   \
	o->data[o->size++] = v;                                 \
	return 0;                                               \
}                                                               \
T Name##_pop(Name *o) { return o->data[--(o->size)]; }          \
                                                                \
la_sz Name##_begin(Name *o) {                                   \
	return 0;                                               \
}                                                               \
la_sz Name##_end(Name *o) {                                     \
	return o->size;                                         \
}                                                               \
la_sz Name##_next(Name *o, la_sz it) {                          \
	return it+1;                                            \
}                                                               \
T Name##_value(Name *o, la_sz i) {                              \
	return o->data[i];                                      \
}                                                               \
void Name##_dummy()

/* hashmap */
#define h_distance(p, h) (((p)-(h)) & (mask))

#define la_hashmap_dec(Name, K, V)            \
typedef struct Name##_entry {                   \
	K key;                                  \
	V value;                                \
} Name##_entry;                                 \
typedef struct Name {                           \
	Name##_entry *data;                     \
	la_sz size;                             \
	la_sz capacity;                         \
} Name

#define la_hashmap_def(Name, K, V, Hash, Eq, Nil)                           \
void Name##_clear(Name *o) {                    \
	o->size = 0;                            \
	for (la_sz i = 0; i < o->capacity; i++) \
		o->data[i].value = Nil;         \
}                                               \
int Name##_init(Name *o) {                                          \
	o->capacity = la_initial_cap;                               \
	o->data = (Name##_entry*)la_malloc(sizeof(Name##_entry) * la_initial_cap); \
	if (!o->data) return 1;                                     \
	Name##_clear(o);                                            \
	return 0;                                                   \
}                                                                   \
void Name##_destroy(Name *o) { la_free(o->data); }                  \
la_sz Name##_size(Name *o) { return o->size; }                      \
la_sz Name##_capacity(Name *o) { return o->capacity; }              \
int Name##_empty(Name *o) { return o->size == 0; }                  \
V Name##_get(Name *o, const K k) {                                  \
	la_sz cap = Name##_capacity(o);                               \
	la_sz mask = (cap-1);                                         \
	la_sz i, h;                                                   \
	i = h = Hash(k) & mask;                                     \
	while (o->data[i].value != Nil) {                           \
		if (h_distance(i, Hash(o->data[i].key)&mask) <      \
			h_distance(i, h)) return Nil;               \
		if (Eq(o->data[i].key, k))                          \
			return o->data[i].value;                    \
		i = ((i+1)&mask);                                   \
	}                                                           \
	return Nil;                                                 \
}                                                                   \
static int Name##_set_prehashed(Name *o, Name##_entry entry) {      \
	la_sz cap = Name##_capacity(o);                               \
	la_sz mask = (cap-1);                                         \
	                                                            \
	Name##_entry tmp;                                           \
	la_sz i = Hash(entry.key)&mask;                               \
	la_sz entry_hash = i;                                         \
	                                                            \
	la_sz dist = 0;                                               \
	for (;;) {                                                  \
		if (o->data[i].value == Nil) {                      \
			o->data[i] = entry;                         \
			o->size++;                                  \
			return 0;                                   \
		}                                                   \
		                                                    \
		la_sz cur_hash = Hash(o->data[i].key)&mask;           \
		la_sz cur_dist = h_distance(i, cur_hash);             \
		if (dist > cur_dist) {                              \
			tmp = entry;                                \
			entry = o->data[i];                         \
			o->data[i] = tmp;                           \
			                                            \
			entry_hash = cur_hash;                      \
			dist = cur_dist;                            \
		}                                                   \
		dist++;                                             \
		i = ((i+1)&mask);                                   \
	}                                                           \
	return 0;                                                   \
}                                                                   \
static int Name##_maybegrow(Name *o) {                                     \
	la_sz i, n;                                                 \
	Name##_entry *old, *data;                                   \
	if (o->size >= o->capacity/2) {                             \
		n = o->capacity * 2;                                \
		data = (Name##_entry*)la_malloc(n * sizeof(Name##_entry));         \
		if (!data) return 1;                                \
		                                                    \
		old = o->data;                                      \
		o->data = data;                                     \
		la_sz cap = o->capacity;                              \
		o->capacity = n;                                    \
		for (la_sz i = 0; i < o->capacity; i++)             \
			o->data[i].value = Nil;                     \
		                                                    \
		o->size = 0;                                        \
		for (i = 0; i < cap; i++) {                         \
			if (old[i].value == Nil) continue;          \
			Name##_set_prehashed(o, old[i]);            \
		}                                                   \
		                                                    \
		la_free(old);                                       \
	}                                                           \
	return 0;                                                   \
}                                                                   \
int Name##_set_impl(Name *o, Name##_entry entry) {                  \
        if (Name##_maybegrow(o)) return 1;                          \
	                                                            \
	la_sz cap = Name##_capacity(o);                               \
	la_sz mask = (cap-1);                                         \
	                                                            \
	Name##_entry tmp;                                           \
	la_sz i = Hash(entry.key)&mask;                               \
	la_sz entry_hash = i;                                         \
	                                                            \
	la_sz dist = 0;                                               \
	for (;;) {                                                  \
		if (o->data[i].value == Nil) {                      \
			o->data[i] = entry;                         \
			o->size++;                                  \
			return 0;                                   \
		}                                                   \
		la_sz cur_hash = Hash(o->data[i].key)&mask;           \
		la_sz cur_dist = h_distance(i, cur_hash);             \
		if (dist > cur_dist) {                              \
			                                            \
			tmp = entry;                                \
			entry = o->data[i];                         \
			o->data[i] = tmp;                           \
			                                            \
			entry_hash = cur_hash;                      \
			dist = cur_dist;                            \
		} else if (dist == cur_dist && entry_hash == cur_hash && \
			Eq(entry.key, o->data[i].key)) {            \
			/*o->data[i].value = value;*/               \
			o->data[i] = entry;                         \
			return 0;                                   \
		}                                                   \
		dist++;                                             \
		i = ((i+1)&mask);                                   \
	}                                                           \
	return 0;                                                   \
}                                                                   \
int Name##_set(Name *o, K key, V value) {                           \
	Name##_entry entry;                                         \
	entry.key = key;                                            \
	entry.value = value;                                        \
	return Name##_set_impl(o, entry);                           \
}                                                                   \
int Name##_remove(Name *o, K k) {                                   \
	/*la_sz cap = Name##_capacity(o);                               \
	la_sz mask = (cap-1);                                         \
	                                                            \
	Name##_entry entry, tmp;                                    \
	entry.key = key;                                            \
	entry.value = value;                                        \
	la_sz i = Hash(key)&mask;                                     \
	la_sz entry_hash = i;                                         \
	                                                            \
	la_sz dist = 0;                                               \
	while (o->data[i].value != Nil) {                           \
		if (h_distance(i, Hash(o->data[i].key)&mask) <      \
			h_distance(i, h)) return 1;                 \
		la_sz cur_hash = Hash(o->data[i].key)&mask;           \
		la_sz cur_dist = h_distance(i, cur_hash);             \
		if (Eq(entry.key, o->data[i].key)) {                \
			o->data[i].value = value;                   \
			return 0;                                   \
		}                                                   \
		dist++;                                             \
		i = ((i+1)&mask);                                   \
	}                                                           \
	return 0;                                                   \
	la_sz i, h; i = h = hfn(key) & (hm->data->cap-1); \
	while (!rhhm_value_empty(hm->data->table+i)) { \
		if (!cfn(hm->data->table[i].key, key)) { \
			la_sz j = i; \
			do { \
				j = ((j+1)&(hm->data->cap-1)); \
				hm->data->table[i] = hm->data->table[j]; \
				i = j; \
			} while (!rhhm_value_empty(hm->data->table+j) && \
				DISTANCE(j, ENTRY_HASH(hm->data->table[j])) != 0); \
			return; \
		} \
		if (ENTRY_HASH(hm->data->table[i]) < h) return; \
		i = ((i+1)&(hm->data->cap-1)); \
	} */\
	return (V)0;                                                \
}                                                                   \
la_sz Name##_begin(Name *o) { \
	for (la_sz i = 0; i < o->capacity; i++) { \
		if (o->data[i].value != Nil) return i; \
	} \
	return ((la_sz)-1); \
} \
la_sz Name##_end(Name *o) { \
	return ((la_sz)-1); \
} \
la_sz Name##_next(Name *o, la_sz it) { \
	for (la_sz i = it+1; i < o->capacity; i++) { \
		if (o->data[i].value != Nil) return i; \
	} \
	return ((la_sz)-1); \
} \
K Name##_key(Name *o, la_sz i) {                                   \
	return o->data[i].key;                                    \
} \
V Name##_value(Name *o, la_sz i) {                                   \
	return o->data[i].value;                                    \
} \
void Name##_dummy()


#define la_hashmap(Name, K, V, Hash, Eq, Nil) \
	la_hashmap_dec(Name, K, V, Nil) \
	la_hashmap_def(Name, K, V, Hash, Eq, Nil)

/* TODO: hashset */

/* heap - TODO: improve bubble up (gt instead of ge) */
#define la_heap_parent(i) ((i)/2)
#define la_heap_left(i)   (2*(i))
#define la_heap_right(i)  (2*(i)+1)

#define la_pqueue(Name, T, Lt)                                         \
                                                                       \
la_vector(Name##_internal, T);                                         \
typedef Name##_internal Name;                                          \
                                                                       \
int Name##_init(Name *o) {                                             \
	if (Name##_internal_init(o)) return 1;                         \
	o->size = 1;                                                   \
	return 0;                                                      \
}                                                                      \
                                                                       \
void Name##_destroy(Name *o) {                                         \
	Name##_internal_destroy(o);                                    \
}                                                                      \
                                                                       \
la_sz Name##_size(Name *o) {                                           \
	return Name##_internal_size(o) - 1;                            \
}                                                                      \
                                                                       \
int Name##_empty(Name *o) {                                            \
	return Name##_internal_size(o) <= 1;                           \
}                                                                      \
                                                                       \
void Name##_clear(Name *o) {                                           \
	Name##_internal_clear(o);                                      \
	o->size = 1;                                                   \
}                                                                      \
                                                                       \
void Name##_bubble_up(Name *o, la_sz i) {                              \
	T v = Name##_internal_get(o, i);                               \
                                                                       \
	while (i > 1 && !Lt(Name##_internal_get(o, la_heap_parent(i)), \
		Name##_internal_get(o, i))) {                          \
		                                                       \
		Name##_internal_set(o, i,                              \
			Name##_internal_get(o, la_heap_parent(i)));    \
                                                                       \
		Name##_internal_set(o, la_heap_parent(i), v);          \
		i = la_heap_parent(i);                                 \
	}                                                              \
}                                                                      \
                                                                       \
T Name##_pop(Name *o) {                                                \
	la_sz i = 1;                                                   \
	T v = Name##_internal_get(o, i);                               \
	la_sz sz = Name##_internal_size(o);                            \
	for (;;) {                                                     \
		la_sz l = la_heap_left(i);                             \
		la_sz r = la_heap_right(i);                            \
		                                                       \
		if (r < sz)  {                                         \
			                                               \
			T lval = Name##_internal_get(o, l);            \
			T rval = Name##_internal_get(o, r);            \
			                                               \
			if (Lt(lval, rval)) {                          \
				Name##_internal_set(o, i, lval);       \
				i = l;                                 \
			} else {                                       \
				Name##_internal_set(o, i, rval);       \
				i = r;                                 \
			}                                              \
			continue;                                      \
		} else if (l < sz) {                                   \
			Name##_internal_set(o, i,                      \
				Name##_internal_get(o, l));            \
			i = l;                                         \
			continue;                                      \
		}                                                      \
		break;                                                 \
	}                                                              \
                                                                       \
	Name##_internal_set(o, i, Name##_internal_pop(o));             \
	Name##_bubble_up(o, i);                                        \
                                                                       \
	return v;                                                      \
}                                                                      \
								       \
void Name##_bubble_down(Name *o, la_sz i) {                            \
	T v = Name##_internal_get(o, i);                               \
	la_sz sz = Name##_internal_size(o);                            \
	for (;;) {                                                     \
		la_sz l = la_heap_left(i);                             \
		la_sz r = la_heap_right(i);                            \
		la_sz smallest = i;                                    \
		                                                       \
		if (l < sz && Lt(Name##_internal_get(o, l),            \
			Name##_internal_get(o,i)))                     \
			smallest = l;                                  \
                                                                       \
		if (r < sz && Lt(Name##_internal_get(o, r),            \
			Name##_internal_get(o, smallest)))             \
			smallest = r;                                  \
                                                                       \
		if (smallest == i) return;                             \
                                                                       \
		Name##_internal_set(o, i,                              \
			Name##_internal_get(o, smallest));             \
		Name##_internal_set(o, smallest, v);                   \
		i = smallest;                                          \
	}                                                              \
}                                                                      \
                                                                       \
int Name##_insert(Name *o, T v) {                                      \
	la_sz pos = Name##_internal_size(o);                           \
	if (Name##_internal_push(o, v)) return 1;                      \
	Name##_bubble_up(o, pos);                                      \
	return 0;                                                      \
}                                                                      \
void Name##_dummy()

