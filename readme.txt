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
        [RUN-2] How to Run
        [CP1-3] Description of Implementation of Checkpoint 1
        [CP2-4] Description of Implementation of Checkpoint 2
        [CP3-5] Description of Implementation of Checkpoint 3

[RUN-2] How to Run
--------------------------------------------------------------------------------

    make clean
    make
    ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder>

[CP1-3] Description of Implementation of Checkpoint 1
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

[CP2-4] Description of Implementation of Checkpoint 2
--------------------------------------------------------------------------------
Firse some changes:
1) The dynamic buffer growing strategy is changed. The old strategy waste a lot
of memory. Now, the capacity of buffer will be increased to 1.5 times its
original capacity when data size is close to buffer capacity.(More specifically,
data + 512 > buffer).

2)A client has two buffers now, one for input one for output.

The HTTP server has a 3-layers:

Layer 0: Server Layer. The server layer interacts with client directly.

When data arrives, the server stores the data in the input buffer associated with
the client and call parser to parse it.

The server also check the output buffer, if it's not empty, send its data to
client.

Source files: io.c server.c
---------------------------

Layer 1: Parser Layer. The parser layer does not interact with client, it gets
data from the input buffer and puts data to the output buffer.

HTTP parser parses data and store useful information in request object. Once
a request has been read completely, it will call function in the Handler Layer
to handle the request. The parser maintains status so that it can continue where
it left last time when it's called.

The status of parser includes:
1. The position of the beginning of un-processed data in the input buffer.
2. What's coming next? Request Line? Request Header? Request Body?

This layer also provides function to send response headers(What it actually
does is putting the string to the output buffer and let Server Layer to send
it to client).

Source files: http_client.c http_parser.c
---------------------------

Layer 2: Handler Layer. This layer response to HTTP GET, POST, HEAD requests.

Handlers in this layer will be called when a complete HTTP request is read. A
handler use the information provided by parser layer which is a request
object. And it also call parser layer function to send response to client. (
Including headers, status code)

Source file: request_handler.c

[CP3-5] Description of Implementation of Checkpoint 3
--------------------------------------------------------------------------------
1. Client status
Maintain status for each client, indicating the current action of the server.
There are 4 status:

C_IDLE          There's no action now.
C_PHEADER       The server is now parsing request headers.
C_PBODY         The server is now receiving request body.
C_PIPING        The server is sending response back to this client.
                See 2. Pipe mechanism for more detail.

The server decides what to do according to the status. For example, when parsing
http request, the server interprets data depending on the status. The server
maintains the status so that the parsing process can be complete in several runs

2. Pipe Mechanism
To avoid ambiguity, the pipe() in <unistd.h> is referred as UNIX pipe. And the
Pipe describe below is referred as pipe.

When sending files and output of a cgi script to client, instead of reading all
content into a buffer, the server setups a pipe to the client socket. This type
of pipe consists of a file descriptor indicating the source of the pipe and an
internal buffer serving as a medium between the source and the server socket.
The server tries to fill the internal buffer with bytes from source fd and send
those bytes to client sockets. This process is repeated until all bytes are
sent. This mechanism helps the server to reduce memory consuming when sending
huge content to client.

Now there're 2 ways to send bytes to client. The response headers is sent
through output buffer associated with each client. The response content is sent
through the pipe described above. The C_PIPING status code is employed to ensure
the order of the output. When a client is in C_PIPING status, the piping will
not begin until the output buffer is empty. This ensures the headers are sent
before the content. And C_PIPING also prevents furthur request handling. This
ensures the headers of next request are sent after current content is sent.

3. Daemonize
1). Call fork() to create child process.
2). Parent process exit.
3). Child process call setsid() to detach from parent process group and create
    a new session.
4). Child process call umask() to set process file mode.
5). Child process tests and locks lock file. This ensure one daemon per lock
    file.
6). Child process closes all file descriptors except the lock file descriptor.
7). Setup signal handling and log file.

The daemon server can be terminated by a SIGTERM. The handler of SIGTERM will
set a flag to inform the server to stop. The server will check the flag each
time before it select().

4. CGI Implementation
The server folks out a child process to run the cgi script. Input and output
are handle using UNIX pipe() (Different from 2. Pipe mechanism). After creating
child process, the server sends request body to stdin of child process. The
child process is not allowed to block the stdin or it will be killed and a 500
will be sent to client. A pipe(see 2. Pipe mechanism) is setup for delivering
the output of child process to client.

5. Process Management
The parent process setups SIGCHLD handler. When a child process dies, waitpid()
will be called to reap it.

6. SSL
The implementation of SSL server-side is easy. Each client is associated with
its own SSL context. And if the client socket is not ssl, this context is
NULL. Thus the server can determine the type of the connection of a client
and call corresponding method to communicate with the client.
