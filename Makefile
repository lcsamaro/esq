all: esq-server esq-tail esq-write esq-bench

WARNS := -Wno-unused-value -Wno-comment -Wno-extern-initializer
FLAGS := -pedantic $(WARNS) -pthread -flto -fomit-frame-pointer -ffast-math -fno-strict-aliasing -DNDEBUG

esq-server: server.c connection.c ring.c session.c store.c queue.c
	gcc -O3 server.c ev.c ring.c queue.c sock.c store.c watchers.c session.c connection.c command.c pool.c threads.c ./lib/liblmdb/mdb.c ./lib/liblmdb/midl.c ./lib/lz4/lz4.c -o esq-server $(FLAGS)

esq-tail: tail.c connection.c ring.c
	gcc -O3 tail.c ev.c ring.c sock.c connection.c -o esq-tail -pthread $(FLAGS)

esq-write: write.c connection.c ring.c
	gcc -O3 write.c ev.c ring.c sock.c connection.c -o esq-write -pthread $(FLAGS)

esq-bench: bench.c connection.c ring.c
	gcc -O3 bench.c ev.c ring.c sock.c connection.c -o esq-bench -pthread $(FLAGS)

