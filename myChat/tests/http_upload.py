"""服务器运行后执行：python3 tests/http_upload.py"""
from pathlib import Path
import socket
import time
import uuid

directory = Path("/home/xuwu/Desktop/httptest")
prefix = "test_upload_" + uuid.uuid4().hex
files = [directory / (prefix + suffix) for suffix in ("_binary", "_empty", "_interrupted")]


def exchange(request):
    with socket.create_connection(("127.0.0.1", 8888), timeout=3) as sock:
        sock.sendall(request)
        result = bytearray()
        while True:
            data = sock.recv(4096)
            if not data:
                return bytes(result)
            result.extend(data)


def check(request, status):
    result = exchange(request)
    assert result.startswith(b"HTTP/1.1 " + status + b" "), result


def request(name, body):
    return ("PUT /files/" + name + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: " +
            str(len(body)) + "\r\n\r\n").encode() + body


try:
    body = bytes(range(256)) * 8192
    check(request(files[0].name, body), b"201")
    assert files[0].read_bytes() == body
    check(request(files[0].name, b"overwrite"), b"409")
    assert files[0].read_bytes() == body
    check(request(files[1].name, b""), b"201")
    assert files[1].read_bytes() == b""
    check(b"PUT /files/%2e%2e%2fx HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", b"400")
    check(b"PUT /files/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n", b"400")
    check(b"PUT /files/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: 536870913\r\n\r\n", b"413")
    check(b"PUT /files/x HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n", b"501")
    check(b"PUT /files/x HTTP/1.1\r\nHost: localhost\r\nContent-Encoding: gzip\r\nContent-Length: 0\r\n\r\n", b"501")
    with socket.create_connection(("127.0.0.1", 8888), timeout=3) as sock:
        sock.sendall(("PUT /files/" + files[2].name + " HTTP/1.1\r\nHost: localhost\r\nContent-Length: 20\r\n\r\npartial").encode())
    time.sleep(0.1)
    assert not files[2].exists()
    print("HTTP upload checks passed")
finally:
    for file in files:
        file.unlink(missing_ok=True)
