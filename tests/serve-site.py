#!/usr/bin/env python3
"""serve-site.py DIR PORT [CSP]: serve the demo book with lmctl.com-like headers (CSP, nosniff, wasm MIME)."""
import functools
import http.server
import sys

DIR, PORT = sys.argv[1], int(sys.argv[2])
CSP = sys.argv[3] if len(sys.argv) > 3 else None


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      '.wasm': 'application/wasm', '.js': 'text/javascript', '.bin': 'application/octet-stream'}

    def end_headers(self):
        if CSP:
            self.send_header('Content-Security-Policy', CSP)
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-Frame-Options', 'DENY')
        super().end_headers()

    def log_message(self, *args):
        pass


http.server.ThreadingHTTPServer(('127.0.0.1', PORT), functools.partial(Handler, directory=DIR)).serve_forever()
