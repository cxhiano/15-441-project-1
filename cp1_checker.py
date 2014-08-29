#!/usr/bin/python

from socket import *
import sys
import random
import os

if len(sys.argv) < 7:
	sys.stderr.write('Usage: %s <ip> <port> <#trials> <#writes and reads per trial> <max # bytes to write at a time> <#connections> \n' % (sys.argv[0]))
	sys.exit(1)

serverHost = gethostbyname(sys.argv[1].strip())
serverPort = int(sys.argv[2])
numTrials = int(sys.argv[3])
numWritesReads = int(sys.argv[4])
numBytes = int(sys.argv[5])
numConnections = int(sys.argv[6])

socketList = []


for i in xrange(numConnections):
	s = socket(AF_INET, SOCK_STREAM)	
	s.connect((serverHost, serverPort))
	socketList.append(s)


for i in xrange(numTrials):
	socketSubset = []
	randomData = []
	randomLen = []
	socketSubset = random.sample(socketList, numConnections)
	for j in xrange(numWritesReads):
			random_len = random.randrange(1, numBytes)
			random_string = os.urandom(random_len)

                        # include num_bytes\n at the front
			randomData.append(str(random_len)+"\n"+random_string) 
			randomLen.append(random_len+len(str(random_len))+1) 
			socketSubset[j].send(randomData[j])

	for j in xrange(numWritesReads):
			data = socketSubset[j].recv(randomLen[j])
			if(data != randomData[j]):
				sys.stderr.write("Error: Data received is not the same as sent! \n")
				sys.exit(1)
				

for i in xrange(numConnections):
	socketList[i].close()

print "Success!"
