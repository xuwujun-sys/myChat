"""服务器启动后执行：python3 tests/http_streaming.py"""
from pathlib import Path
import socket
import time
import uuid

directory = Path("/home/xuwu/Desktop/httptest")
name = "test_stream_" + uuid.uuid4().hex
target = directory / name
body = bytes(range(256)) * 12288  # 3 MiB


def connection():
    return socket.create_connection(("127.0.0.1", 8888), timeout=10)


def receive(sock):
    response = bytearray()
    while True:
        data = sock.recv(65536)
        if not data:
            return bytes(response)
        response.extend(data)


try:
    with connection() as sock:
        header = f"PUT /files/{name} HTTP/1.1\r\nHost: localhost\r\nContent-Length: {len(body)}\r\n\r\n".encode()
        sock.sendall(header + body[:100])
        for offset in range(100, len(body), 32768):
            sock.sendall(body[offset:offset + 32768])
        assert receive(sock).startswith(b"HTTP/1.1 201 ")
    assert target.read_bytes() == body

    with connection() as sock:
        sock.sendall(f"GET /files/{name} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode())
        sock.shutdown(socket.SHUT_WR)
        time.sleep(0.2)  # 暂时不读取，模拟慢速客户端。
        response = receive(sock)
        header, downloaded = response.split(b"\r\n\r\n", 1)
        assert header.startswith(b"HTTP/1.1 200 ") and downloaded == body

    before = set(directory.glob(".upload-*"))
    with connection() as sock:
        sock.sendall(b"PUT /files/interrupted-test HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\npartial")
    time.sleep(0.2)
    assert set(directory.glob(".upload-*")) == before
    print("Large file, slow download and interrupted upload checks passed")
finally:
    target.unlink(missing_ok=True)
