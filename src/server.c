/*
 * server.c - Implementation of a concurrent echo server
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "server.h"
#include "io.h"

//Create and config a socket on given port.
static int setup_server_socket(unsigned short port) {
	static int yes = 1; //For setsockopt
	int server_fd;
	static struct sockaddr_in server_addr;

	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Failed creating socket.");
		return -1;
	}

	//Enable duplicate address and port binding
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
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

static client_list_t* new_client(int fd) {
	client_list_t *client = malloc(sizeof(client_list_t));

	client->fd = fd;
	client->buf = malloc(sizeof(buf_t));
	io_init(client->buf);
	client->next = NULL;

	return client;
}

static void deinit_client(client_list_t *client) {
	close(client->fd);
	fprintf(stderr, "Closed fd %d\n", client->fd);
	io_deinit(client->buf);
	free(client);
}

/*
 * serve - Create a concurrent server on given port
 *
 * Use select() to handle multiple socket. Each time a new connection established,
 * the newly created socket will be made non-blocking so that the server can read
 * as much data as possible each time by polling the socket.
 */
void serve(unsigned short port) {
	int server_fd, client_fd;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;

	fd_set master, 		//master fd list
		   read_fds, write_fds; //fd list to be passed in to select()

	int fd_max;		//Keep track of max fd passed into select()

	client_list_t *client,
				  *prev; //previous item in linked list when iterating
	int nbytes;

	if ((server_fd = setup_server_socket(port)) == -1) return;
	client_head = NULL;

	//Ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

	//initialize fd lists
	FD_ZERO(&master);
	fd_max = server_fd;

	/*===============Start accepting requests================*/
	client_addr_len = sizeof(client_addr);
	while (1) {
		read_fds = master;
		write_fds = master;
		FD_SET(server_fd, &read_fds);

		if (select(fd_max + 1, &read_fds, &write_fds, NULL, NULL) == -1) {
			perror("select error");
			continue;
		}

		//New request!
		if (FD_ISSET(server_fd, &read_fds)) {
			if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
									(socklen_t *)&client_addr_len)) == -1) {
				perror("Error accepting connection.");
			}
			//Add socket to fd list
			FD_SET(client_fd, &master);
			//Make client_fd a non-blocking socket
			fcntl(client_fd, F_SETFL, O_NONBLOCK);
			//Insert into client list
			client = new_client(client_fd);
			//Put at the head of client list
			if (client_head == NULL)
				client_head = client;
			else {
				client->next = client_head->next;
				client_head->next = client;
			}
		}

		fd_max = server_fd;

		prev = NULL;
		for (client = client_head; client != NULL; ) {
			//Prevent deleting a client If no action performed on it
			nbytes = 1;

			if (FD_ISSET(client->fd, &read_fds)) //New data arrived!
				nbytes = io_recv(client->fd, client->buf);
			else if (FD_ISSET(client->fd, &write_fds) &&
					 client->buf->writehead < client->buf->datasize) //Time to send more data
				nbytes = io_send(client->fd, client->buf);

			if (nbytes <= 0) { //Delete client
				FD_CLR(client->fd, &master); //Remove from fd set

				//Remove from the linked list
				if (prev == NULL)
					client_head = client->next;
				else
					prev->next = client->next;
				deinit_client(client);

				if (prev == NULL)
					client = client_head;
				else
					client = prev->next;
			} else {
				if (client->fd > fd_max)
					fd_max = client->fd;
				prev = client;
				client = client->next;
			}
		} // End for client
	}
}
