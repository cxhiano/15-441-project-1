/** @file server.c
 *  @brief Implementation of a concurrent http server
 *
 *  Newly arrive data will be append to the input buffer of the client. And
 *  Each time when it's okay to send data to the client, the server tries to
 *  send data from the output buffer of the client.
 *
 *  @author Chao Xin(cxin)
 */
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <openssl/ssl.h>
#include "server.h"
#include "io.h"
#include "log.h"
#include "http_client.h"
#include "http_parser.h"

int terminate = 0;

static int http_fd, https_fd;

/** @brief Create and config a socket on given port. */
static int setup_server_socket(unsigned short port) {
	static int yes = 1; //For setsockopt
	int server_fd;
	static struct sockaddr_in server_addr;

	if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		log_error("Failed creating socket.");
		return -1;
	}

	//Enable duplicate address and port binding
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
		log_error("setsockopt failed.");
		return -1;
	}

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		close(server_fd);
		log_error("Failed binding socket.");
		return -1;
	}

	if (listen(server_fd, DEFAULT_BACKLOG)) {
		close(server_fd);
		log_error("Failed listening on socket.");
		return -1;
	}

	return server_fd;
}

/** @brief Accept connection from server_fd. If sucess, construct a client
 *	  	   struct and append it to the client linked list started with
 *   	   client_head
 *
 *  @param server_fd The server file descriptor which will be passed into
 * 		   accept()
 *  @param client_head The pointer to the head of a client linked list
 */
static void accept_connection(int server_fd, http_client_t **client_head) {
	int client_fd, ip_addr;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;
	struct hostent *host;
	http_client_t *client;

	client_addr_len = sizeof(client_addr);
	if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
							(socklen_t *)&client_addr_len)) == -1) {
		log_error("Error accepting connection.");
		return;
	}
	// Add socket to fd list
	add_read_fd(client_fd);
	add_write_fd(client_fd);
	// Make client_fd a non-blocking socket
	fcntl(client_fd, F_SETFL, O_NONBLOCK);
	//Insert into client list
	client = new_client(client_fd);
	// Record ip address
	ip_addr = client_addr.sin_addr.s_addr;
	if (inet_ntop(AF_INET, &client_addr, client->remote_ip,
			INET_ADDRSTRLEN) == NULL)
		log_error("Record client IP address error");

	// Get host name
	host = gethostbyaddr(&(client_addr.sin_addr), sizeof(struct in_addr), AF_INET);
	if (host == NULL)
		log_error("Record client host name error");
	client->remote_host = host->h_name;

	log_msg(L_INFO, "Incoming request from %s\n", client->remote_ip);

	// Put at the head of client list
	if (*client_head == NULL)
		*client_head = client;
	else {
		client->next = (*client_head)->next;
		(*client_head)->next = client;
	}
}

/** @brief Finalize the server
 *
 *  Free all memory and close all sockets.
 */
void finalize() {
	http_client_t *client, *next;

	close(http_fd);
	close(https_fd);

	for (client = client_head; client != NULL; client = next) {
		next = client->next;
		deinit_client(client);
	}
}

/** @brief Create a concurrent server to serve on given port
 *
 *  The server will serve on both http_port and https_port(see config.h).
 *  Use select() to handle multiple socket. Each time a new connection
 *  established, the newly created socket will be made non-blocking so that the
 *  server can read as much data as possible each time by polling the socket.
 *
 *  @return Should never return
 */
void serve() {
	http_client_t *client,
				  *prev; //previous item in linked list when iterating
	int nbytes, bad;

	if ((http_fd = setup_server_socket(http_port)) == -1) return;
	if ((https_fd = setup_server_socket(https_port)) == -1) return;
	client_head = NULL;

	//Ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

	//initialize fd lists
	init_select_context();
	add_read_fd(http_fd);
	add_read_fd(https_fd);

	/*===============Start accepting requests================*/
	while (!terminate) {
		if (io_select() == -1) {
			log_error("select error");
			continue;
		}

		//New request!
		if (test_read_fd(http_fd))
			accept_connection(http_fd, &client_head);

		prev = NULL;
		for (client = client_head; client != NULL; ) {
			/*
			 * Normally, bad will be 0 normally. When erro occurs, bad will
			 * be set to 1. And corresponding socket will be closed.
			 */
			bad = 0;

			// New data arrived!
			if (client->alive && test_read_fd(client->fd)) {
				nbytes = io_recv(client->fd, client->in);
				if (nbytes == -1) bad = 1;
			}

			// Parse data
			if (!bad && client->alive && client->status != C_PIPING) {
				if (http_parse(client) == -1) {
					// Something goes wrong and beyond repair.
					io_send(client->fd, client->out); 	// Response error
					bad = 1;	// End the connection
				}

				// Free part of the buffer if a lot of data has been processed
				if (empty(client->in)) io_shrink(client->in);
			}

			// Send data to client
			if (!bad && test_write_fd(client->fd)) {
				// Send data from buffer
				if (client->out->pos < client->out->datasize) {
					nbytes = io_send(client->fd, client->out);
					if (nbytes == -1) bad = 1;
				} else if (!bad && client->status == C_PIPING &&
						test_read_fd(client->pipe->from_fd)) {
					// Need to pipe data to client from some fd
					nbytes = io_pipe(client->fd, client->pipe);
					// Piping complete
					if (nbytes == 1)
						client->status = C_IDLE;
					if (nbytes == -1) bad = 1;
					// Deinit client pipe
					if (nbytes != 0) {
						free(client->pipe);
						client->pipe = NULL;
					}
				}
			}

			if (bad || (client->status == C_IDLE && !client->alive)) { //Delete client
				remove_read_fd(client->fd);
				remove_write_fd(client->fd);

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
				prev = client;
				client = client->next;
			}
		} // End for client
	}
}
