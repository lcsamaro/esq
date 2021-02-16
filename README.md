# esq - event streaming queue
This implements the publish-subscribe pattern as a set of commands (esq-server, esq-tail, esq-write, esq-drop). A client application may directly access esq-server (esq-tail, esq-write, esq-drop are just that).

Topics are stored forever, using [lmdb](https://symas.com/lmdb/).

Async I/O is done through [libev](http://software.schmorp.de/pkg/libev.html).

## building
`$ make`

## run server
`$ ./esq-server`

## tail topic
`$ ./esq-tail topic_a`

## write event
`$ echo "hello" | ./esq-write topic_a`

## load newline-delimited data
`$ ./esq-write topic_a < data.ndjson`

## compose
`$ ./esq-tail topic_a | jq .foo | ./esq-write topic_b`

## drop topic
`$ ./esq-drop topic_b`

## benchmark
`$ ./esq-bench`

## bundled external libraries
* [freebsd tailq](https://github.com/freebsd/freebsd-src/blob/master/contrib/sendmail/include/sm/tailq.h)
* [libev](http://software.schmorp.de/pkg/libev.html)
* [lmdb](https://symas.com/lmdb/)
* [lz4](http://lz4.github.io/lz4/)
* [tinycthread](https://github.com/tinycthread/tinycthread)

## TODO
* code cleanup
* esq-drop

