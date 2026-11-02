#!/usr/bin/env python3
"""Interactive-flow check: click real buttons and confirm real backend
responses render, not just the static initial page load."""
from playwright.sync_api import sync_playwright
import time, os

BASE = "http://127.0.0.1:4173"
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(REPO_ROOT, "docs", "technical_report", "e2e_evidence")
os.makedirs(OUT_DIR, exist_ok=True)

errors = []

with sync_playwright() as p:
    browser = p.chromium.launch()
    page = browser.new_page(viewport={"width": 1440, "height": 900})
    page.on("console", lambda m: errors.append(("console", m.text)) if m.type == "error" else None)
    page.on("pageerror", lambda e: errors.append(("pageerror", str(e))))

    page.goto(BASE, wait_until="networkidle")

    # --- Geometry Lab: click "Load sample" for STL, then for CSV ---
    page.get_by_role("button", name="Geometry Lab", exact=True).click()
    time.sleep(0.3)
    page.get_by_role("button", name="Load sample (self-authored tetrahedron)").click()
    time.sleep(1.0)
    body1 = page.inner_text("body")
    page.screenshot(path=f"{OUT_DIR}/geometry_after_stl_load.png", full_page=True)

    page.get_by_role("button", name="Load sample (synthetic)").click()
    time.sleep(1.0)
    body2 = page.inner_text("body")
    page.screenshot(path=f"{OUT_DIR}/geometry_after_csv_load.png", full_page=True)

    stl_ok = "No STL loaded yet" not in body1
    csv_ok = "No CSV loaded yet" not in body2
    print(f"STL sample loaded (no longer shows 'No STL loaded yet'): {stl_ok}")
    print(f"CSV sample loaded (no longer shows 'No CSV loaded yet'): {csv_ok}")

    # --- Neural Computation: click "Re-run training" ---
    page.get_by_role("button", name="Neural Computation", exact=True).click()
    time.sleep(0.3)
    before = page.inner_text("body")
    page.get_by_role("button", name="Re-run training").click()
    time.sleep(1.0)
    after = page.inner_text("body")
    rerun_ok = "Final y_hat" in after
    print(f"Neural 're-run training' produced a result: {rerun_ok}")

    # --- Hardware Integration: click "Re-run sequence" ---
    page.get_by_role("button", name="Hardware Integration", exact=True).click()
    time.sleep(0.3)
    page.get_by_role("button", name="Re-run sequence").click()
    time.sleep(1.0)
    hw_after = page.inner_text("body")
    hw_ok = "CONNECTED" in hw_after
    print(f"Hardware 're-run sequence' produced a result: {hw_ok}")

    browser.close()

print(f"\nconsole/page errors during interactive flow: {len(errors)}")
for t, m in errors:
    print(f"  [{t}] {m[:200]}")

all_ok = stl_ok and csv_ok and rerun_ok and hw_ok and not errors
print(f"\nALL INTERACTIVE CHECKS OK: {all_ok}")
