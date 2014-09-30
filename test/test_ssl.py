import ssl
import socket
import sys

if __name__ == '__main__':
    sock = socket.create_connection((sys.argv[1], sys.argv[2]))
    tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                                ca_certs='../server.crt',
                                ssl_version=ssl.PROTOCOL_TLSv1)


    tls.sendall('post /cgi/echo.py http/1.1\r\ncontent-length:10\r\n\r\n1234567890')
    print tls.recv(4096)
    tls.sendall('get /style.css http/1.1\r\n\r\n')
    print tls.recv(4096)
    tls.close()
