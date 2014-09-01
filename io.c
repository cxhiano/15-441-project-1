#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include "io.h"

int io_recvall(int sock, buf_t *bp) {
    int nbytes;

    while ((nbytes = recv(sock, bp->buf + bp->datasize, bp->bufsize - bp->datasize - 1, 0)) > 0) {
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

    if (nbytes < 0) {
        perror("recv error!");
        return nbytes;
    }

    return bp->datasize;
}

int io_send(int sock, buf_t *bp) {
    int bytes_sent = 0, nbytes;

    while (bytes_sent < bp->datasize) {
        nbytes = send(sock, bp->buf + bytes_sent, bp->datasize - bytes_sent, 0);
        if (nbytes < 0) {
            perror("send error!");
            return nbytes;
        }

        bytes_sent += nbytes;
    }

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