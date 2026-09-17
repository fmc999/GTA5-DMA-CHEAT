#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BE 封禁查询（BattlEye Ban Lookup —— battleye.dudx.info）

为什么要用真浏览器：该站点的访问令牌由页面内的混淆 WASM 生成（配合 /api/pow 的
RSA-PoW 挑战），纯 HTTP 客户端无法伪造 → 只能用真浏览器引擎代跑。
这里用 Playwright 驱动系统自带 Edge（不额外下载浏览器）。

用法：
    python be_lookup.py --rid 306305404 --rid 123456789 --out be_bans.json
输出（JSON）：
    { "306305404": {"banned": true,  "reason": "Global Ban #365afc", "raw": "Account Banned"},
      "123456789": {"banned": false, "reason": "", "raw": "..."} }
"""
import argparse
import io
import json
import os
import sys
import time

SITE = "https://battleye.dudx.info/"
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"


def lookup(page, rid: str, wait_s: float = 12.0) -> dict:
    """在已打开的站点页面上查询一个 RID，返回结果字典。"""
    # 切到 RID 模式
    page.evaluate("""() => {
        const b = [...document.querySelectorAll('button')].find(x => x.textContent.trim() === 'RID');
        if (b) b.click();
    }""")
    page.wait_for_timeout(300)
    inp = page.query_selector("input[type=text]")
    if inp is None:
        return {"banned": None, "reason": "", "raw": "找不到输入框"}
    inp.fill("")
    inp.type(rid, delay=15)
    page.wait_for_timeout(200)
    inp.press("Enter")

    # 等结果区出现「RID: <目标RID>」（避免读到上一次的残留结果）
    deadline = time.time() + wait_s
    last = ""
    shown = False
    while time.time() < deadline:
        page.wait_for_timeout(400)
        try:
            txt = page.evaluate("""() => {
                const t = document.body.innerText;
                const i = t.indexOf('Account Checker');
                return i >= 0 ? t.slice(i, i + 400) : t.slice(0, 400);
            }""")
        except Exception as e:
            last = "读取失败: %s" % e
            continue
        last = txt
        if ("RID: " + rid) in txt:
            shown = True
            # 出现状态词即认为渲染完成
            if any(k in txt for k in ("Account Banned", "Not Banned", "No Ban", "No ban", "Clean", "Not Found", "not found", "Unknown")):
                break

    banned = None
    reason = ""
    if "Account Banned" in last:
        banned = True
    elif any(k in last for k in ("Not Banned", "No Ban", "No ban", "Clean", "Not Found", "not found", "Unknown", "No record")):
        banned = False
    m = None
    for line in last.splitlines():
        if "Global Ban #" in line or "Ban #" in line:
            m = line.strip()
            break
    if m:
        reason = m

    return {"banned": banned, "reason": reason, "shown": shown, "raw": " | ".join(last.splitlines())[:300]}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rid", action="append", default=[], help="要查询的 RID（可多次）")
    ap.add_argument("--rids-file", default="", help="每行一个 RID 的文件")
    ap.add_argument("--out", default="be_bans.json", help="结果 JSON 输出路径")
    ap.add_argument("--headful", action="store_true", help="显示浏览器窗口（调试用）")
    ap.add_argument("--timeout", type=float, default=15.0)
    args = ap.parse_args()

    rids = [str(r).strip() for r in args.rid if str(r).strip()]
    if args.rids_file and os.path.isfile(args.rids_file):
        with io.open(args.rids_file, encoding="utf-8") as f:
            rids += [ln.strip() for ln in f if ln.strip()]
    # 去重保序
    seen, uniq = set(), []
    for r in rids:
        if r not in seen:
            seen.add(r)
            uniq.append(r)
    if not uniq:
        print("没有要查询的 RID", file=sys.stderr)
        return 2

    from playwright.sync_api import sync_playwright

    out = {}
    with sync_playwright() as p:
        try:
            browser = p.chromium.launch(channel="msedge", headless=not args.headful)
        except Exception:
            browser = p.chromium.launch(executable_path=EDGE, headless=not args.headful)
        page = browser.new_page(locale="zh-CN")
        page.goto(SITE, wait_until="domcontentloaded", timeout=90000)
        page.wait_for_timeout(4000)  # 等 WASM 完成令牌交换

        for rid in uniq:
            try:
                res = lookup(page, rid, args.timeout)
            except Exception as e:
                res = {"banned": None, "reason": "", "raw": "异常: %s" % e}
            out[rid] = res
            print("%-12s banned=%s  %s" % (rid, res.get("banned"), (res.get("reason") or res.get("raw") or "")[:110]))

        # 换行前把结果写盘（含时间戳）
        payload = {"fetched_at": time.strftime("%Y-%m-%dT%H:%M:%S"), "results": out}
        with io.open(args.out, "w", encoding="utf-8") as f:
            f.write(json.dumps(payload, ensure_ascii=False, indent=2))
        browser.close()

    print("已写出 %s（%d 个 RID）" % (args.out, len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
