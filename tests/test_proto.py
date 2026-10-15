"""Builds tests/test_proto.c on the host, starts the reference server on a free port and runs the client test."""
import os, socket, subprocess, sys, tempfile, time
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def main():
    port = free_port()
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "test_proto")
        subprocess.check_call(["cc", "-I" + os.path.join(ROOT, "src"), os.path.join(ROOT, "tests", "test_proto.c"), os.path.join(ROOT, "src", "net", "proto.c"), "-o", exe])
        srv = subprocess.Popen([sys.executable, os.path.join(ROOT, "server", "cast_server.py"), "--port", str(port), "--db", os.path.join(tmp, "t.sqlite")], stdout=subprocess.PIPE, text=True)
        try:
            for _ in range(50):
                try: socket.create_connection(("127.0.0.1", port), timeout=0.2).close(); break
                except OSError: time.sleep(0.1)
            r = subprocess.run([exe, str(port)], capture_output=True, text=True)
            print(r.stdout.strip())
            if r.returncode: print(r.stderr); sys.exit(1)
            # two clients: presence + PM delivery
            a = socket.create_connection(("127.0.0.1", port)); b = socket.create_connection(("127.0.0.1", port))
            a.sendall(b"HELLO Ana pt 1\n"); b.sendall(b"HELLO Beto pt 1\n"); time.sleep(0.2)
            a.sendall(b"ADD Beto\nPM Beto oi beto\n"); time.sleep(0.3)
            got = b.recv(4096).decode()
            assert "PM Ana oi beto" in got, got
            a.recv(4096); b.close(); time.sleep(0.3)
            assert "PRESENCE Beto 0" in a.recv(4096).decode()
            print("server OK")
        finally:
            srv.terminate()

if __name__ == "__main__":
    main()
