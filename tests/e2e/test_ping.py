"""
버전: v0.3.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: 등록 전후 PING/PONG 응답과 파라미터 부족 에러를 확인한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


class PingTest(unittest.TestCase):
    def test_ping_before_registration(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PING hello\r\n")
                response = sock.recv(1024)
                self.assertTrue(response.endswith(b"\r\n"), "CRLF 누락")
                text = response.decode("utf-8", errors="replace").strip("\r\n")
                self.assertEqual("PONG hello", text)

    def test_ping_trailing_roundtrip(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PING :hello world\r\n")
                text = recv_line(sock)
                self.assertEqual("PONG :hello world", text)

    def test_ping_and_pong_need_payload(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PING\r\n")
                ping_error = recv_line(sock)
                self.assertIn("409", ping_error)
                self.assertIn(":출처 없음", ping_error)

                sock.sendall(b"PONG\r\n")
                pong_error = recv_line(sock)
                self.assertIn("409", pong_error)
                self.assertIn(":출처 없음", pong_error)


if __name__ == "__main__":
    unittest.main()

