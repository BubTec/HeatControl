#!/usr/bin/env python3

import argparse
import contextlib
import json
import os
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MOCK_SERVER = os.path.join(REPO_ROOT, "tools", "dev_web_mock.py")


def http_get_text(url: str, timeout_s: float = 2.0) -> str:
    with urllib.request.urlopen(url, timeout=timeout_s) as response:
        return response.read().decode("utf-8", errors="replace")


def http_post_json(url: str, payload: dict, timeout_s: float = 2.0) -> str:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout_s) as response:
        return response.read().decode("utf-8", errors="replace")


def wait_http_ok(url: str, timeout_s: float = 5.0) -> None:
    start = time.monotonic()
    last_error: Exception | None = None
    while time.monotonic() - start < timeout_s:
        try:
            http_get_text(url)
            return
        except (urllib.error.URLError, ConnectionError) as exc:
            last_error = exc
            time.sleep(0.1)
    raise RuntimeError(f"Server did not respond in time: {url} ({last_error})")


def get_free_port() -> int:
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def require_selenium() -> None:
    try:
        import selenium  # noqa: F401
    except ModuleNotFoundError:
        raise SystemExit(
            "Selenium is not installed for this Python interpreter.\n"
            "Install it e.g. via: pip install selenium\n"
            "Then re-run: python3 tools/web_ui_selenium_smoke.py\n"
        )


def create_driver(headless: bool):
    from selenium import webdriver
    from selenium.webdriver.chrome.options import Options as ChromeOptions

    options = ChromeOptions()
    if headless:
        options.add_argument("--headless=new")
    options.add_argument("--no-sandbox")
    options.add_argument("--disable-dev-shm-usage")
    options.add_argument("--window-size=1200,900")
    return webdriver.Chrome(options=options)


def wait_for_text_not_equal(driver, css_selector: str, not_value: str, timeout_s: float = 5.0) -> str:
    from selenium.webdriver.support.ui import WebDriverWait

    def _predicate(drv):
        element = drv.find_element("css selector", css_selector)
        text = (element.text or "").strip()
        if text and text != not_value:
            return text
        return False

    return str(WebDriverWait(driver, timeout_s).until(_predicate))


def click(driver, css_selector: str, timeout_s: float = 5.0) -> None:
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.support import expected_conditions as EC
    from selenium.webdriver.common.by import By

    WebDriverWait(driver, timeout_s).until(EC.element_to_be_clickable((By.CSS_SELECTOR, css_selector))).click()


def run_smoke(base_url: str) -> None:
    from selenium.webdriver.common.by import By
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.support import expected_conditions as EC

    driver = create_driver(headless=True)
    try:
        http_post_json(
            base_url + "/__mock/state",
            {
                "manualMode": 1,
                "mode": "MANUAL",
                "manualPercent1": 50,
                "manualPercent2": 25,
                "current1": 24.0,
                "current2": 26.0,
            },
        )

        driver.get(base_url + "/")

        WebDriverWait(driver, 8).until(EC.presence_of_element_located((By.CSS_SELECTOR, "#modePill")))
        WebDriverWait(driver, 8).until(lambda d: d.find_element(By.CSS_SELECTOR, "#modePill").text.strip() == "MANUAL")
        firmware_version = wait_for_text_not_equal(driver, "#firmwareVersion", "-")
        print(f"[ok] firmware version shown: {firmware_version}")

        wait_for_text_not_equal(driver, "#currentTemp1Display", "-")
        wait_for_text_not_equal(driver, "#currentTemp2Display", "-")
        print("[ok] status polling updates UI")

        click(driver, "#advancedToggle")
        click(driver, "#otaOpenBtn")
        WebDriverWait(driver, 8).until(lambda d: d.current_url.rstrip("/").endswith("/update"))
        print("[ok] OTA page navigation works")

        driver.get(base_url + "/")
        click(driver, "#temp1UpBtn")
        print("[ok] temp control is clickable")
    finally:
        driver.quit()


def main() -> int:
    parser = argparse.ArgumentParser(description="Selenium smoke test for HeatControl web UI")
    parser.add_argument("--base-url", default="", help="Use existing server, e.g. http://127.0.0.1:8080")
    args = parser.parse_args()

    require_selenium()

    if args.base_url:
        base_url = args.base_url.rstrip("/")
        run_smoke(base_url)
        return 0

    port = get_free_port()
    base_url = f"http://127.0.0.1:{port}"
    proc = subprocess.Popen(
        [sys.executable, MOCK_SERVER, "--host", "127.0.0.1", "--port", str(port)],
        cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        wait_http_ok(base_url + "/status", timeout_s=5)
        payload = json.loads(http_get_text(base_url + "/status"))
        print(f"[ok] mock server started on {base_url} (mode={payload.get('mode')})")
        run_smoke(base_url)
        return 0
    finally:
        proc.terminate()
        with contextlib.suppress(Exception):
            proc.wait(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
