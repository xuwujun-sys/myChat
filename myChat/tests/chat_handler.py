"""启动聊天服务器后运行：python3 tests/chat_handler.py"""
import socket
import time
import os


def connect():
    sock = socket.create_connection(("127.0.0.1", int(os.environ.get("CHAT_TEST_PORT", "8888"))), timeout=3)
    return sock, sock.makefile("rb")


def expect(stream, line):
    actual = stream.readline()
    assert actual == line, (actual, line)


first, first_in = connect()
second, second_in = connect()
guest, guest_in = connect()
try:
    guest.sendall(b"MSG before login\n")
    expect(guest_in, b"ERROR LOGIN_REQUIRED\n")
    first.sendall(b"LOG")
    second.sendall(b"LOGIN bob\n")
    expect(second_in, b"OK LOGIN\n")
    first.sendall(b"IN alice\n")
    expect(first_in, b"OK LOGIN\n")
    guest.sendall(b"LOGIN alice\n")
    expect(guest_in, b"ERROR NICKNAME_TAKEN\n")
    first.sendall(b"LOGIN another\n")
    expect(first_in, b"ERROR ALREADY_LOGGED_IN\n")

    # 半条消息和粘包；仅已登录的其他用户收到消息。
    first.sendall(b"MSG hel")
    first.sendall(b"lo\nMSG second\n")
    expect(second_in, b"CHAT alice hello\n")
    expect(second_in, b"CHAT alice second\n")
    for sock in (first, guest):
        sock.settimeout(0.15)
        try:
            extra = sock.recv(1)
            raise AssertionError(("unexpected response", extra))
        except socket.timeout:
            pass
        finally:
            sock.settimeout(3)

    second.sendall(b"BAD command\nMSG still here\n")
    expect(second_in, b"ERROR INVALID_COMMAND\n")
    expect(first_in, b"CHAT bob still here\n")
    second_in.close()
    second.close()
    # 断开回调处理完成后昵称可重新使用。
    for _ in range(20):
        guest.sendall(b"LOGIN bob\n")
        line = guest_in.readline()
        if line == b"OK LOGIN\n":
            break
        assert line == b"ERROR NICKNAME_TAKEN\n", line
        time.sleep(0.05)
    else:
        raise AssertionError("disconnected nickname was not released")
    guest.sendall(b"MSG after reconnect\n")
    expect(first_in, b"CHAT bob after reconnect\n")

    # 超长未结束行返回错误并关闭，不再参与群聊。
    guest.sendall(b"MSG " + b"x" * 1025)
    expect(guest_in, b"ERROR MESSAGE_TOO_LONG\n")
    assert guest_in.read(1) == b""
    first.sendall(b"MSG alone\n")
    first.sendall(b"BAD command\n")
    expect(first_in, b"ERROR INVALID_COMMAND\n")
    print("ChatHandler integration checks passed")
finally:
    for stream in (first_in, second_in, guest_in):
        stream.close()
    for sock in (first, second, guest):
        sock.close()
