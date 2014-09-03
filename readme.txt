################################################################################
# readme.txt                                                                   #
#                                                                              #
# Description: Documentation for Liso server Implementation                    #
#                                                                              #
# Author: Chao Xin                                                             #
# Andrew ID: cxin                                                              #
#                                                                              #
################################################################################




[TOC-1] Table of Contents
--------------------------------------------------------------------------------

        [TOC-1] Table of Contents
        [DES-2] Description of Files
        [RUN-3] How to Run
        [IMP-4] Description of Implementation

[DES-2] Description of Files
--------------------------------------------------------------------------------

                    .../readme.txt                  - Current document
                    .../tests.txt                   - Description of test cases
                    .../vulnerabilities.txt         - Documentation of
                                                      vulnerabilities

                    .../src/Makefile                - Contains rules for make
                    .../src/lisod.c                 - Liso server laucher
                    .../src/config.h                - Definition of global vars
                    .../src/server.h                - Header file for server.c
                    .../src/server.c                - Implementation of server
                    .../src/io.h                    - Header file for io.c
                    .../src/io.c                    - Socket IO functions

                    .../test/cp1_checker.py         - Python test script for CP1
                    .../test/linked_list.py         - Test linked list
                                                      Implementation
                    .../test/large_data.py          - Test server with large
                                                      data


[RUN-3] How to Run
--------------------------------------------------------------------------------

    cd src
    make
    ./lisod <HTTP port>

[IMP-4] Description of Implementation
--------------------------------------------------------------------------------
SIGPIPE is set to be ignored before server starts accepting requests.

All client sockets are maintained using linked list and each client socket is
associated with a buffer to stored received data. Also, all client sockets
will be made non-blocking using fcntl.

select() is used to implement a concurrent server. The server repeatedly call
select(). Each time select() returns, the server will check server socket as
well as go through the linked list to find out active sockets.

When data arrives at client socket, recv() will be called repeatedly until it
returns 0 or - 1 in order to receive as much data as possible. Since client
sockets are set to be non-blocking, it will return -1 with errno set to
EWOULDBLOCK or EAGAIN when there is no data availeble.

Data sending is handle similarly. send() will be called repeatedly until all
data is sent or it returns -1 with errno set to EWOULDBLOCK or EAGAIN.

When error is encountered during send() and recv(), corresponding client socket
will be closed.

The memory buffer associated with client socket is dynamically allocated. It
starts at a specific size and as data in it grows, it grows accordingly. (by
calling realloc). More specifically, each time data size exceeds half of buffer
capacity, buffer capacity will be increased to 1.5 times its original capacity.

The memory buffers will also shrink. Each time after sending data, if free
space in the buffer is more than the initial size of the buffer, half of free
space in the buffer will be freed.
