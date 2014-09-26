/** @file http_client.h
 *  @brief Define client struct and function to deal with requests from client.
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

/**
 * Client status code
 *
 * Indicates what's happening with a client
 */
#define C_IDLE 0            // Idle
#define C_PHEADER 1         // Parsing header
#define C_PBODY 2           // Waiting for complete body
/**
 * The client is piping from a file or cgi output to the client socket. When
 * a client is in this status, it will not do any parsing until the piping is
 * done. This prevents the server from returning the response data of next
 * request before finishing the current response.
 */
#define C_PIPING 3

/* Methods */
#define M_GET 0
#define M_HEAD 1
#define M_POST 2

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
    int method;
    char uri[MAXBUF];
    char *body;
    int content_len;
    http_header_t *headers; //Headers in a linked list
} http_request_t;

/** @brief Store information of a single client.
 *
 *  Clients are organized using linked list. The server maintain an this object
 *  for each client. The object includes the file descriptor, data regarding
 *  current request, and an input buffer and an output buffer.
 */
typedef struct http_client {
    int fd;                 //<!client's file descriptor
    pipe_t *pipe;           //<!pipe from a file or cgi output
    int status;             //<!thte current status of this client
    buf_t *in, *out;        //<!input and output buffer assigned to this client
    http_request_t* req;     //<!current request from this client
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
void client_write_string(http_client_t *client, char* str);
int client_readline(http_client_t *client, char *line);
void send_response_line(http_client_t *client, int code);
void send_header(http_client_t *client, char* key, char* val);
int end_request(http_client_t *client, int code);

/* helper functions */
int strcicmp(char* s1, char* s2);
char* get_request_header(http_request_t *req, char *key);
int connection_close(http_request_t *req);

#endif