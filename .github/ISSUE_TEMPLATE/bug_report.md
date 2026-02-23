---
name: Bug report
about: Report a reproducible HeatControl bug
title: '[BUG] Short title'
labels: bug
assignees: ''

---

## Summary
Clear and concise description of the bug.

## Firmware / Environment
- Firmware version (from UI): [e.g. 0.0.xx]
- Hardware board: [e.g. ESP32-C3 board]
- Browser + OS: [e.g. Chrome 133 on Windows 11]
- Power setup: [battery/lab supply details]

## Network setup at time of issue
- AP SSID/IP: [e.g. your-ap-ssid / 4.3.2.1]
- Home WiFi (STA) SSID/IP: [e.g. home-wifi / 192.168.x.x]
- Connection state in header: [AP status, Home status]

Please do not post private data (real passwords, full public IPs, sensitive hostnames). Mask values where needed.

## Steps to reproduce
1. ...
2. ...
3. ...
4. ...

## Expected behavior
What should happen?

## Actual behavior
What happens instead?

## Current configuration
- Mode: [AUTO / POWER / MANUAL]
- Targets: [H1 xx.x C, H2 xx.x C]
- Sensor mapping: [normal / swapped]
- Battery cells: [H1 xS, H2 xS]
- Manual OFF/ON window: [ms]
- AP auto-off timeout: [minutes]

## Serial log (required)
Please paste relevant output from `/logs` (or serial monitor), especially around the failure.

```text
Paste serial log here
```

## Screenshots / video
If relevant, attach UI screenshots or a short video.

## Additional context
Anything else that may help reproduce/debug.
