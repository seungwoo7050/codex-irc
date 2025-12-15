"""
버전: v0.8.0
관련 문서: design/protocol/contract.md, design/server/v0.8.0-config-logging.md
테스트: 이 파일 자체
설명: REHASH 명령이 설정 파일 변경을 반영하고 성공 numeric을 반환하는지 확인한다.
"""
import os
import socket
import tempfile
import unittest

from .utils import recv_line, run_server


def write_config(path, server_name, level="info"):
    with open(path, "w", encoding="utf-8") as file:
        file.write("[server]\n")
        file.write(f"name={server_name}\n")
        file.write("[logging]\n")
        file.write(f"level={level}\n")
        file.write("file=-\n")
        file.write("[limits]\n")
        file.write("messages_per_5s=0\n")


class RehashTest(unittest.TestCase):
    def test_rehash_applies_new_server_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            config_path = os.path.join(tmp, "server.ini")
            write_config(config_path, "alpha-irc", level="warn")

            with run_server(config_path=config_path) as (_proc, port, password):
                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
                    sock.sendall(f"PASS {password}\r\n".encode())
                    sock.sendall(b"NICK hero\r\n")
                    sock.sendall(b"USER user 0 * :Real User\r\n")
                    welcome = recv_line(sock)
                    self.assertTrue(welcome.startswith(":alpha-irc 001"))

                    write_config(config_path, "bravo-irc", level="debug")

                    sock.sendall(b"REHASH\r\n")
                    rehash_reply = recv_line(sock)
                    self.assertTrue(rehash_reply.startswith(":bravo-irc 382"))
                    self.assertIn("설정 리로드 완료", rehash_reply)

                    sock.sendall(b"UNKNOWNCMD\r\n")
                    unknown_reply = recv_line(sock)
                    self.assertTrue(unknown_reply.startswith(":bravo-irc 421"))


if __name__ == "__main__":
    unittest.main()

