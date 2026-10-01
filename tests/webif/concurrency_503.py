#!/usr/bin/env python3
import os
import re
import signal
import socket
import subprocess
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
BIN = os.environ.get("TCMG_WEBIF_BIN", os.path.join(ROOT, "build", "tcmg"))

def wait_http(port, timeout=8.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25) as s:
                s.sendall(b"GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")
                if s.recv(32).startswith(b"HTTP/1.1"):
                    return True
        except OSError:
            time.sleep(0.05)
    return False

def response_code(port):
    with socket.create_connection(("127.0.0.1", port), timeout=3) as s:
        s.sendall(b"GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")
        head = s.recv(128)
    line = head.split(b"\r\n", 1)[0]
    return int(line.split()[1])

def wait_for_code(port, expected, timeout=2.0):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        try:
            last = response_code(port)
            if last == expected:
                return True
        except OSError:
            pass
        time.sleep(0.02)
    return False

def main():
    if not os.path.exists(BIN):
        raise SystemExit(f"missing TCMG binary: {BIN}")
    server_src = open(os.path.join(ROOT, "webif", "server.c"), encoding="utf-8").read()
    m = re.search(r"^#define\s+WEBIF_MAX_THREADS\s+(\d+)", server_src, re.MULTILINE)
    if not m:
        raise SystemExit("WEBIF_MAX_THREADS not found")
    workers = int(m.group(1))
    if workers < 1:
        raise SystemExit("WEBIF_MAX_THREADS invalid")

    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]

    with tempfile.TemporaryDirectory(prefix="tcmg-webif-503-") as d:
        with open(os.path.join(d, "tcmg.conf"), "w", encoding="utf-8") as f:
            f.write(
                "[global]\nsocket_timeout=30\nserver_keepalive=20\nserver_keepalive_misses=3\n"
                "ecm_log=0\nlogfile=\nusrfile=\n"
                f"[webif]\nenabled=1\nport={port}\nrefresh=7\nuser=\npassword=\n"
                "bindaddr=127.0.0.1\n"
                "[newcamd]\nport=0\nbindaddr=\nkey=0102030405060708091011121314\n"
                "keepalive=1\nmode=auto\n"
                "[cccam]\nport=0\nbindaddr=\n"
                "[cs378x]\nport=0\nbindaddr=\n"
                "[failban]\nenabled=0\nallowlist=\nmax_fails=5\nban_secs=300\n"
            )
        open(os.path.join(d, "tcmg.users"), "w", encoding="utf-8").close()
        open(os.path.join(d, "tcmg.readers"), "w", encoding="utf-8").close()

        p = subprocess.Popen([BIN, "-c", d], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        held = []
        try:
            if not wait_http(port):
                raise SystemExit("WebIF did not start")

            for _ in range(workers):
                s = socket.create_connection(("127.0.0.1", port), timeout=3)
                s.sendall(b"GET /status HTTP/1.1\r\nHost: 127.0.0.1\r\n")
                held.append(s)

            time.sleep(0.1)
            code = response_code(port)
            if code != 503:
                raise SystemExit(f"expected 503 with {workers} workers occupied, got {code}")

            for s in held:
                s.close()
            held.clear()
            if not wait_for_code(port, 200):
                raise SystemExit("WebIF did not recover after releasing worker slots")

            print(f"WEBIF_503_CONCURRENCY: PASS ({workers} workers saturated)")
        finally:
            for s in held:
                try:
                    s.close()
                except OSError:
                    pass
            try:
                p.send_signal(signal.SIGTERM)
                p.wait(timeout=5)
            except Exception:
                p.kill()
                p.wait(timeout=2)

if __name__ == "__main__":
    main()
