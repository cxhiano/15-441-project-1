/** @file io.h
 *  @brief Header file for io.c
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __MYIO_H__
#define __MYIO_H__

/*
 * Initial buffer size
 */
#define INIT_BUFFERSIZE 1024

/** @brief A dynamic size buffer */
typedef struct {
    char* buf;          //!<Memory allocated to this buffer
    int bufsize;        //!<Size of allocated memory
    int datasize;       //!<Size of data store in memory
    /**
     * An internal pointer for user, usually points to the head of unprocessed
     * data. For an output buffer, pos points to the first byte which has not
     * been sent
     */
    int pos;
} buf_t;

buf_t* io_init();
void io_deinit(buf_t *bp);

int full(buf_t *bp);
int empty(buf_t *bp);
void io_shrink(buf_t *bp);

int io_recv(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);

#endif