# SPDX-FileCopyrightText: Copyright 2026 NSPX
# SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared shader cache server for Eden UWP: one D3D12 pipeline cache (d3d12.bin) per title.

    GET /<title_id>/d3d12.bin  -> the cache, or 404
    PUT /<title_id>/d3d12.bin  -> stored when it is a valid cache larger than the current one

Consoles merge the shared cache into theirs before uploading, so a larger upload is a superset.
Run on any PC on the consoles' network:  python server.py [--port 8791] [--dir ./data]
and set Settings > System > Shared shader cache to http://<pc-address>:8791 on each console.

After NXbox's tools/shader-share/server.ts (NXbox by NSPX, https://github.com/NspxMiguel/NXbox,
GPL-3.0), rewritten in standard-library Python for the D3D12 cache.
"""
import argparse
import os
import re
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_BYTES = 64 * 1024 * 1024
MAGIC = b"yuzucach"
PATH = re.compile(r"^/([0-9a-f]{16})/d3d12\.bin$")


def valid_cache(data: bytes) -> bool:
    return 12 < len(data) <= MAX_BYTES and data.startswith(MAGIC)


def make_handler(data_dir: str):
    class Handler(BaseHTTPRequestHandler):
        def _target(self):
            match = PATH.match(self.path)
            return os.path.join(data_dir, match.group(1) + ".bin") if match else None

        def _reply(self, status: int, body: bytes = b"", content_type="text/plain"):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            target = self._target()
            if target is None:
                return self._reply(404, b"not found")
            if not os.path.exists(target):
                return self._reply(404, b"no cache yet")
            with open(target, "rb") as f:
                self._reply(200, f.read(), "application/octet-stream")

        def do_PUT(self):
            target = self._target()
            if target is None:
                return self._reply(404, b"not found")
            length = int(self.headers.get("Content-Length") or 0)
            if length <= 0 or length > MAX_BYTES:
                return self._reply(400, b"invalid size")
            data = self.rfile.read(length)
            if not valid_cache(data):
                return self._reply(400, b"invalid cache")
            current = os.path.getsize(target) if os.path.exists(target) else 0
            if len(data) <= current:
                return self._reply(200, b"kept existing")
            fd, tmp = tempfile.mkstemp(dir=data_dir, suffix=".tmp")
            with os.fdopen(fd, "wb") as f:
                f.write(data)
            os.replace(tmp, target)
            self.log_message("%s %d -> %d", os.path.basename(target), current, len(data))
            self._reply(201, b"stored")

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8791)
    parser.add_argument("--dir", default=os.path.join(os.path.dirname(__file__), "data"))
    args = parser.parse_args()
    os.makedirs(args.dir, exist_ok=True)
    server = ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(args.dir))
    print(f"shader-share on :{args.port}, data in {args.dir}")
    server.serve_forever()


if __name__ == "__main__":
    main()
