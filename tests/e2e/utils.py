"""
버전: v0.2.0
관련 문서: design/protocol/contract.md
테스트: tests/e2e
설명: E2E 테스트를 위한 서버 실행/소켓 유틸리티를 제공한다.
"""
import contextlib
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


@contextlib.contextmanager
def run_server(password="testpass"):
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    server_path = os.path.join(repo_root, "modern-irc")
    port = find_free_port()

    proc = subprocess.Popen(
        [server_path, str(port), password], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )

    try:
        if not wait_for_listen("127.0.0.1", port):
            proc.kill()
            raise RuntimeError("서버 기동 실패")
        yield proc, port, password
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
