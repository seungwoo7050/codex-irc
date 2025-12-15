"""
버전: v0.5.0
관련 문서: design/protocol/contract.md, design/server/v0.5.0-messaging.md
테스트: 이 파일 자체
설명: PRIVMSG/NOTICE 라우팅과 NAMES/LIST numeric 응답을 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def register_client(sock: socket.socket, password: str, nick: str):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    recv_line(sock)


class MessagingTest(unittest.TestCase):
    def test_privmsg_user_to_user(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sender:
                register_client(sender, password, "alice")
                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as receiver:
                    register_client(receiver, password, "bob")

                    sender.sendall(b"PRIVMSG bob :hello world\r\n")
                    msg = recv_line(receiver)

                    self.assertTrue(msg.startswith(":alice!"))
                    self.assertIn("PRIVMSG bob :hello world", msg)

    def test_privmsg_to_channel_broadcast(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as a:
                register_client(a, password, "alpha")
                a.sendall(b"JOIN #room\r\n")
                recv_line(a)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as b:
                    register_client(b, password, "bravo")
                    b.sendall(b"JOIN #room\r\n")
                    recv_line(b)
                    recv_line(a)

                    a.sendall(b"PRIVMSG #room :hey team\r\n")
                    delivered = recv_line(b)

                    self.assertTrue(delivered.startswith(":alpha!"))
                    self.assertIn("PRIVMSG #room :hey team", delivered)

    def test_names_includes_all_members(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as a:
                register_client(a, password, "hero1")
                a.sendall(b"JOIN #squad\r\n")
                recv_line(a)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as b:
                    register_client(b, password, "hero2")
                    b.sendall(b"JOIN #squad\r\n")
                    recv_line(b)
                    recv_line(a)

                    a.sendall(b"NAMES #squad\r\n")
                    names_line = recv_line(a)
                    end_line = recv_line(a)

                    self.assertIn("353", names_line)
                    self.assertIn("hero1", names_line)
                    self.assertIn("hero2", names_line)
                    self.assertIn("366", end_line)

    def test_list_reports_channels(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as a:
                register_client(a, password, "one")
                a.sendall(b"JOIN #listroom\r\n")
                recv_line(a)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as b:
                    register_client(b, password, "two")
                    b.sendall(b"JOIN #listroom\r\n")
                    recv_line(b)
                    recv_line(a)

                    a.settimeout(3.0)
                    a.sendall(b"LIST\r\n")
                    start = recv_line(a)
                    entry = recv_line(a)
                    end = recv_line(a)

                    self.assertIn("321", start)
                    self.assertIn("322", entry)
                    self.assertIn("#listroom", entry)
                    self.assertIn("2", entry)
                    self.assertIn("323", end)


if __name__ == "__main__":
    unittest.main()
