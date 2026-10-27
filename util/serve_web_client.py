#!/usr/bin/env python3
#
# Serve the web browser build of snis_client (bin/web, from "make WEB=1 web").
#
# The browser only allows the threads snis_client needs (SharedArrayBuffer)
# on pages served with the two "cross-origin isolation" headers added here,
# which is the only reason this is not just "python3 -m http.server".
#
# usage: util/serve_web_client.py [port [directory]]
#
import functools
import http.server
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
	def end_headers(self):
		self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
		self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
		super().end_headers()


Handler.extensions_map['.wasm'] = 'application/wasm'

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8088
directory = sys.argv[2] if len(sys.argv) > 2 else 'bin/web'
server = http.server.ThreadingHTTPServer(('', port), functools.partial(Handler, directory=directory))
print('Serving %s at http://localhost:%d/snis_client.html' % (directory, port))
server.serve_forever()
