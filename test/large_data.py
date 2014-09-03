from socket import *
import sys
import random

def randdata(dataSize):
    ret = []
    for i in range(dataSize):
        ret.append(chr(random.randrange(0, 256)))
    return ''.join(ret)

def correct(host, port, dataSize):
    s = socket(AF_INET, SOCK_STREAM)
    s.connect((host, port))

    data = randdata(dataSize)
    s.send(data)

    recv_data = ""
    while len(recv_data) < dataSize: #Receive all data
        recv_data = recv_data + s.recv(dataSize - len(recv_data))

    if recv_data == data:
        return True
    else:
        return False

def test(host, port, dataSize):
    s = socket(AF_INET, SOCK_STREAM)
    s.connect((host, port))

    data = randdata(dataSize)
    s.send(data)

    close = random.randrange(0, 2)
    if close == 0:
        s.close()
        print "Connection closed"
    else:
        #Receive random amount of data
        recv_data = s.recv(random.randrange(0, dataSize + 1))
        print "recv {0} bytes data".format(len(recv_data))

    del s

    if correct(host, port, dataSize):
        return False
    else:
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
            print "Failed"
            exit(1)
    print "Success"
