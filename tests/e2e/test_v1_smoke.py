"""
관련 문서: design/protocol/contract.md
테스트: v1.0.0 핵심 흐름 스모크(PASS/NICK/USER, JOIN/PART, 채널 PRIVMSG, MODE +k 거부/허용)를 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def register(sock: socket.socket, password: str, nick: str):
    """PASS/NICK/USER를 보내고 환영 numeric을 소비한다."""
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    welcome = recv_line(sock)
    assert "001" in welcome


class V1CoreFlowTest(unittest.TestCase):
    def test_registration_channel_message_and_mode_key(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as alice, socket.create_connection(
                ("127.0.0.1", port), timeout=2.0
            ) as bob:
                register(alice, password, "alice")
                register(bob, password, "bob")

                alice.sendall(b"JOIN #room\r\n")
                alice_join = recv_line(alice)
                self.assertIn("JOIN #room", alice_join)

                bob.sendall(b"JOIN #room\r\n")
                bob_join_self = recv_line(bob)
                self.assertIn("JOIN #room", bob_join_self)
                bob_join_broadcast = recv_line(alice)
                self.assertIn("bob", bob_join_broadcast)
                self.assertIn("JOIN #room", bob_join_broadcast)

                alice.sendall(b"PRIVMSG #room :hi all\r\n")
                msg_to_bob = recv_line(bob)
                self.assertIn("PRIVMSG #room :hi all", msg_to_bob)

                bob.sendall(b"PART #room :bye\r\n")
                part_self = recv_line(bob)
                self.assertIn("PART #room", part_self)
                part_broadcast = recv_line(alice)
                self.assertIn("bob", part_broadcast)
                self.assertIn("PART #room", part_broadcast)

                alice.sendall(b"MODE #room +k secret\r\n")
                mode_notice = recv_line(alice)
                self.assertIn("MODE #room +k secret", mode_notice)

                bob.sendall(b"JOIN #room wrong\r\n")
                deny = recv_line(bob)
                self.assertIn("475", deny)

                bob.sendall(b"JOIN #room secret\r\n")
                bob_rejoin = recv_line(bob)
                self.assertIn("JOIN #room", bob_rejoin)
                rejoin_broadcast = recv_line(alice)
                self.assertIn("bob", rejoin_broadcast)
                self.assertIn("JOIN #room", rejoin_broadcast)


if __name__ == "__main__":
    unittest.main()
