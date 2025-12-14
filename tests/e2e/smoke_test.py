"""
설명: modern-irc 서버를 기동해 PING/PONG 왕복을 검증한다.
버전: v0.1.0
관련 문서: design/protocol/contract.md
테스트: 이 파일 자체
"""
import os
import socket
import subprocess
import time


def find_free_port():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("", 0))
    _, port = sock.getsockname()
    sock.close()
    return port


def wait_for_listen(host, port, timeout=5.0):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.create_connection((host, port), timeout=0.5):
                return True
        except (ConnectionRefusedError, socket.timeout, OSError):
            time.sleep(0.1)
    return False


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    server_path = os.path.join(repo_root, "modern-irc")
    port = find_free_port()
    password = "testpass"

    proc = subprocess.Popen(
        [server_path, str(port), password], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )

    try:
        if not wait_for_listen("127.0.0.1", port):
            proc.kill()
            raise SystemExit("서버가 기동되지 않았습니다.")

        with socket.create_connection(("127.0.0.1", port), timeout=2.0) as sock:
            sock.sendall(b"PING hello\r\n")
            response = sock.recv(1024)
            if not response.endswith(b"\r\n"):
                raise SystemExit("CRLF가 누락된 응답")
            text = response.decode("utf-8", errors="replace").strip("\r\n")
            if text != "PONG hello":
                raise SystemExit(f"예상과 다른 응답: {text}")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    main()
