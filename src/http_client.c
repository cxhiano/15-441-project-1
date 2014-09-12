#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "http_client.h"
#include "log.h"

/** @brief Create a new http client associated with socket fd */
client_list_t* new_client(int fd) {
    client_list_t *client = malloc(sizeof(client_list_t));

    client->fd = fd;
    client->in = malloc(sizeof(buf_t));
    io_init(client->in);
    client->out = malloc(sizeof(buf_t));
    io_init(client->out);
    client->next = NULL;

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