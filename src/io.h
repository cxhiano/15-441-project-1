/** @file io.h
 *  @brief Header file for io.c
 *
 *  @author Chao Xin(cxin)
 */
#ifndef __MYIO_H__
#define __MYIO_H__

#include <unistd.h>

/*
 * Initial buffer size
 */
#define BUFSIZE 1024

/** @brief
 *
 */
typedef struct {

} io_context;

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

/** @brief
 *
 */
typedef struct {
    int from_fd;
    char buf[BUFSIZE];
    int offset;
    int datasize;
} pipe_t;

buf_t* init_buf();
void deinit_buf(buf_t *bp);
pipe_t* init_pipe();

int full(buf_t *bp);
int empty(buf_t *bp);
void io_shrink(buf_t *bp);

int io_recv(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);
int io_pipe(int sock, pipe_t *pp);

#endif
