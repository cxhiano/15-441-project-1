#ifndef __MYIO_H__
#define __MYIO_H__

#define INIT_BUFFERSIZE 1024    //Initial buffer size

typedef struct {
    void* buf;
    int bufsize;
    int datasize;
    int writehead;
} buf_t;

void io_init(buf_t *bp);
void io_deinit(buf_t *bp);
void io_shrink(buf_t *bp);

int io_recv(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);

#endif