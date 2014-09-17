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
#include "io.h"
#include "log.h"

/** @brief The buffer is full and need to be expand? */
inline int full(buf_t *bp) {
    return bp->datasize + (INIT_BUFFERSIZE >> 1) > bp->bufsize;
}

/** @brief The buffer is empty and should be shrink? */
inline int empty(buf_t *bp) {
    int freespace = bp->bufsize - bp->datasize + bp->pos;
    return freespace > INIT_BUFFERSIZE;
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
 *  Call recv() until connection closed or error occurs. Required non-blocking
 *  socket. When there's temporily no more data, recv will return -1 and set
 *  errno to EWOULDBLOCK or EAGAIN. Each time received data size exceed half
 *  of buffer size, buffer will increase its capacity.
 *
 *  @param bp A pointer to a buf_t struct
 *  @return Number of bytes received on normal exit, 0 on connection closed,
 *          -1 on error.
 */
int io_recv(int sock, buf_t *bp) {
    int nbytes;

    log_msg(L_IO_DEBUG, "recv from %d start.\n", sock);
    while ((nbytes = recv(sock, bp->buf + bp->datasize, bp->bufsize - bp->datasize - 1, 0)) > 0) {
        log_msg(L_IO_DEBUG, "----%d bytes data received.\n", nbytes);
        bp->datasize += nbytes;

        //More data! Allocate more memory
        if (full(bp)) {
            bp->bufsize += bp->bufsize >> 1;
            bp->buf = realloc(bp->buf, bp->bufsize);
        }
    }

    if (nbytes == 0) {
        log_msg(L_IO_DEBUG, "Connection ends.\n");
        return 0;
    }

    if (nbytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        log_error("recv error!");
        return nbytes;
    }

    log_msg(L_IO_DEBUG, "recv complete. %d bytes of data received from socket %d.\n", bp->datasize, sock);

    return bp->datasize;
}

/*
 * io_send - Send data to socket sock
 *
 * Repeatedly call send() to send data in buffer to socket sock until all the
 * data is sent.
 * ----------------------------------------------------------------------------
 * Args:
 *      bp - a pointer to a buf_t struct
 * Returns:
 *      number of bytes sent
 *      -1: error
 */
int io_send(int sock, buf_t *bp) {
    int bytes_sent = 0, nbytes;

    log_msg(L_IO_DEBUG, "Send start! %d bytes data to be sent to socket %d.\n",
            bp->datasize - bp->pos, sock);

    while (bp->pos < bp->datasize) {
        nbytes = send(sock, bp->buf + bp->pos, bp->datasize - bp->pos, 0);

        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else {
                log_error("send error!");
                return -1;
            }
        }

        log_msg(L_IO_DEBUG, "----%d bytes data sent.\n", nbytes);
        bp->pos += nbytes;
        bytes_sent += nbytes;
    }
    log_msg(L_IO_DEBUG, "Send complete. %d bytes of data sent to socket %d\n", bytes_sent, sock);

    //Shrink buffer when there is too much free space in the buffer
    if (empty(bp))
        io_shrink(bp);

    return bytes_sent;
}

/** @brief Init a buf_t struct
 *
 *  @return A pointer to the newly created buf_t struct
 */
buf_t* io_init() {
    buf_t *bp = malloc(sizeof(buf_t));

    bp->bufsize = INIT_BUFFERSIZE;
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
void io_deinit(buf_t *bp) {
    free(bp->buf);
    free(bp);
}
