"""
버전: v0.9.0
관련 문서: design/protocol/contract.md, design/server/v0.9.0-defensive.md
테스트: 이 파일 자체
설명: 레이트리밋과 송신 큐 백프레셔 정책을 검증한다.
"""
import os
import socket
import tempfile
import time
import unittest

from .utils import recv_line, run_server


def register_client(sock: socket.socket, password: str, nick: str):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    recv_line(sock)


def write_config(limit: int, outbound_lines: int | None = None) -> str:
    fd, path = tempfile.mkstemp()
    with os.fdopen(fd, "w") as f:
        f.write("[server]\n")
        f.write("name=modern-irc\n")
        f.write("[logging]\n")
        f.write("level=error\n")
        f.write("file=-\n")
        f.write("[limits]\n")
        f.write(f"messages_per_5s={limit}\n")
        if outbound_lines is not None:
            f.write(f"outbound_lines={outbound_lines}\n")
    return path


class DefensiveTest(unittest.TestCase):
    def test_rate_limit_blocks_spam(self):
        config_path = write_config(2)
        try:
            with run_server(config_path=config_path) as (_proc, port, password):
                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sender:
                    register_client(sender, password, "limiter")
                    sender.sendall(b"JOIN #room\r\n")
                    recv_line(sender)

                    with socket.create_connection(("127.0.0.1", port), timeout=2.0) as receiver:
                        register_client(receiver, password, "target")
                        receiver.sendall(b"JOIN #room\r\n")
                        recv_line(receiver)
                        recv_line(sender)

                        for text in ["one", "two"]:
                            sender.sendall(f"PRIVMSG #room :{text}\r\n".encode())
                            delivered = recv_line(receiver)
                            self.assertIn(text, delivered)

                        sender.settimeout(2.0)
                        sender.sendall(b"PRIVMSG #room :three\r\n")
                        warning = recv_line(sender)

                        self.assertIn("439", warning)
                        self.assertIn("발송 속도 초과", warning)

                        receiver.settimeout(0.5)
                        try:
                            extra = recv_line(receiver)
                            self.fail(f"unexpected delivery: {extra}")
                        except socket.timeout:
                            pass
        finally:
            os.remove(config_path)

    def test_slow_consumer_gets_closed_on_queue_overflow(self):
        config_path = write_config(0, outbound_lines=8)
        try:
            with run_server(config_path=config_path) as (_proc, port, password):
                slow_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                slow_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 512)
                slow_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_WINDOW_CLAMP, 1024)
                slow_sock.settimeout(2.0)
                slow_sock.connect(("127.0.0.1", port))

                with slow_sock:
                    register_client(slow_sock, password, "slow")
                    slow_sock.sendall(b"JOIN #drain\r\n")
                    recv_line(slow_sock)

                    senders = []
                    try:
                        for idx in range(3):
                            sender = socket.create_connection(("127.0.0.1", port), timeout=2.0)
                            register_client(sender, password, f"pump{idx}")
                            sender.sendall(b"JOIN #drain\r\n")
                            recv_line(sender)
                            recv_line(slow_sock)
                            senders.append(sender)

                        slow_sock.settimeout(0.05)
                        payload = "x" * 400
                        for i in range(800):
                            for sender in list(senders):
                                try:
                                    sender.sendall(f"PRIVMSG #drain :{payload}{i}\r\n".encode())
                                except (ConnectionResetError, BrokenPipeError):
                                    senders.remove(sender)
                                    sender.close()

                        time.sleep(0.5)
                        slow_sock.settimeout(0.2)
                        closed = False
                        deadline = time.time() + 10.0
                        while time.time() < deadline:
                            try:
                                data = slow_sock.recv(1024)
                                if data == b"":
                                    closed = True
                                    break
                            except socket.timeout:
                                continue
                            except (ConnectionResetError, BrokenPipeError):
                                closed = True
                                break
                            if not data:
                                continue

                        self.assertTrue(closed, "송신 큐 초과로 연결이 닫히지 않음")
                    finally:
                        for sender in senders:
                            sender.close()
        finally:
            os.remove(config_path)


if __name__ == "__main__":
    unittest.main()
