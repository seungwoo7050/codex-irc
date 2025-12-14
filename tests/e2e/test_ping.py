"""
버전: v0.2.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: 등록 전에도 PING 응답이 오는지 확인한다.
"""
import socket
import unittest

from .utils import run_server


class PingTest(unittest.TestCase):
    def test_ping_before_registration(self):
        with run_server() as (_proc, port, _password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                sock.sendall(b"PING hello\r\n")
                response = sock.recv(1024)
                self.assertTrue(response.endswith(b"\r\n"), "CRLF 누락")
                text = response.decode("utf-8", errors="replace").strip("\r\n")
                self.assertEqual("PONG hello", text)


if __name__ == "__main__":
    unittest.main()

