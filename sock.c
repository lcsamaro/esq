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
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <string.h>

int socket_setnonblock(int fd) {
	int flags = fcntl(fd, F_GETFL);
	if (flags < 0) return flags;
	flags |= O_NONBLOCK;
	if ((flags = fcntl(fd, F_SETFL, flags)) < 0) return flags;
	return 0;
}

int socket_setnodelay(int fd) {
	int y = 1;
	return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&y, sizeof(int));
}

int socket_setreuse(int fd) {
	int y = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int))) {
		return 1;
	}
	return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &y, sizeof(int));
}

int socket_bindlisten(char *addr, char *port, int backlog) {
	struct addrinfo hints, *info;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;

	if (getaddrinfo(addr, port, &hints, &info)) {
		return -1;
	}

	int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (fd < 0) {
		freeaddrinfo(info);
		return -1;
	}

	if (socket_setreuse(fd)) return -1;
	if (bind(fd, info->ai_addr, info->ai_addrlen)) {
		freeaddrinfo(info);
		return -1;
	}

	freeaddrinfo(info);

	if (listen(fd, backlog) < 0) return -1;
	if (socket_setnonblock(fd) < 0) return -1;

	return fd;
}

