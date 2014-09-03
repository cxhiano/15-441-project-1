from socket import *
import sys
import random

if __name__ == '__main__':
    if len(sys.argv) < 4:
        sys.stderr.write("Usage: {0} <host> <port> <number of connections>\n".format(argv[0]))

    host = sys.argv[1]
    port = int(sys.argv[2])
    n_conn = int(sys.argv[3])

    sockets = []
    for i in range(n_conn):
        sockets.append(socket(AF_INET, SOCK_STREAM))
        sockets[-1].connect((host, port))

    close_order = range(n_conn)
    for i in range(n_conn):     #Randomly swap elements in close_order
        x = random.randrange(0, n_conn)
        y = random.randrange(0, n_conn)
        close_order[x], close_order[y] = close_order[y], close_order[x]

    print close_order

    for i in range(n_conn): #Close sockets according to close_order
        n = close_order[i]
        data = str(sockets[n])
        sockets[n].send(data)
        if sockets[n].recv(len(data)) != data:
            print "Failed!"
            exit(1)
        sockets[n].close()

    print "Success!"
