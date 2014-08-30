#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include "server.h"

static int setup_server_socket(unsigned short port) {
	static int yes = 1; //For set setsockopt
	int server_fd;
	static struct sockaddr_in server_addr;

	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Failed creating socket.\n");
		return -1;
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		fprintf(stderr, "setsockopt failed.\n");
		return -1;
	}

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		close(server_fd);
		fprintf(stderr, "Failed binding socket.\n");
		return -1;
	}

	if (listen(server_fd, DEFAULT_BACKLOG)) {
		close(server_fd);
		fprintf(stderr, "Failed listening on socket.\n");
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

	char buf[MAXBUF];

	if ((server_fd = setup_server_socket(port)) == -1) {
		fprintf(stderr, "Failed seting up server socket.\n");
		return;
	}

	FD_ZERO(&master);
	FD_SET(server_fd, &master);
	fd_max = server_fd;

	/*===============Start accepting requests================*/
	client_addr_len = sizeof(client_addr);
	while (1) {
		read_fds = master;

		if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) == -1) {
			fprintf(stderr, "select error");
			return;
		}

		for (i = 0; i < fd_max + 1; ++i) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == server_fd) {	//New request!
					puts("incoming requests!");
					if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, 
											  (socklen_t *)&client_addr_len)) == -1) {
						close(server_fd);
						fprintf(stderr, "Error accepting connection.\n");
						return;
					}

					FD_SET(client_fd, &master);
					if (client_fd > fd_max)
						fd_max = client_fd;
				} else {	//More data from current request
					nbytes = recv(i, buf, sizeof(buf), 0);
					if (nbytes < 0) {
						fprintf(stderr, "recv error!");
						return;
					}
					if (nbytes == 0) {
						close(i);
						FD_CLR(i, &master);
					}
					if (nbytes > 0) {
						puts(buf);
						if (send(i, buf, nbytes, 0) == -1) {
							fprintf(stderr, "send error!");
							return;
						}
					}
				}	//End IO processing
			}	//End if (FD_ISSET)
		}	//End for i
	}

}