#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "server.h"
#include "io.h"

static int setup_server_socket(unsigned short port) {
	static int yes = 1; //For set setsockopt
	int server_fd;
	static struct sockaddr_in server_addr;

	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Failed creating socket.");
		return -1;
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		perror("setsockopt failed.");
		return -1;
	}

	//don't generate SIGPIPE
	if (setsockopt(server_fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int)) < 0) {
		perror("setsockopt failed.");
		return -1;
	}

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		close(server_fd);
		perror("Failed binding socket.");
		return -1;
	}

	if (listen(server_fd, DEFAULT_BACKLOG)) {
		close(server_fd);
		perror("Failed listening on socket.");
		return -1;
	}

	return server_fd;
}

void serve(unsigned short port) {
	int server_fd, client_fd;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;

	fd_set master, read_fds;
	int fd_max;		//Keep track of max fd passed into select

	int i; //Iteration variables
	int nbytes;

	buf_t buf;

	if ((server_fd = setup_server_socket(port)) == -1) return;

	FD_ZERO(&master);
	FD_SET(server_fd, &master);
	fd_max = server_fd;

	/*===============Start accepting requests================*/
	client_addr_len = sizeof(client_addr);
	while (1) {
		read_fds = master;

		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select error");
			return;
		}

		for (i = 0; i < fd_max + 1; ++i) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == server_fd) {	//New request!
					fprintf(stderr, "incoming requests!\n");
					if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
								   		    (socklen_t *)&client_addr_len)) == -1) {
						close(server_fd);
						perror("Error accepting connection.");
						return;
					}

					FD_SET(client_fd, &master);

					fcntl(client_fd, F_SETFL, O_NONBLOCK);

					if (client_fd > fd_max)
						fd_max = client_fd;
				} else {	//More data from current request
					io_init(&buf);
					nbytes = io_recvall(i, &buf);

					if (nbytes == 0) {
						fprintf(stderr, "Connection ends.\n");
						close(i);
						FD_CLR(i, &master);
					}

					io_send(i, &buf);
					io_deinit(&buf);
				}	//End IO processing
			}	//End if (FD_ISSET)
		}	//End for i
	}
}
