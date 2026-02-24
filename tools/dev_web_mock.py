#!/usr/bin/env python3
import argparse
import json
import os
import time
import urllib.parse
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPLOAD_DIR = os.path.join(REPO_ROOT, "upload")


def _now_iso() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


class MockState:
    def __init__(self) -> None:
        self.data: dict[str, object] = {
            "manualMode": 0,
            "mode": "AUTO",
            "current1": 23.0,
            "current2": 23.0,
            "target1": 23.0,
            "target2": 23.0,
            "h1": 0,
            "h2": 0,
            "manualPercent1": 0,
            "manualPercent2": 0,
            "swap": False,
            "batt1Cells": 3,
            "batt2Cells": 3,
            "batt1Chem": 0,
            "batt2Chem": 0,
            "batt1V": 12.2,
            "batt2V": 12.1,
            "batt1Soc": 78,
            "batt2Soc": 74,
            "batt1CellV": 4.07,
            "batt2CellV": 4.03,
            "adc1Mv": 4100,
            "adc2Mv": 4050,
            "ntcMosfet1C": 42.3,
            "ntcMosfet2C": 41.8,
            "ntcMosfet1Mv": 1234,
            "ntcMosfet2Mv": 1250,
            "mosfetOvertempLimitC": 80,
            "mosfet1OvertempActive": 0,
            "mosfet2OvertempActive": 0,
            "mosfet1OvertempLatched": 0,
            "mosfet2OvertempLatched": 0,
            "mosfet1OvertempTripC": 0.0,
            "mosfet2OvertempTripC": 0.0,
            "apEnabled": 1,
            "wifiRadiosDisabled": 0,
            "apIp": "4.3.2.1",
            "apSsid": "HeatControl-AP",
            "ssid": "MyWiFi",
            "staIp": "192.168.1.50",
            "staConnected": 1,
            "apTimeoutMin": 10,
            "manualToggleMaxOffMs": 1500,
            "signalTimingPreset": 1,
            "totalRuntime": "1h 23m",
            "currentRuntime": "5m 12s",
            "bootPin": "HIGH",
            "logLevel": "info",
        }
        self.log_lines: list[str] = [
            f"[{_now_iso()}] INFO Mock server started",
            f"[{_now_iso()}] INFO This is not real firmware",
        ]

    def add_log(self, level: str, message: str) -> None:
        self.log_lines.append(f"[{_now_iso()}] {level.upper()} {message}")
        if len(self.log_lines) > 400:
            self.log_lines = self.log_lines[-400:]


STATE = MockState()


