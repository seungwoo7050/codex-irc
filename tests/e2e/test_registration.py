"""
버전: v0.2.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: PASS/NICK/USER 등록 절차와 사전 거부 정책을 검증한다.
"""
import socket
import unittest

from .utils import run_server


def recv_line(sock):
    data = b""
    while b"\r\n" not in data:
        chunk = sock.recv(1024)
        if not chunk:
            break
        data += chunk
    return data.decode("utf-8", errors="replace").strip("\r\n")


class RegistrationTest(unittest.TestCase):
    def test_registration_success(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(f"PASS {password}\r\n".encode())
                sock.sendall(b"NICK hero\r\n")
                sock.sendall(b"USER user 0 * :Real User\r\n")
                welcome = recv_line(sock)
                self.assertTrue(welcome.endswith(":등록 완료"))
                self.assertIn("001", welcome)
                self.assertIn("hero", welcome)

    def test_pass_missing_or_wrong_closes(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PASS\r\n")
                error_line = recv_line(sock)
                self.assertIn("461", error_line)
                sock.settimeout(0.5)
                data = sock.recv(1024)
                self.assertEqual(b"", data)

        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PASS wrong\r\n")
                error_line = recv_line(sock)
                self.assertIn("464", error_line)
                sock.settimeout(0.5)
                data = sock.recv(1024)
                self.assertEqual(b"", data)

    def test_commands_rejected_before_registration(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"JOIN #room\r\n")
                join_reply = recv_line(sock)
                self.assertIn("451", join_reply)
                sock.sendall(b"PRIVMSG someone :hi\r\n")
                msg_reply = recv_line(sock)
                self.assertIn("451", msg_reply)


if __name__ == "__main__":
    unittest.main()

