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


[RUN-3] How to Run
--------------------------------------------------------------------------------

    cd src
    make
    ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder>
            <CGI script path> <private key file> <certificate file>

[IMP-4] Description of Implementation
--------------------------------------------------------------------------------
SIGPIPE is set to be ignored before server starts accepting requests.
All socket will be put into a file descriptor list handled by select().

The server repeatedly call select(). Each time select() returns, the server will
find active file descriptors.

If a new reqeust arrives, the file descriptor list will be updated so that
select can handle the newly created socket. The newly created socket will be set
to be non-blocking using fcntl.

When data arrives at client socket, recv() will be called repeatedly until it
returns 0 or - 1 in order to receive as much data as possible. Since client
sockets are set to be non-blocking, it will return -1 with a errno set to
EWOULDBLOCK or EAGAIN when there is no data availeble.

A specific size of memory will be allocated for buffer used to store received
data at the beginning. And as the data in buffer grows, its capacity will be
increased accordingly(by calling realloc). More specifically, each time data
size exceeds half of buffer capacity, buffer capacity will be doubled.
