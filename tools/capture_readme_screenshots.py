#!/usr/bin/env python3

import contextlib
import json
import os
import socket
import subprocess
import sys
import time
import urllib.request


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MOCK_SERVER = os.path.join(REPO_ROOT, "tools", "dev_web_mock.py")
DOC_DIR = os.path.join(REPO_ROOT, "documentation")


def get_free_port() -> int:
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def http_post_json(url: str, payload: dict, timeout_s: float = 2.0) -> None:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=timeout_s):
        return


def wait_http_ok(url: str, timeout_s: float = 5.0) -> None:
    start = time.monotonic()
    while time.monotonic() - start < timeout_s:
        try:
            with urllib.request.urlopen(url, timeout=1.0):
                return
        except Exception:
            time.sleep(0.1)
    raise RuntimeError(f"Server did not respond in time: {url}")


def create_driver():
    from selenium import webdriver
    from selenium.webdriver.chrome.options import Options as ChromeOptions

    options = ChromeOptions()
    options.add_argument("--headless=new")
    options.add_argument("--no-sandbox")
    options.add_argument("--disable-dev-shm-usage")
    options.add_argument("--window-size=520,1200")
    return webdriver.Chrome(options=options)


def click(driver, css_selector: str, timeout_s: float = 8.0) -> None:
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.support import expected_conditions as EC
    from selenium.webdriver.common.by import By

    WebDriverWait(driver, timeout_s).until(EC.element_to_be_clickable((By.CSS_SELECTOR, css_selector))).click()


def screenshot_element(driver, css_selector: str, out_path: str, timeout_s: float = 8.0) -> None:
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.common.by import By

    element = WebDriverWait(driver, timeout_s).until(lambda d: d.find_element(By.CSS_SELECTOR, css_selector))
    driver.execute_script("arguments[0].scrollIntoView({block: 'start', inline: 'nearest'});", element)
    time.sleep(0.2)
    element.screenshot(out_path)


def screenshot_xpath(driver, xpath: str, out_path: str, timeout_s: float = 8.0) -> None:
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.common.by import By

    element = WebDriverWait(driver, timeout_s).until(lambda d: d.find_element(By.XPATH, xpath))
    driver.execute_script("arguments[0].scrollIntoView({block: 'start', inline: 'nearest'});", element)
    time.sleep(0.2)
    element.screenshot(out_path)


def screenshot_closest(driver, css_selector: str, ancestor_selector: str, out_path: str, timeout_s: float = 8.0) -> None:
    from selenium.webdriver.support.ui import WebDriverWait
    from selenium.webdriver.common.by import By

    element = WebDriverWait(driver, timeout_s).until(lambda d: d.find_element(By.CSS_SELECTOR, css_selector))
    ancestor = driver.execute_script("return arguments[0].closest(arguments[1]);", element, ancestor_selector)
    if not ancestor:
        raise RuntimeError(f"Ancestor not found for {css_selector} closest({ancestor_selector})")
    driver.execute_script("arguments[0].scrollIntoView({block: 'start', inline: 'nearest'});", ancestor)
    time.sleep(0.2)
    ancestor.screenshot(out_path)


def main() -> int:
    os.makedirs(DOC_DIR, exist_ok=True)

    port = get_free_port()
    base_url = f"http://127.0.0.1:{port}"
    proc = subprocess.Popen(
        [sys.executable, MOCK_SERVER, "--host", "127.0.0.1", "--port", str(port)],
        cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        wait_http_ok(base_url + "/status")
        http_post_json(
            base_url + "/__mock/state",
            {
                "manualMode": 0,
                "mode": "AUTO",
                "staConnected": 1,
                "apEnabled": 1,
                "current1": 24.5,
                "current2": 26.0,
                "target1": 23.0,
                "target2": 24.0,
                "batt1Soc": 78,
                "batt2Soc": 74,
            },
        )

        driver = create_driver()
        try:
            driver.get(base_url + "/")
            time.sleep(0.8)

            path_main = os.path.join(DOC_DIR, "GUI.png")
            driver.save_screenshot(path_main)

            path_heaters = os.path.join(DOC_DIR, "GUI-heaters.png")
            screenshot_xpath(driver, "(//section[contains(@class,'heater-card')])[1]", path_heaters)

            click(driver, "#advancedToggle")
            time.sleep(0.4)

            path_wifi = os.path.join(DOC_DIR, "GUI-settings-wifi.png")
            screenshot_element(driver, "#wifiSection", path_wifi)

            path_ota = os.path.join(DOC_DIR, "GUI-settings-ota.png")
            screenshot_closest(driver, "#otaOpenBtn", ".sub-card", path_ota)

            path_diagnostics = os.path.join(DOC_DIR, "GUI-settings-diagnostics.png")
            screenshot_element(driver, "#settingsSection", path_diagnostics)

            driver.get(base_url + "/update")
            time.sleep(0.6)
            path_update = os.path.join(DOC_DIR, "GUI-update.png")
            screenshot_element(driver, ".card", path_update)
        finally:
            driver.quit()

        print("Wrote:")
        print("- documentation/GUI.png")
        print("- documentation/GUI-heaters.png")
        print("- documentation/GUI-settings-wifi.png")
        print("- documentation/GUI-settings-ota.png")
        print("- documentation/GUI-settings-diagnostics.png")
        print("- documentation/GUI-update.png")
        return 0
    finally:
        proc.terminate()
        with contextlib.suppress(Exception):
            proc.wait(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
