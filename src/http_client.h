/** @file http_client.h
 *  @brief Define client struct
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __HTTP_CLIENT__
#define __HTTP_CLIENT__

#include "io.h"

/** @brief Store information of a single http header.
 *
 * Headers are organized using linked list.
 */
typedef struct http_header {
    char* key, val;
    struct http_header *next;
} http_header_t;

/** @brief Store information of a single request */
typedef struct http_request {
    char method[10];        //<! Only accept GET, POST, HEAD as method
    char uri[MAXLINE];
    http_header_t *headers;
} http_request_t;

/** @brief Store information of a single client.
 *
 * Clients are organized using linked list.
 */
typedef struct client_list {
    int fd;                 //<!client's file descriptor
    buf_t *in, *out;        //<!input and output buffer assigned to this client
    http_request_t req;     //<!request from this client
    struct client_list *next;   //<!next client in the linked list
} client_list_t;

client_list_t *client_head;     //<!first client in the linked list

void deinit_client(client_list_t *client);
client_list_t* new_client(int fd);

void write_client(client_list_t *client, char* buf, int buf_len);
int client_readline(client_list_t *client, char *line);

#endif