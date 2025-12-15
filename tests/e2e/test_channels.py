"""
버전: v0.4.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: JOIN/PART 채널 멤버십과 브로드캐스트를 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def register_client(sock: socket.socket, password: str, nick: str):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    recv_line(sock)


class ChannelTest(unittest.TestCase):
    def test_join_broadcast_between_clients(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock1:
                register_client(sock1, password, "hero1")
                sock1.sendall(b"JOIN #room\r\n")
                join_self = recv_line(sock1)
                self.assertIn("JOIN #room", join_self)
                self.assertIn("hero1", join_self)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock2:
                    register_client(sock2, password, "hero2")
                    sock2.sendall(b"JOIN #room\r\n")
                    join_second_self = recv_line(sock2)
                    self.assertIn("JOIN #room", join_second_self)
                    self.assertIn("hero2", join_second_self)

                    join_from_other = recv_line(sock1)
                    self.assertIn("JOIN #room", join_from_other)
                    self.assertIn("hero2", join_from_other)

    def test_part_stops_additional_channel_messages(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock1:
                register_client(sock1, password, "alpha")
                sock1.sendall(b"JOIN #room\r\n")
                recv_line(sock1)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock2:
                    register_client(sock2, password, "beta")
                    sock2.sendall(b"JOIN #room\r\n")
                    recv_line(sock2)
                    recv_line(sock1)

                    sock1.sendall(b"PART #room :bye\r\n")
                    part_self = recv_line(sock1)
                    self.assertIn("PART #room", part_self)
                    self.assertIn("bye", part_self)

                    part_notice = recv_line(sock2)
                    self.assertIn("PART #room", part_notice)
                    self.assertIn("alpha", part_notice)

                    with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock3:
                        register_client(sock3, password, "gamma")
                        sock3.sendall(b"JOIN #room\r\n")
                        recv_line(sock3)
                        recv_line(sock2)

                        sock1.settimeout(0.5)
                        with self.assertRaises(socket.timeout):
                            sock1.recv(1024)


if __name__ == "__main__":
    unittest.main()
