# SPDX-License-Identifier: GPL-3.0-or-later
"""python tools/shader-share/test_server.py - starts the server on a free port and checks the rules."""
import os
import sys
import tempfile
import threading
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(__file__))
import server  # noqa: E402


def request(method, url, data=None):
    req = urllib.request.Request(url, data=data, method=method)
    try:
        with urllib.request.urlopen(req) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def main():
    with tempfile.TemporaryDirectory() as d:
        httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.make_handler(d))
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
        base = f"http://127.0.0.1:{httpd.server_address[1]}/0100000000010000/d3d12.bin"
        cache = server.MAGIC + b"\x01\x00\x00\x00" + b"x" * 20
        assert request("GET", base)[0] == 404
        assert request("PUT", base, b"garbage" * 4)[0] == 400
        assert request("PUT", base, cache)[0] == 201
        assert request("GET", base) == (200, cache)
        assert request("PUT", base, cache[:-1])[1] == b"kept existing"
        assert request("PUT", base, cache + b"y")[0] == 201
        assert request("GET", base)[1] == cache + b"y"
        assert request("GET", base.replace("d3d12", "opengl"))[0] == 404
        httpd.shutdown()
    print("shader-share server: all checks passed")


if __name__ == "__main__":
    main()
