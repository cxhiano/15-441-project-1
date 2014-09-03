/*
 * io.c - Provides functions to read data from socket or write data to socket
 */
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "io.h"

/*
 * io_recv - Try to recv as much data as possible
 *
 * Call recv() until connection closed or error occurs. Required non-blocking
 * socket. When there's temporily no more data, recv will return -1 and set
 * errno to EWOULDBLOCK or EAGAIN. Each time received data size exceed half
 * of buffer size, buffer will double its capacity. Return size of received
 * data if no error occurs, 0 if connection has benn closed, -1 on error.
 * ---------------------------------------------------
 *  Args:
 *      bp - a pointer to a buf_t struct
 */
int io_recv(int sock, buf_t *bp) {
    int nbytes;

    fprintf(stderr, "recv from %d start.\n", sock);
    while ((nbytes = recv(sock, bp->buf + bp->datasize, bp->bufsize - bp->datasize - 1, 0)) > 0) {
        fprintf(stderr, "----%d bytes data received.\n", nbytes);
        bp->datasize += nbytes;

        //More data! Allocate more memory
        if (bp->datasize * 2 >= bp->bufsize) {
            bp->bufsize += bp->bufsize >> 1;
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

    fprintf(stderr, "recv complete. %d bytes of data received from socket %d.\n", bp->datasize, sock);

    return bp->datasize;
}

/*
 * io_send - Send data to socket sock
 *
 * Repeatedly call send() to send data in buffer to socket sock until all the
 * data is sent. Return size of sent data if no error occurs, -1 on error.
 * ----------------------------------------------------------------------------
 * Args:
 *      bp - a pointer to a buf_t struct
 */
int io_send(int sock, buf_t *bp) {
    int bytes_sent = 0, nbytes,
        freespace;

    fprintf(stderr, "Send start! %d bytes data to be sent to socket %d.\n",
            bp->datasize - bp->writehead, sock);

    while (bp->writehead < bp->datasize) {
        nbytes = send(sock, bp->buf + bp->writehead, bp->datasize - bp->writehead, 0);

        if (nbytes < 0) {
            perror("send error!");
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                return -1;
        }

        fprintf(stderr, "----%d bytes data sent.\n", nbytes);
        bp->writehead += nbytes;
        bytes_sent += nbytes;
    }
    fprintf(stderr, "Send complete. %d bytes of data sent to socket %d\n", bytes_sent, sock);

    //Shrink buffer when free space in the buffer is to large
    freespace = bp->bufsize - bp->datasize + bp->writehead;
    if (freespace > INIT_BUFFERSIZE)
        io_shrink(bp);

    return bytes_sent;
}

/*
 * io_shrink
 */
void io_shrink(buf_t *bp) {
    int freespace;

    fprintf(stderr, "Start shrinking buffer. bufsize: %d datasize: %d writehead: %d\n",
            bp->bufsize, bp->datasize, bp->writehead);
    freespace = bp->bufsize - bp->datasize + bp->writehead;
    //move data to the head of buffer
    bp->datasize -= bp->writehead;
    memmove(bp->buf, bp->buf + bp->writehead, bp->datasize);
    bp->writehead = 0;
    //cut of half of the free space
    bp->bufsize -= freespace >> 1;

    bp->buf = realloc(bp->buf, bp->bufsize);

    fprintf(stderr, "Shrinking completed. bufsize: %d datasize: %d writehead: %d\n",
            bp->bufsize, bp->datasize, bp->writehead);
}

void io_init(buf_t *bp) {
    bp->bufsize = INIT_BUFFERSIZE;
    bp->datasize = 0;
    bp->writehead = 0;
    bp->buf = malloc(bp->bufsize);
}

void io_deinit(buf_t *bp) {
    free(bp->buf);
}
