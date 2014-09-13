#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "http_client.h"
#include "log.h"

/** @brief Destroy a http_header struct */
void deinit_header(http_header_t *header) {
    if (header->key != NULL)
        free(header->key);
    if (header->val != NULL)
        free(header->val);
}

/** @brief Destroy a http_request struct */
void deinit_request(http_request_t *req) {
    http_header_t *ptr, *tmp;

    ptr = req->headers;

    while (ptr != NULL) {
        tmp = ptr->next;
        deinit_header(ptr);
        ptr = tmp;
    }
}

/** @brief Create a new http_request */
http_request_t* new_request() {
    http_request_t *req;

    req = malloc(sizeof(http_request_t));
    req->headers = NULL;

    return req;
}

/** @brief Create a new http client associated with socket fd */
http_client_t* new_client(int fd) {
    http_client_t *client = malloc(sizeof(http_client_t));

    client->fd = fd;

    client->in = io_init();
    client->out = io_init();

    client->req = NULL;

    client->next = NULL;

    return client;
}

/** @brief Destroy a client struct, free all its resource */
void deinit_client(http_client_t *client) {
    close(client->fd);
    log_msg(L_INFO, "Closed fd %d\n", client->fd);
    io_deinit(client->in);
    free(client->in);
    io_deinit(client->out);
    free(client->out);
    deinit_request(client->req);
    free(client->req);
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
         * An additional INIt_BUFFERSIZE is added to the bufsize to prevent
         * frequent realloc
         */
        c_buf->bufsize = c_buf->datasize + buf_len + INIT_BUFFERSIZE;
        c_buf->buf = realloc(c_buf->buf, c_buf->bufsize);
    }

    memmove(c_buf->buf + c_buf->datasize, buf, buf_len);
    c_buf->datasize += buf_len;
}

/** @brief Read a line ends in \n from client's input buffer
 *
 *  Find \n started from the internal pointer pos of the client's input buffer
 *  and copy the content between pos pointer and \n(include) to the passed in
 *  buffer. Also update the pointer pos to the character next to '\n'.
 *
 *  @param client A pointer to a client struct
 *  @param line The buffer which stores the line.
 *  @return Length of line. If no \n is found, return 0. If the length of line
 *  exceed MAX_LINE_LEN, return -1.
 */
int client_readline(http_client_t *client, char *line) {
    buf_t *bp = client->in;
    int end = bp->pos,
        n;

    while (end < bp->datasize) {
        end += 1;
        if (end - bp->pos >= MAXBUF)
            return -1;
        if (bp->buf[end] == '\n') {
            n = end - bp->pos;

            /* Deal with \r\n */
            if (n > 0 && bp->buf[end - 1] == '\r')
                n -= 1;

            strncpy(line, bp->buf + bp->pos, n);

            line[n] = '\0';     //End the string

            bp->pos = end;
            return end - bp->pos;
        }
    }

    return 0;
}

void send_response_line(http_client_t *client, int code) {
    char* line;

    client_write(client, http_version, strlen(http_version));

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

    client_write(client, line, strlen(line));
    client_write(client, "\r\n", 2);
}