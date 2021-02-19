.PHONY: all
all: hashmap ring queue pool store watchers

hashmap: hashmap.c
	gcc -O2 munit/munit.c hashmap.c -o hashmap -pthread

ring: ring.c ../ring.c
	gcc -O2 munit/munit.c ../ring.c ring.c -o ring -pthread

queue: queue.c ../queue.c
	gcc -O2 munit/munit.c ../threads.c ../queue.c ../ring.c queue.c -o queue -pthread

pool: pool.c ../session.c ../connection.c ../ring.c ../ev.c
	gcc -O2 munit/munit.c pool.c ../threads.c ../ev.c ../ring.c ../connection.c ../session.c ../pool.c -o pool -pthread -w

store: store.c ../store.c
	gcc -O2 munit/munit.c ../lib/liblmdb/mdb.c ../lib/liblmdb/midl.c ../lib/lz4/lz4.c ../store.c store.c -o store -pthread

watchers: watchers.c ../watchers.c
	gcc -O2 munit/munit.c ../ev.c ../threads.c ../session.c ../connection.c ../ring.c ../watchers.c watchers.c -o watchers -pthread

.PHONY: run
run: all
	./hashmap
	./ring
	./queue
	./pool
	./store
	./watchers

