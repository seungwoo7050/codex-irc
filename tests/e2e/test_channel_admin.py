"""
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
설명: TOPIC/KICK/INVITE 권한과 흐름을 E2E로 검증한다.
"""
import socket
import unittest

from .utils import recv_line, run_server


def register_client(sock: socket.socket, password: str, nick: str):
    sock.sendall(f"PASS {password}\r\n".encode())
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Real {nick}\r\n".encode())
    recv_line(sock)


class ChannelAdminTest(unittest.TestCase):
    def test_kick_without_operator_privilege_fails(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as op_sock:
                register_client(op_sock, password, "op1")
                op_sock.sendall(b"JOIN #room\r\n")
                recv_line(op_sock)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as user_sock:
                    register_client(user_sock, password, "user2")
                    user_sock.sendall(b"JOIN #room\r\n")
                    recv_line(user_sock)
                    recv_line(op_sock)

                    user_sock.sendall(b"KICK #room op1 :bye\r\n")
                    error_line = recv_line(user_sock)
                    self.assertIn("482", error_line)
                    self.assertIn("채널 권한 없음", error_line)

    def test_invite_allows_invited_join(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as inviter:
                register_client(inviter, password, "host")
                inviter.sendall(b"JOIN #room\r\n")
                recv_line(inviter)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as guest:
                    register_client(guest, password, "guest")

                    inviter.sendall(b"INVITE guest #room\r\n")
                    inviter_ack = recv_line(inviter)
                    self.assertIn("341", inviter_ack)

                    invite_notice = recv_line(guest)
                    self.assertIn("INVITE guest #room", invite_notice)
                    self.assertIn("host", invite_notice)

                    guest.sendall(b"JOIN #room\r\n")
                    guest_join = recv_line(guest)
                    self.assertIn("JOIN #room", guest_join)

                    join_broadcast = recv_line(inviter)
                    self.assertIn("JOIN #room", join_broadcast)
                    self.assertIn("guest", join_broadcast)

    def test_topic_requires_operator_and_is_broadcast(self):
        with run_server() as (_proc, port, password):
            with socket.create_connection(("127.0.0.1", port), timeout=2.0) as op_sock:
                register_client(op_sock, password, "op1")
                op_sock.sendall(b"JOIN #room\r\n")
                recv_line(op_sock)

                with socket.create_connection(("127.0.0.1", port), timeout=2.0) as member:
                    register_client(member, password, "user2")
                    member.sendall(b"JOIN #room\r\n")
                    recv_line(member)
                    recv_line(op_sock)

                    member.sendall(b"TOPIC #room :unauthorized\r\n")
                    denied = recv_line(member)
                    self.assertIn("482", denied)

                    op_sock.sendall(b"TOPIC #room :welcome topic\r\n")
                    topic_self = recv_line(op_sock)
                    self.assertIn("TOPIC #room :welcome topic", topic_self)

                    topic_broadcast = recv_line(member)
                    self.assertIn("TOPIC #room :welcome topic", topic_broadcast)

                    member.sendall(b"TOPIC #room\r\n")
                    topic_reply = recv_line(member)
                    self.assertIn("332", topic_reply)
                    self.assertIn("welcome topic", topic_reply)


if __name__ == "__main__":
    unittest.main()
