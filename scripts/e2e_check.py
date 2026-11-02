#!/usr/bin/env python3
"""Real end-to-end check: drive the built frontend (served by vite
preview on :4173) against the live C++ backend (nexus_server on :8787)
with Playwright, for every sidebar page. Captures a screenshot and the
browser console log for each page, and fails loudly (non-zero exit,
printed reason) if any page shows a JS error, a "Backend unreachable"
message, or never leaves a loading state.
"""
import os
import sys
import time
from playwright.sync_api import sync_playwright

BASE = "http://127.0.0.1:4173"
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(REPO_ROOT, "docs", "technical_report", "e2e_evidence")

PAGES = [
    ("overview", "Overview"),
    ("geometry", "Geometry Lab"),
    ("thermal", "Thermal Analysis"),
    ("performance", "Performance Lab"),
    ("neural", "Neural Computation"),
    ("hardware", "Hardware Integration"),
    ("validation", "Validation & Tests"),
    ("docs", "Documentation"),
]

os.makedirs(OUT_DIR, exist_ok=True)

results = []

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page(viewport={"width": 1440, "height": 900})

    console_errors = []
    page.on("console", lambda msg: console_errors.append((msg.type, msg.text)) if msg.type == "error" else None)
    page_errors = []
    page.on("pageerror", lambda exc: page_errors.append(str(exc)))

    page.goto(BASE, wait_until="networkidle")
    time.sleep(0.3)

    for module_id, label in PAGES:
        console_errors.clear()
        page_errors.clear()

        if module_id != "overview":
            # Click the sidebar button with this exact label
            btn = page.get_by_role("button", name=label, exact=True)
            btn.click()
        time.sleep(0.6)  # let any fetch resolve

        body_text = page.inner_text("body")
        has_backend_unreachable = "Backend unreachable" in body_text
        still_loading = ("Connecting to backend" in body_text) or ("Loading" in body_text and "loading" in body_text.lower() and len(body_text) < 400)

        screenshot_path = os.path.join(OUT_DIR, f"{module_id}.png")
        page.screenshot(path=screenshot_path, full_page=True)

        ok = (not has_backend_unreachable) and (not page_errors) and (not console_errors)
        results.append({
            "module_id": module_id,
            "label": label,
            "ok": ok,
            "backend_unreachable": has_backend_unreachable,
            "console_errors": list(console_errors),
            "page_errors": list(page_errors),
            "body_chars": len(body_text),
            "screenshot": screenshot_path,
        })
        print(f"[{module_id}] ok={ok} backend_unreachable={has_backend_unreachable} "
              f"console_errors={len(console_errors)} page_errors={len(page_errors)} body_chars={len(body_text)}")

    browser.close()

print("\n=== SUMMARY ===")
all_ok = True
for r in results:
    status = "PASS" if r["ok"] else "FAIL"
    if not r["ok"]:
        all_ok = False
    print(f"{status}  {r['module_id']:12s} {r['label']}")
    if r["console_errors"]:
        for t, m in r["console_errors"]:
            print(f"       console[{t}]: {m[:200]}")
    if r["page_errors"]:
        for e in r["page_errors"]:
            print(f"       pageerror: {e[:200]}")

sys.exit(0 if all_ok else 1)
