#! /usr/bin/python
import sys
import os

if __name__ == '__main__':
    print sys.argv
    print os.environ
    content_length = int(os.environ['CONTENT_LENGTH'])
    print content_length
    print sys.stdin.read(content_length)

