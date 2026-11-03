"""启动服务器后运行：python3 tests/http_requests.py"""
import socket
import time

ADDRESS = ("127.0.0.1", 8888)


def connect():
    return socket.create_connection(ADDRESS, timeout=2)


def response(sock):
    result = bytearray()
    while True:
        data = sock.recv(4096)
        if not data:
            return bytes(result)
        result.extend(data)


def check(request, status):
    with connect() as sock:
        sock.sendall(request)
        result = response(sock)
    assert result.startswith(b"HTTP/1.1 " + status + b" "), result
    assert b"Connection: close\r\n" in result, result


with connect() as first, connect() as second:
    first.sendall(b"GET /one HTTP/1.1\r\nHo")
    second.sendall(b"GET /two HTTP/1.1\r\nHost: test\r\n\r\n")
    assert response(second).endswith(b"Hello, HTTP!\n")
    first.sendall(b"st: test\r\n\r\n")
    assert response(first).endswith(b"Hello, HTTP!\n")

with connect() as sock:
    sock.sendall(b"PUT /x HTTP/1.1\r\nHost: test\r\nContent-Length: 5\r\n\r\nab")
    time.sleep(0.05)
    sock.sendall(b"c\x00d")
    assert response(sock).startswith(b"HTTP/1.1 200 ")

check(b"GET / HTTP/1.1\r\n\r\n", b"400")
check(b"GET / HTTP/1.1\r\nHost: test\r\nContent-Length: -1\r\n\r\n", b"400")
check(b"GET / HTTP/1.1\r\nHost: test\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n", b"400")
check(b"GET / HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n", b"501")
check(b"PUT / HTTP/1.1\r\nHost: test\r\n\r\n", b"411")
check(b"GET / HTTP/1.1\r\nHost: test\r\nX: " + b"a" * 8192 + b"\r\n\r\n", b"431")
print("HTTP request checks passed")
