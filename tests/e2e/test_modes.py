"""
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: MODE(+i/+t/+k/+l) 기반 JOIN 거부/허용을 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def register_client(sock: socket.socket, password: str, nick: str):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    recv_line(sock)


class ModeTest(unittest.TestCase):
    def test_invite_only_blocks_uninvited_join(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as op_sock:
                register_client(op_sock, password, "op")
                op_sock.sendall(b"JOIN #room\r\n")
                recv_line(op_sock)

                op_sock.sendall(b"MODE #room +i\r\n")
                mode_line = recv_line(op_sock)
                self.assertIn("MODE #room +i", mode_line)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as guest:
                    register_client(guest, password, "guest")
                    guest.sendall(b"JOIN #room\r\n")
                    deny = recv_line(guest)
                    self.assertIn("473", deny)
                    self.assertIn("초대 전용", deny)

    def test_key_mode_blocks_wrong_key_and_accepts_correct_key(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as op_sock:
                register_client(op_sock, password, "keeper")
                op_sock.sendall(b"JOIN #vault\r\n")
                recv_line(op_sock)

                op_sock.sendall(b"MODE #vault +k secret\r\n")
                mode_line = recv_line(op_sock)
                self.assertIn("MODE #vault +k", mode_line)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as guest:
                    register_client(guest, password, "visitor")
                    guest.sendall(b"JOIN #vault wrong\r\n")
                    deny = recv_line(guest)
                    self.assertIn("475", deny)
                    self.assertIn("채널 키 불일치", deny)

                    guest.sendall(b"JOIN #vault secret\r\n")
                    join_line = recv_line(guest)
                    self.assertIn("JOIN #vault", join_line)

                    broadcast = recv_line(op_sock)
                    self.assertIn("visitor", broadcast)
                    self.assertIn("JOIN #vault", broadcast)

    def test_limit_mode_blocks_when_full(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as op_sock:
                register_client(op_sock, password, "cap")
                op_sock.sendall(b"JOIN #tiny\r\n")
                recv_line(op_sock)

                op_sock.sendall(b"MODE #tiny +l 1\r\n")
                mode_line = recv_line(op_sock)
                self.assertIn("MODE #tiny +l", mode_line)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as guest:
                    register_client(guest, password, "late")
                    guest.sendall(b"JOIN #tiny\r\n")
                    deny = recv_line(guest)
                    self.assertIn("471", deny)
                    self.assertIn("채널 인원 초과", deny)


if __name__ == "__main__":
    unittest.main()
