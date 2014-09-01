/*
 * io.c - Provides functions to read data from socket or write data to socket
 */
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include "io.h"

/*
 * io_recvall - Try to recv as much data as possible
 *
 * Call recv() until connection closed or error occurs. Required non-blocking
 * socket. When there's temporily no more data, recv will return -1 and set
 * errno to EWOULDBLOCK or EAGAIN. Each time received data size exceed half
 * of buffer size, buffer will double its capacity. Return size of received
 * data if no error occurs, 0 if connection has benn closed, -1 on error.
 * ---------------------------------------------------
 *  Args:
 *      sock - a non-blocking socket
 *      bp - a pointer to an initialized buf_t object
 */
int io_recvall(int sock, buf_t *bp) {
    int nbytes;

    fprintf(stderr, "recv start.\n");
    while ((nbytes = recv(sock, bp->buf + bp->datasize, bp->bufsize - bp->datasize - 1, 0)) > 0) {
        fprintf(stderr, "----%d bytes data received.\n", nbytes);
        bp->datasize += nbytes;

        //More data! Allocate more memory
        if (bp->datasize * 2 >= bp->bufsize) {
            bp->bufsize *= 2;
            bp->buf = realloc(bp->buf, bp->bufsize);
        }
    }

    if (nbytes == 0) {
        fprintf(stderr, "Connection ends.\n");
        return 0;
    }

    if (nbytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recv error!");
        return nbytes;
    }

    fprintf(stderr, "recv complete. %d bytes of data received.\n", bp->datasize);

    return bp->datasize;
}

/*
 * io_send - Send data to socket sock
 *
 * Repeatedly call send() to send data in buffer to socket sock until all the
 * data is sent. Return size of sent data if no error occurs, -1 on error.
 * ----------------------------------------------------------------------------
 * Args:
 *      sock - socket number
 *      bp - a pointer to a buf_t struct
 */
int io_send(int sock, buf_t *bp) {
    int bytes_sent = 0, nbytes;

    fprintf(stderr, "Send start! %d bytes data to be sent.\n", bp->datasize);
    while (bytes_sent < bp->datasize) {
        nbytes = send(sock, bp->buf + bytes_sent, bp->datasize - bytes_sent, 0);

        if (nbytes < 0) {
            perror("send error!");
            return -1;
        }

        fprintf(stderr, "----%d bytes data sent.\n", bp->datasize);
        bytes_sent += nbytes;
    }
    fprintf(stderr, "Send complete. %d bytes of data sent\n", bytes_sent);

    return bytes_sent;
}

void io_init(buf_t *bp) {
    bp->bufsize = INIT_BUFFERSIZE;
    bp->datasize = 0;
    bp->buf = malloc(bp->bufsize);
}

void io_deinit(buf_t *bp) {
    free(bp->buf);
}