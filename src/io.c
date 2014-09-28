/** @file io.c
 *  @brief Provides functions to read data from socket or write data to socket
 *
 *  The function take a greedy approach, that is, send and receive as much bytes
 *  as possible in one call. When receiving, the buffer size will increase
 *  dynamically. When sending, the buffer size might shrink when it's empty
 *  enough.
 *
 *  @author Chao Xin(cxin)
 */
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "io.h"
#include "log.h"

static select_context context;

/** @brief The buffer is full and need to be expand? */
inline int full(buf_t *bp) {
    return bp->datasize + (BUFSIZE >> 1) > bp->bufsize;
}

/** @brief The buffer is empty and should be shrink? */
inline int empty(buf_t *bp) {
    int freespace = bp->bufsize - bp->datasize + bp->pos;
    return freespace > BUFSIZE;
}

/** @brief Shrink buffer size
 *
 *  Move data started at pointer bp->pos to the head of buffer and then deduce
 *  the free memory by half.
 *
 *  @param bp The buffer to be shrunk
 *
 *  @return Void
 */
void io_shrink(buf_t *bp) {
    int freespace;

    log_msg(L_IO_DEBUG, "Start shrinking buffer. bufsize: %d datasize: %d pos: %d\n",
            bp->bufsize, bp->datasize, bp->pos);

    //calculate free memory
    freespace = bp->bufsize - bp->datasize + bp->pos;
    //move data to the head of buffer
    bp->datasize -= bp->pos;
    memmove(bp->buf, bp->buf + bp->pos, bp->datasize);
    bp->pos = 0;
    //cut of half of the free space
    bp->bufsize -= freespace >> 1;

    bp->buf = realloc(bp->buf, bp->bufsize);

    log_msg(L_IO_DEBUG, "Shrinking completed. bufsize: %d datasize: %d pos: %d\n",
            bp->bufsize, bp->datasize, bp->pos);
}

/** @brief Try to recv as much data as possible
 *
 *  Call recv()/SSL_read() until connection closed or error occurs.
 *
 *  @param sock Client socket
 *  @param bp A pointer to a buf_t struct which stores received data
 *  @param ssl_context If ssl_context if not NULL, SSL_read() will be used
 *                     instead of recv().
 *  @return Number of bytes received on normal exit, 0 on connection closed,
 *          -1 on error.
 */
int io_recv(int sock, buf_t *bp, SSL* ssl_context) {
    int nbytes;

    if (ssl_context)
        nbytes = SSL_read(ssl_context, bp->buf + bp->datasize,
                          bp->bufsize - bp->datasize - 1);
    else
        nbytes = recv(sock, bp->buf + bp->datasize,
                      bp->bufsize - bp->datasize - 1, 0);
    if (nbytes > 0) {
        log_msg(L_IO_DEBUG, "io_recv: %d bytes data received.\n", nbytes);
        bp->datasize += nbytes;

        // Allocate more memory
        if (full(bp)) {
            bp->bufsize += bp->bufsize >> 1;
            bp->buf = realloc(bp->buf, bp->bufsize);
        }
    }

    if (nbytes < 0)
        log_error("io_recv error");

    return nbytes;
}

/** @brief Send data to socket sock
 *
 *  Call send()/SSL_write() to send data in buffer to socket sock
 *
 *  @param sock Client socket
 *  @param bp A pointer to a buf_t struct which store data to be sent
 *  @param ssl_context If ssl_context if not NULL, SSL_write() will be used
 *                     instead of send().
 *  @return Number of bytes sent, -1 on error
 */
int io_send(int sock, buf_t *bp, SSL* ssl_context) {
    int nbytes;

    if (bp->pos < bp->datasize) {
        if (ssl_context)
            nbytes = SSL_write(ssl_context, bp->buf + bp->pos, bp->datasize - bp->pos);
        else
            nbytes = send(sock, bp->buf + bp->pos, bp->datasize - bp->pos, 0);

        if (nbytes <= 0) {
            log_error("io_send error");
            return -1;
        }

        log_msg(L_IO_DEBUG, "io_send: %d bytes sent.\n", nbytes);
        bp->pos += nbytes;
    }

    //Shrink buffer when there is too much free space in the buffer
    if (empty(bp))
        io_shrink(bp);

    return nbytes;
}

