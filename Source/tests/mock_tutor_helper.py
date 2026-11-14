#!/usr/bin/env python3
# Stand-in for the built-in tutor's helper program, used by the session harness on Linux.
# It accepts the same arguments the plugin passes to the real helper and answers the same three addresses.
import json, os, sys
from http.server import BaseHTTPRequestHandler, HTTPServer

args = sys.argv[1:]
def value(flag, fallback=""):
    return args[args.index(flag) + 1] if flag in args and args.index(flag) + 1 < len(args) else fallback

MODEL = value("-m")
if not os.path.isfile(MODEL):
    sys.exit(2)

class Handler(BaseHTTPRequestHandler):
    def _send(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            self._send({"status": "ok"})
        elif self.path == "/v1/models":
            self._send({"data": [{"id": value("-a", "none")}]})
        else:
            self.send_error(404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length)
        if raw.startswith(b"\r\n"):                      # see mock_ollama.py
            raw = raw[2:] + self.rfile.read(2)
        try:
            req = json.loads(raw or b"{}")
        except ValueError:
            self.send_error(400)
            return
        if self.path != "/v1/chat/completions" or req.get("stream") is not False:
            self.send_error(400)
            return
        question = next((m["content"] for m in reversed(req["messages"]) if m["role"] == "user"), "")
        self._send({"choices": [{"index": 0, "finish_reason": "stop",
                                 "message": {"role": "assistant",
                                             "content": "### Built-in answer\n* to: " + question + " (model " + os.path.basename(MODEL) + ", ctx " + value("-c") + ")"}}]})

    def log_message(self, *a):
        pass

HTTPServer((value("--host", "127.0.0.1"), int(value("--port", "47611"))), Handler).serve_forever()
