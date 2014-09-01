#ifndef __MYIO_H__
#define __MYIO_H__

#define INIT_BUFFERSIZE 1024

typedef struct {
    void* buf;
    int bufsize;
    int datasize;
} buf_t;

void io_init(buf_t *bp);
void io_deinit(buf_t *bp);

int io_recvall(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);

#endif