/** @brief Pipe content directly to client socket without reading it extirely
 *         into buffer
 *
 *  If buf in pipe is not empty, send data in buf to socket sock. After the buf
 *  becomes empty, refill it using data read from fd associated with the pipe.
 *
 *  @param sock Client socket
 *  @param pp The pointer to a pipe to the file client requested or a cgi
 *            script process output.
 *  @param ssl_context If ssl_context if not NULL, SSL_write() will be used
 *                     instead of send().
 *  @return 1 piping complete. 0 to be continued. -1 error.
 */
int io_pipe(int sock, pipe_t *pp, SSL *ssl_context) {
    int n;

    if (pp->datasize <= pp->offset) { // No data in buf
        pp->datasize = read(pp->from_fd, pp->buf, BUFSIZE); // Get new data
        if (pp->datasize == -1) {
            close(pp->from_fd);
            remove_read_fd(pp->from_fd);
            log_error("io_pipe read error");
            return -1;
        }
        if (pp->datasize == 0) { // Got EOF. Piping completed
            close(pp->from_fd);
            remove_read_fd(pp->from_fd);
            return 1;
        }
        pp->offset = 0;
    }

    // Send to client
    if (ssl_context)
        n = SSL_write(ssl_context, pp->buf + pp->offset,
                      pp->datasize - pp->offset);
    else
        n = send(sock, pp->buf + pp->offset, pp->datasize - pp->offset, 0);

    if (n == -1) {
        close(pp->from_fd);
        remove_read_fd(pp->from_fd);
        log_error("io_pipe send error");
        return -1;
    }
    log_msg(L_IO_DEBUG, "io_pipe: %d bytes sent.\n", n);
    pp->offset += n;

    return 0;
}

/** @brief Init a pipe_t struct
 *
 *  @return A pointer to the newly created pipe_t struct
 */
pipe_t* init_pipe() {
    pipe_t *pp = malloc(sizeof(pipe_t));

    pp->offset = 0;
    pp->datasize = 0;
    return pp;
}

/** @brief Init a buf_t struct
 *
 *  @return A pointer to the newly created buf_t struct
 */
buf_t* init_buf() {
    buf_t *bp = malloc(sizeof(buf_t));

    bp->bufsize = BUFSIZE;
    bp->datasize = 0;
    bp->pos = 0;
    bp->buf = malloc(bp->bufsize);

    return bp;
}

/** @brief Destroy a buf_t struct, free allocated memory
 *
 *  @param bp A buf_t struct
 *  @return Void
 */
void deinit_buf(buf_t *bp) {
    free(bp->buf);
    free(bp);
}

void init_select_context() {
    FD_ZERO(&context.read_fds);
    FD_ZERO(&context.read_fds_cpy);
    FD_ZERO(&context.write_fds);
    FD_ZERO(&context.write_fds_cpy);
}

void add_read_fd(int fd) {
    FD_SET(fd, &context.read_fds_cpy);
    if (fd > context.fd_max)
        context.fd_max = fd;
}

void remove_read_fd(int fd) {
    FD_CLR(fd, &context.read_fds_cpy);
}

int test_read_fd(int fd) {
    return FD_ISSET(fd, &context.read_fds);
}

void add_write_fd(int fd) {
    FD_SET(fd, &context.write_fds_cpy);
    if (fd > context.fd_max)
        context.fd_max = fd;
}

void remove_write_fd(int fd) {
    FD_CLR(fd, &context.write_fds_cpy);
}

int test_write_fd(int fd) {
    return FD_ISSET(fd, &context.write_fds);
}

/** @brief Wrapper for select()
 *
 *  @return What select() returns
 */
int io_select() {
    context.read_fds = context.read_fds_cpy;
    context.write_fds = context.write_fds_cpy;
    return select(context.fd_max + 1, &context.read_fds, &context.write_fds,
        NULL, NULL);
}
