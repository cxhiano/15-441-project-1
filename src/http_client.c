/** @file http_client.c
 *  @brief Contains implementation of communication between server and client.
 *
 *  A client will have an input buffer and output buffer. When the server
 *  receive data from client, it put data in the input buffer. When server is
 *  ready to write to a client, it takes data from the output buffer.
 *
 *  Two most important function here are doing similar things.
 *  What client_readline do is getting data from the input buffer which is
 *  filled by the server. (The server is feeding client_readline)
 *  What client_write to is putting data in the output buffer and those data is
 *  pending for the server to send.
 *
 *  @author Chao Xin(cxin)
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "log.h"
#include "http_client.h"

/** brief Compare two string(case insensitive) */
int strcicmp(char* s1, char* s2) {
    int len = strlen(s1),
        i;
    char c1, c2;

    if (len != strlen(s2)) return 1;

    for (i = 0; i < len; ++i) {
        c1 = s1[i]; c2 = s2[i];
        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';
        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';
        if (c1 != c2) return 1;
    }

    return 0;
}

/** @brief Destroy a http_header struct */
void deinit_header(http_header_t *header) {
    if (header == NULL) return;
    if (header->key != NULL)
        free(header->key);
    if (header->val != NULL)
        free(header->val);
    free(header);
}

/** @brief Destroy a http_request struct */
void deinit_request(http_request_t *req) {
    http_header_t *ptr, *tmp;

    if (req == NULL) return;

    ptr = req->headers;

    while (ptr != NULL) {
        tmp = ptr->next;
        deinit_header(ptr);
        ptr = tmp;
    }

    /*
     * Don't need to free body. Since it's a pointer to memory in the input
     * buffer of a client.
     */

    free(req);
}

/** @brief Create a new http_request */
http_request_t* new_request() {
    http_request_t *req;

    req = malloc(sizeof(http_request_t));
    req->cnt_headers = 0;
    req->headers = NULL;
    req->body = NULL;

    return req;
}

/** @brief Create a new http client associated with socket fd */
http_client_t* new_client(int fd) {
    http_client_t *client = malloc(sizeof(http_client_t));

    client->fd = fd;
    client->status = C_IDLE;
    client->alive = 1;

    client->in = init_buf();
    client->out = init_buf();

    client->req = new_request();

    client->next = NULL;

    return client;
}

/** @brief Destroy a client struct, free all its resource */
void deinit_client(http_client_t *client) {
    if (client == NULL) return;
    close(client->fd);
    log_msg(L_INFO, "Closed fd %d\n", client->fd);
    deinit_buf(client->in);
    deinit_buf(client->out);
    deinit_request(client->req);
    free(client);
}

/** @brief Write a buffer to client
 *
 *  Copy buf_len bytes from buf to client's output buffer
 *
 *  @param client A pointer to a client struct
 *  @param buf The buffer to be written to client
 *  @param buf_len The length of the buffer
 *  @return Void
 */
void client_write(http_client_t *client, char* buf, int buf_len) {
    buf_t *c_buf = client->out;     //Internal output buffer of client

    //The space is not enough, realloc !
    if (c_buf->datasize + buf_len > c_buf->bufsize) {
        /*
         * An additional BUFSIZE is added to the bufsize to prevent
         * frequent realloc
         */
        c_buf->bufsize = c_buf->datasize + buf_len + BUFSIZE;
        c_buf->buf = realloc(c_buf->buf, c_buf->bufsize);
    }

    memmove(c_buf->buf + c_buf->datasize, buf, buf_len);
    c_buf->datasize += buf_len;
}

/** @brief Write a string to client */
void client_write_string(http_client_t *client, char* str) {
    client_write(client, str, strlen(str));
}

/** @brief Read a line ends in \n from client's input buffer
 *
 *  Find \n started from the internal pointer pos of the client's input buffer
 *  and copy the content between pos pointer and \n(include) to the passed in
 *  buffer. Also update the pointer pos to the character next to '\n'.
 *
 *  @param client A pointer to a client struct
 *  @param line The buffer which stores the line.
 *  @return 1 on success. If no \n is found, return 0. If the length of line
 *  exceed MAX_LINE_LEN, return -1.
 */
int client_readline(http_client_t *client, char *line) {
    buf_t *bp = client->in;
    int end = bp->pos,
        n;

    while (end < bp->datasize) {
        if (end - bp->pos >= MAXBUF)
            return -1;
        if (bp->buf[end] == '\n') {
            n = end - bp->pos;

            /* Deal with \r\n */
            if (n > 0 && bp->buf[end - 1] == '\r')
                n -= 1;

            strncpy(line, bp->buf + bp->pos, n);

            line[n] = '\0';     //End the string

            bp->pos = end + 1;
            return 1;
        }
        end += 1;
    }

    return 0;
}

/** @brief Send the response line to client with status code
 *
 *  @param client The corresponding client
 *  @param code Status code
 */
void send_response_line(http_client_t *client, int code) {
    char* line;

    client_write_string(client, http_version);

    if (code == OK)
        line = " 200 OK";
    if (code == BAD_REQUEST)
        line = " 400 Bad Request";
    if (code == NOT_FOUND)
        line = " 404 Not Found";
    if (code == METHOD_NOT_ALLOWED)
        line = " 405 Method Not Allowed";
    if (code == LENGTH_REQUIRED)
        line = " 411 Length Required";
    if (code == INTERNAL_SERVER_ERROR)
        line = " 500 Internal Server Error";
    if (code == NOT_IMPLEMENTED)
        line = " 501 Not Implemeneted";
    if (code == SERVICE_UNAVAILABLE)
        line = " 503 Service Unavailable";
    if (code == HTTP_VERSION_NOT_SUPPORTED)
        line = " 505 HTTP Version Not Supported";

    client_write_string(client, line);
    client_write_string(client, "\r\n");
}

/** @brief Send the response header */
void send_header(http_client_t *client, char* key, char* val) {
    client_write_string(client, key);
    client_write_string(client, ": ");
    client_write_string(client, val);
    client_write_string(client, "\r\n");
}

//In case of what kind of error should the connection be closed?
static int is_fatal(int code) {
    return code == BAD_REQUEST || code == INTERNAL_SERVER_ERROR;
}

/** @brief Ends current request with given status code and destroy request
 *  object.
 *
 *  @return 0 if the connection should be kept alive. -1 if the connection
 *          should be closed.
 */
int end_request(http_client_t *client, int code) {
    client->status = C_IDLE;
    send_response_line(client, code);

    /* The client signal a "Connection: Close" */
    if (connection_close(client->req))
        client->alive = 0;

    if (is_fatal(code)) {
        send_header(client, "Connection", "Close");
        client_write_string(client, "\r\n");
        client->alive = 0;
        return -1;
    }
    client_write_string(client, "\r\n");

    return 0;
}

/** @brief Retrieve the value of a request header by key
 *
 *  @return The value corresponds to the given key. NULL if not found
 */
char* get_request_header(http_request_t *req, char *key) {
    http_header_t *ptr;

    ptr = req->headers;
    while (ptr) {
        if (strcicmp(ptr->key, key) == 0)
            return ptr->val;
        ptr = ptr->next;
    }

    return NULL;
}


/** @brief Check if the value of Connection header is close
 *
 *  Return 1 if connection is stated "close" in request header. If there's no
 *  "connection" header or connection is stated "keep-alive".
 */
int connection_close(http_request_t *req) {
    char *connection;

    connection = get_request_header(req, "connection");
    if (connection != NULL && strcicmp(connection, "close") == 0)
        return 1;
    return 0;
}
