"""
버전: v0.3.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: QUIT 종료와 미지원 명령/길이 초과 오류 처리를 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def complete_registration(sock, password):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(b"NICK hero\r\n")
    sock.sendall(b"USER user 0 * :Real User\r\n")
    recv_line(sock)


class QuitAndErrorTest(unittest.TestCase):
    def test_quit_closes_connection(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"QUIT :bye\r\n")
                sock.settimeout(1.0)
                data = sock.recv(1024)
                self.assertEqual(b"", data)

    def test_unknown_command_after_registration(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                complete_registration(sock, password)
                sock.sendall(b"FROB\r\n")
                reply = recv_line(sock)
                self.assertIn("421", reply)
                self.assertIn("FROB", reply)

    def test_line_too_long_disconnects(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                long_line = b"a" * 513 + b"\r\n"
                sock.sendall(long_line)
                sock.settimeout(1.0)
                try:
                    data = sock.recv(1024)
                    self.assertEqual(b"", data)
                except ConnectionResetError:
                    pass


if __name__ == "__main__":
    unittest.main()
