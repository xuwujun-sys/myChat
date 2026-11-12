"""仅对隔离测试数据库运行：python3 tests/chat_handler.py（会创建账号）。"""
import socket
import time
import os
import uuid


def connect():
    sock = socket.create_connection(("127.0.0.1", int(os.environ.get("CHAT_TEST_PORT", "8888"))), timeout=3)
    return sock, sock.makefile("rb")


def expect(stream, line):
    actual = stream.readline()
    assert actual == line, (actual, line)


def expect_users(stream, *names):
    line = stream.readline()
    parts = line.rstrip(b"\n").split(b" ")
    assert line.endswith(b"\n") and parts[0] == b"USERS", line
    assert len(parts) - 1 == len(names) and set(parts[1:]) == set(names), line


first, first_in = connect()
second, second_in = connect()
guest, guest_in = connect()
suffix = uuid.uuid4().hex[:12].encode()
alice = b"a" + suffix
bob = b"b" + suffix
try:
    guest.sendall(b"MSG before login\n")
    expect(guest_in, b"ERROR LOGIN_REQUIRED\n")
    first.sendall(b"REGISTER " + alice + b" pass\n")
    expect(first_in, b"OK REGISTER\n")
    second.sendall(b"REGISTER " + bob + b" pass\n")
    expect(second_in, b"OK REGISTER\n")
    guest.sendall(b"REGISTER " + alice + b" pass\n")
    expect(guest_in, b"ERROR NICKNAME_TAKEN\n")
    guest.sendall(b"LOGIN " + bob + b" wrong\n")
    expect(guest_in, b"ERROR INVALID_CREDENTIALS\n")
    guest.sendall(b"LOGIN nonexisting pass\n")
    expect(guest_in, b"ERROR INVALID_CREDENTIALS\n")
    if os.environ.get("CHAT_TEST_FAKE_DB") == "1":
        guest.sendall(b"REGISTER dbfailure pass\nLOGIN dbfailure pass\n")
        expect(guest_in, b"ERROR DATABASE_ERROR\n")
        expect(guest_in, b"ERROR DATABASE_ERROR\n")
    first.sendall(b"LOG")
    second.sendall(b"LOGIN " + bob + b" pass\n")
    expect(second_in, b"OK LOGIN\n")
    expect_users(second_in, bob)
    first.sendall(b"IN " + alice + b" pass\n")
    expect(first_in, b"OK LOGIN\n")
    expect_users(first_in, alice, bob)
    expect_users(second_in, alice, bob)
    guest.sendall(b"LOGIN " + alice + b" pass\n")
    expect(guest_in, b"ERROR NICKNAME_TAKEN\n")
    first.sendall(b"LOGIN another pass\n")
    expect(first_in, b"ERROR ALREADY_LOGGED_IN\n")

    # 半条消息和粘包；仅已登录的其他用户收到消息。
    first.sendall(b"MSG hel")
    first.sendall(b"lo\nMSG second\n")
    expect(second_in, b"CHAT " + alice + b" hello\n")
    expect(second_in, b"CHAT " + alice + b" second\n")
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
    expect(first_in, b"CHAT " + bob + b" still here\n")
    second_in.close()
    second.close()
    expect_users(first_in, alice)
    # 断开回调处理完成后昵称可重新使用。
    for _ in range(20):
        guest.sendall(b"LOGIN " + bob + b" pass\n")
        line = guest_in.readline()
        if line == b"OK LOGIN\n":
            break
        assert line == b"ERROR NICKNAME_TAKEN\n", line
        time.sleep(0.05)
    else:
        raise AssertionError("disconnected nickname was not released")
    expect_users(guest_in, alice, bob)
    expect_users(first_in, alice, bob)
    guest.sendall(b"MSG after reconnect\n")
    expect(first_in, b"CHAT " + bob + b" after reconnect\n")

    # 超长未结束行返回错误并关闭，不再参与群聊。
    guest.sendall(b"MSG " + b"x" * 1025)
    expect(guest_in, b"ERROR MESSAGE_TOO_LONG\n")
    assert guest_in.read(1) == b""
    expect_users(first_in, alice)
    first.sendall(b"MSG alone\n")
    first.sendall(b"BAD command\n")
    expect(first_in, b"ERROR INVALID_COMMAND\n")
    print("ChatHandler integration checks passed")
finally:
    for stream in (first_in, second_in, guest_in):
        stream.close()
    for sock in (first, second, guest):
        sock.close()
