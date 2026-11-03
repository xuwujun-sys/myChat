"""服务器运行后执行：python3 tests/http_streaming.py"""
import json
from pathlib import Path
import socket
import time
import uuid
from urllib.parse import quote

directory = Path('/home/xuwu/Desktop/httptest')
name = 'stream_' + uuid.uuid4().hex + '_中文".bin'
path = directory / name
partial = directory / ('partial_' + uuid.uuid4().hex)
data = bytes(range(256)) * (3 * 1024 * 1024 // 256)


def request(raw):
    with socket.create_connection(('127.0.0.1', 8888), timeout=5) as sock:
        sock.sendall(raw)
        output = bytearray()
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                return bytes(output)
            output.extend(chunk)


def get(target):
    return request(('GET ' + target + ' HTTP/1.1\r\nHost: localhost\r\n\r\n').encode())


try:
    encoded = quote(name)
    header = ('PUT /files/' + encoded + ' HTTP/1.1\r\nHost: localhost\r\nContent-Length: ' + str(len(data)) + '\r\n\r\n').encode()
    with socket.create_connection(('127.0.0.1', 8888), timeout=5) as sock:
        sock.sendall(header)
        for offset in range(0, len(data), 16000):
            sock.sendall(data[offset:offset + 16000])
        assert sock.recv(1024).startswith(b'HTTP/1.1 201 ')
    assert path.read_bytes() == data
    page = get('/')
    assert b'HTTP/1.1 200 ' in page and b'<!doctype html>' in page
    response = get('/api/files')
    entries = json.loads(response.split(b'\r\n\r\n', 1)[1])
    assert any(item['name'] == name and item['size'] == len(data) for item in entries)
    with socket.create_connection(('127.0.0.1', 8888), timeout=10) as slow:
        slow.sendall(('GET /files/' + encoded + ' HTTP/1.1\r\nHost: localhost\r\n\r\n').encode())
        time.sleep(0.2)
        assert get('/api/files').startswith(b'HTTP/1.1 200 ')
        response = bytearray()
        while True:
            chunk = slow.recv(32768)
            if not chunk:
                break
            response.extend(chunk)
        assert bytes(response).split(b'\r\n\r\n', 1)[1] == data
    before = set(directory.glob('.upload-*'))
    with socket.create_connection(('127.0.0.1', 8888), timeout=5) as sock:
        sock.sendall(('PUT /files/' + partial.name + ' HTTP/1.1\r\nHost: localhost\r\nContent-Length: 500000\r\n\r\npartial').encode())
    time.sleep(0.2)
    assert not partial.exists() and set(directory.glob('.upload-*')) == before
    with socket.create_connection(('127.0.0.1', 8888), timeout=40) as sock:
        sock.sendall(('PUT /files/' + partial.name + ' HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nx').encode())
        time.sleep(32)
        assert sock.recv(1) == b''
    assert not partial.exists() and set(directory.glob('.upload-*')) == before
    print('Streaming, concurrency, listing, interruption and idle timeout checks passed')
finally:
    path.unlink(missing_ok=True)
    partial.unlink(missing_ok=True)
