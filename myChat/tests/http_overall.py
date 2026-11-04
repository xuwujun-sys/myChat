"""使用 2 秒空闲超时的测试服务器运行：python3 tests/http_overall.py"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import socket
import time
import uuid

directory = Path("/home/xuwu/Desktop/httptest")
prefix = "test_overall_" + uuid.uuid4().hex
files = [directory / (prefix + str(i)) for i in range(4)]


def connect():
    return socket.create_connection(("127.0.0.1", 8888), timeout=6)


def response(sock):
    result = bytearray()
    while True:
        data = sock.recv(65536)
        if not data:
            return bytes(result)
        result.extend(data)


def upload(path):
    body = bytes(range(256)) * 16384
    with connect() as sock:
        sock.sendall(("PUT /files/" + path.name + " HTTP/1.1\r\nHost: test\r\nContent-Length: " +
                      str(len(body)) + "\r\n\r\n").encode())
        sock.sendall(body)
        assert response(sock).startswith(b"HTTP/1.1 201 ")
    assert path.read_bytes() == body


try:
    # 未完成的上传超时后关闭连接，并清除临时文件。
    before = set(directory.glob(".upload-*"))
    with connect() as sock:
        sock.sendall(("PUT /files/" + prefix + "_idle HTTP/1.1\r\nHost: test\r\nContent-Length: 10\r\n\r\nx").encode())
        assert response(sock) == b""
    assert not (set(directory.glob(".upload-*")) - before)
    assert not (directory / (prefix + "_idle")).exists()

    # 持续有进展的连接，即使总时间超过超时值也不应被关闭。
    with connect() as sock:
        for part in (b"GET / HTTP/1.1\r\n", b"Host: ", b"test", b"\r\n\r\n"):
            sock.sendall(part)
            time.sleep(0.75)
        assert response(sock).startswith(b"HTTP/1.1 200 ")

    # 并发上传期间穿插普通请求，检查响应和文件完整性。
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=5) as pool:
        jobs = [pool.submit(upload, path) for path in files]
        with connect() as sock:
            sock.sendall(b"GET / HTTP/1.1\r\nHost: test\r\n\r\n")
            assert response(sock).startswith(b"HTTP/1.1 200 ")
        latency = time.monotonic() - started
        for job in jobs:
            job.result()
    print("HTTP overall checks passed; concurrent request response: %.3f s" % latency)
finally:
    for path in files:
        path.unlink(missing_ok=True)
