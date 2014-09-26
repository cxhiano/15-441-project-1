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

/** @brief Context for using select */
typedef struct {
    fd_set read_fds, read_fds_cpy;
    fd_set write_fds, write_fds_cpy;
    int fd_max;
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

/** @brief A struct for piping content from specific fd
 *
 *  Data in from_fd will be first read into buf, and directly sent out. This
 *  process will be repeated until an error occurs or an EOF is read.
 */
typedef struct {
    int from_fd;
    char buf[BUFSIZE];
    int offset;
    int datasize;
} pipe_t;

/* Init and deinit data structure */
buf_t* init_buf();
void deinit_buf(buf_t *bp);
pipe_t* init_pipe();

/* Monitor dynamic buffer */
int full(buf_t *bp);
int empty(buf_t *bp);
void io_shrink(buf_t *bp);

/* Send/recv with client */
int io_recv(int sock, buf_t *bp);
int io_send(int sock, buf_t *bp);
int io_pipe(int sock, pipe_t *pp);

/* IO context */
int io_select();       // Shorthand for select
void init_io_context();
void add_read_fd(int fd);
void remove_read_fd(int fd);
int test_read_fd(int fd);
void add_write_fd(int fd);
void remove_write_fd(int fd);
int test_write_fd(int fd);

#endif
