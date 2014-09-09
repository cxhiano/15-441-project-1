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
    void* buf;          //Memory allocated to this buffer
    int bufsize;        //Size of allocated memory
    int datasize;       //Size of data store in memory
    int writehead;      //The first character to be written out
} buf_t;

void io_init(buf_t *bp);
void io_deinit(buf_t *bp);

int io_recv(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);

#endif