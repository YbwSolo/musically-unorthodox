#!/usr/bin/env python3
# Stand-in for a local Ollama server, used by the session harness to test the AI tutor plumbing.
#   python3 mock_ollama.py 11599
import json, sys
from http.server import BaseHTTPRequestHandler, HTTPServer

class Handler(BaseHTTPRequestHandler):
    def _send(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/api/tags":
            self._send({"models": [{"name": "nomic-embed-text:latest"}, {"name": "llama3.2:latest"}]})
        else:
            self.send_error(404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        # JUCE's Linux-only HTTP code (never shipped: the plugin is built for macOS) puts a stray blank line
        # in front of the body, which pushes the last two bytes past the declared length. Read them as well.
        if raw.startswith(b"\r\n"):
            raw = raw[2:] + self.rfile.read(2)
        if "--debug" in sys.argv:
            sys.stderr.write("POST %s headers=%r body=%r\n" % (self.path, dict(self.headers), raw[:200]))
        try:
            req = json.loads(raw or b"{}")
        except ValueError:
            self.send_error(400)
            return
        if self.path != "/api/chat" or req.get("stream") is not False:
            self.send_error(400)
            return
        system = next((m["content"] for m in req["messages"] if m["role"] == "system"), "")
        question = next((m["content"] for m in reversed(req["messages"]) if m["role"] == "user"), "")
        self._send({"model": req.get("model"), "done": True,
                    "message": {"role": "assistant",
                                "content": "<think>checking the notes</think>**Mock answer** to: " + question + " (system prompt " + str(len(system)) + " chars)"}})

    def log_message(self, *args):
        pass

HTTPServer(("127.0.0.1", int(sys.argv[1]) if len(sys.argv) > 1 else 11599), Handler).serve_forever()