class Handler(BaseHTTPRequestHandler):
    server_version = "HeatControlMock/1.0"

    def log_message(self, fmt: str, *args) -> None:
        STATE.add_log("debug", fmt % args)

    def _send_bytes(self, status: int, data: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def _send_json(self, status: int, payload: object) -> None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self._send_bytes(status, data, "application/json; charset=utf-8")

    def _read_form(self) -> dict[str, str]:
        length = int(self.headers.get("Content-Length", "0") or "0")
        raw = self.rfile.read(length) if length > 0 else b""
        content_type = self.headers.get("Content-Type", "")
        if "application/x-www-form-urlencoded" in content_type:
            parsed = urllib.parse.parse_qs(raw.decode("utf-8"), keep_blank_values=True)
            return {k: (v[0] if v else "") for k, v in parsed.items()}
        return {}

    def _serve_upload_file(self, rel_path: str) -> None:
        rel_path = rel_path.lstrip("/")
        if not rel_path or rel_path.endswith("/"):
            rel_path = os.path.join(rel_path, "index.html")

        safe_path = os.path.normpath(rel_path).replace("\\", "/")
        if safe_path.startswith("../") or safe_path == "..":
            self._send_bytes(HTTPStatus.NOT_FOUND, b"not found", "text/plain")
            return

        file_path = os.path.join(UPLOAD_DIR, safe_path)
        if not os.path.isfile(file_path):
            self._send_bytes(HTTPStatus.NOT_FOUND, b"not found", "text/plain")
            return

        _, ext = os.path.splitext(file_path.lower())
        if ext == ".html":
            ctype = "text/html; charset=utf-8"
        elif ext == ".png":
            ctype = "image/png"
        elif ext == ".txt" or ext == ".md":
            ctype = "text/plain; charset=utf-8"
        elif ext == ".js":
            ctype = "application/javascript; charset=utf-8"
        elif ext == ".css":
            ctype = "text/css; charset=utf-8"
        else:
            ctype = "application/octet-stream"

        with open(file_path, "rb") as f:
            data = f.read()
        self._send_bytes(HTTPStatus.OK, data, ctype)

    def do_GET(self) -> None:
        path = urllib.parse.urlparse(self.path).path
        if path == "/status":
            self._send_json(HTTPStatus.OK, STATE.data)
            return
        if path == "/logs":
            text = "\n".join(STATE.log_lines) + "\n"
            self._send_bytes(HTTPStatus.OK, text.encode("utf-8"), "text/plain; charset=utf-8")
            return
        if path == "/update":
            html = (
                "<!doctype html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Mock OTA</title></head><body>"
                "<h3>Mock OTA Upload</h3>"
                "<p>This page is served by tools/dev_web_mock.py (not real firmware).</p>"
                "<form method='post' action='/update' enctype='multipart/form-data'>"
                "<input type='file' name='firmware' accept='.bin' required>"
                "<button type='submit'>Upload</button>"
                "</form></body></html>"
            )
            self._send_bytes(HTTPStatus.OK, html.encode("utf-8"), "text/html; charset=utf-8")
            return

        self._serve_upload_file(path)

    def do_POST(self) -> None:
        path = urllib.parse.urlparse(self.path).path

        if path == "/update":
            STATE.add_log("info", "Received mock OTA upload")
            self._send_bytes(HTTPStatus.OK, b"OK (mock)\n", "text/plain; charset=utf-8")
            return

        form = self._read_form()
        if path == "/setTemp":
            if "temp1" in form:
                STATE.data["target1"] = float(form["temp1"])
            if "temp2" in form:
                STATE.data["target2"] = float(form["temp2"])
            STATE.add_log("info", f"Set target temps: {STATE.data['target1']}/{STATE.data['target2']}")
            self._send_json(HTTPStatus.OK, {"ok": 1})
            return
        if path == "/setApEnabled":
            enabled = form.get("enabled", "0")
            STATE.data["apEnabled"] = 1 if enabled == "1" else 0
            STATE.add_log("info", f"AP enabled set to {STATE.data['apEnabled']}")
            self._send_json(HTTPStatus.OK, {"ok": 1})
            return
        if path == "/setLogLevel":
            level = (form.get("level") or "info").lower()
            if level in ("error", "info", "debug"):
                STATE.data["logLevel"] = level
            STATE.add_log("info", f"Log level set to {STATE.data['logLevel']}")
            self._send_json(HTTPStatus.OK, {"ok": 1})
            return
        if path in (
            "/setManualToggle",
            "/setBattery1",
            "/setBattery2",
            "/setWiFi",
            "/restart",
            "/resetRuntime",
            "/resetOvertemp",
        ):
            STATE.add_log("info", f"Handled {path} (mock)")
            self._send_json(HTTPStatus.OK, {"ok": 1})
            return

        self._send_bytes(HTTPStatus.NOT_FOUND, b"not found\n", "text/plain; charset=utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="HeatControl: static UI + mock API dev server")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    if not os.path.isfile(os.path.join(UPLOAD_DIR, "index.html")):
        raise SystemExit(f"upload/index.html not found at {UPLOAD_DIR}")

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Serving UI from {UPLOAD_DIR}")
    print(f"Listening on http://{args.host}:{args.port} (0.0.0.0 means all interfaces)")
    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

