from socket import *
import sys
import random

def randdata(dataSize):
    ret = []
    for i in range(dataSize):
        ret.append(chr(random.randrange(0, 256)))
    return ''.join(ret)

def test(host, port, dataSize):
    s = socket(AF_INET, SOCK_STREAM)
    s.connect((host, port))

    data = randdata(dataSize)
    s.send(data)

    close = random.randrange(0, 2)
    if close == 0:
        s.close()
    else:
        recv_data = s.recv(random.randrange(0, dataSize + 1))
        print "recv {0} bytes data".format(len(recv_data))

    del s

    s = socket(AF_INET, SOCK_STREAM)
    s.connect((host, port))

    data = randdata(dataSize)
    s.send(data)

    recv_data = ""
    while len(recv_data) < dataSize:
        recv_data = recv_data + s.recv(dataSize - len(recv_data))

    if recv_data == data:
        print "Success!"
        return False
    else:
        print "Failed"
        return True

if __name__ == '__main__':
    if len(sys.argv) < 5:
        sys.stderr.write('Usage: {0} <host> <port> <data size> <trials>\n'.format(sys.argv[0]))
        exit(0)

    serverHost = sys.argv[1]
    serverPort = int(sys.argv[2])
    dataSize = int(sys.argv[3])
    trials = int(sys.argv[4])

    for i in range(trials):
        if test(serverHost, serverPort, dataSize):
            break
