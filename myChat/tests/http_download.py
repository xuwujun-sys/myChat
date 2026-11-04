"""从服务器工作目录运行：python3 tests/http_download.py"""
from pathlib import Path
import socket
import uuid
from urllib.parse import quote

# 与 config/config.h 中的 HTTP_FILES_DIR 保持一致。
directory = Path("/home/xuwu/Desktop/httptest")
prefix = "test_" + uuid.uuid4().hex
paths = [directory / (prefix + suffix) for suffix in ("_empty", "_binary", "_large", "_link", "_中文", "_fits", "_over")]


def get(path):
    with socket.create_connection(("127.0.0.1", 8888), timeout=3) as sock:
        sock.sendall(("GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n").encode("ascii"))
        result = bytearray()
        while True:
            data = sock.recv(65536)
            if not data:
                break
            result.extend(data)
    header, body = bytes(result).split(b"\r\n\r\n", 1)
    return header, body


try:
    paths[0].write_bytes(b"")
    contents = bytes(range(256)) * 100
    paths[1].write_bytes(contents)
    paths[2].write_bytes(b"x" * (1024 * 1024))
    paths[3].symlink_to("/etc/passwd")
    paths[4].write_bytes(b"unicode filename")
    limit = 1024 * 1024
    header = b"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n"
    largest = limit - len(header)
    paths[5].write_bytes(b"x" * largest)
    paths[6].write_bytes(b"x" * (largest + 1))
    for path, expected in ((paths[0], b""), (paths[1], contents), (paths[4], b"unicode filename")):
        header, body = get("/files/" + quote(path.name))
        assert header.startswith(b"HTTP/1.1 200 "), header
        assert (b"Content-Length: " + str(len(expected)).encode()) in header, header
        assert body == expected
    for target, status in (("/files/" + prefix + "_missing", b"404"),
                           ("/files/" + paths[3].name, b"404"),
                           ("/files/" + paths[2].name, b"413"),
                           ("/files/" + paths[6].name, b"413"),
                           ("/files/%2e%2e%2fetc%2fpasswd", b"400")):
        header, _ = get(target)
        assert header.startswith(b"HTTP/1.1 " + status + b" "), header
    header, body = get("/files/" + paths[5].name)
    assert header.startswith(b"HTTP/1.1 200 ") and len(body) == largest
    print("HTTP download checks passed")
finally:
    for path in paths:
        try:
            path.unlink()
        except FileNotFoundError:
            pass
