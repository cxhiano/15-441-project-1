#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "http_client.h"
#include "log.h"

/** @brief Destroy a http_header struct */
void deinit_header(http_header_t *header) {
    if (req->key != NULL)
        free(req->key);
    if (req->val != NULL)
        free(req->val);
}

/** @brief Destroy a http_request struct */
void deinit_request(http_request_t *req) {
    http_header_t *ptr, *tmp;

    if (req->method != NULL)
        free(req->method);
    if (req->uri != NULL)
        free(req->uri);
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
    req->method = req->uri = req->headers = NULL;

    return req;
}

/** @brief Create a new http client associated with socket fd */
client_list_t* new_client(int fd) {
    client_list_t *client = malloc(sizeof(client_list_t));

    client->fd = fd;
    client->in = malloc(sizeof(buf_t));
    io_init(client->in);
    client->out = malloc(sizeof(buf_t));
    io_init(client->out);
    client->next = NULL;

    client->req = new_request();

    return client;
}

/** @brief Destroy a client struct, free all its resource */
void deinit_client(client_list_t *client) {
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
void write_client(client_list_t *client, char* buf, int buf_len) {
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
int client_readline(client_list_t *client, char *line) {
    but_t *bp = client->in;
    int end = bp->pos;

    while (end < bp->datasize) {
        end += 1;
        if (end - bp->pos >= MAXLINE)
            return -1;
        if (bp->buf[end] == '\n') {
            strcpy(line, bp->buf + bp->pos, end - bp->pos);
            bp->pos = end;
            return end - bp->pos;
        }
    }

    return 0;
}