/** @file http_client.h
 *  @brief Define client struct
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __HTTP_CLIENT__
#define __HTTP_CLIENT__

#include "io.h"

/* http response code */
#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define METHOD_NOT_ALLOWED 405
#define LENGTH_REQUIRED 411
#define INTERNAL_SERVER_ERROR 500
#define NOT_IMPLEMENTED 501
#define SERVICE_UNAVAILABLE 503
#define HTTP_VERSION_NOT_SUPPORTED 505

/* Maximum size of a string buffer */
#define MAXBUF 8096

/** @brief Store information of a single http header.
 *
 * Headers are organized using linked list.
 */
typedef struct http_header {
    char* key;
    char* val;
    struct http_header *next;
} http_header_t;

/** @brief Store information of a single request */
typedef struct http_request {
    char method[MAXBUF];        //<! Only accept GET, POST, HEAD as method
    char uri[MAXBUF];
    http_header_t *headers;
} http_request_t;

/** @brief Store information of a single client.
 *
 * Clients are organized using linked list.
 */
typedef struct http_client {
    int fd;                 //<!client's file descriptor
    buf_t *in, *out;        //<!input and output buffer assigned to this client
    http_request_t* req;     //<!request from this client
    struct http_client* next;   //<!next client in the linked list
} http_client_t;

http_client_t *client_head;     //<!first client in the linked list

/* Initialize and destroy object */
void deinit_header(http_header_t *header);
void deinit_request(http_request_t *req);
http_request_t* new_request();
void deinit_client(http_client_t *client);
http_client_t* new_client(int fd);

/* IO with client */
void client_write(http_client_t *client, char* buf, int buf_len);
int client_readline(http_client_t *client, char *line);
void send_response_line(http_client_t *client, int code);

#endif