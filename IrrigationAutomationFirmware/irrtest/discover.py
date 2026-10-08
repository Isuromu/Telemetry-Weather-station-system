"""Step A: navigate + inspect only. No clicks. Finds the real Start/Stop controls."""
import json
import sys
import time

sys.path.insert(0, "/home/kamolovp/NX-devel/Electronics/Esp32/Projects/Telemetry-Weather-station/IrrigationAutomationFirmware/irrtest")
from cdp import Chrome, HOST, ts  # noqa: E402

PROBE = r"""
(() => {
  const vis = el => {
    if (!el) return false;
    const r = el.getBoundingClientRect();
    return r.width > 0 && r.height > 0;
  };
  const btns = [...document.querySelectorAll('button, a[role=button], .v-btn, input[type=button], input[type=submit]')]
    .filter(vis)
    .map((b, i) => ({
      i,
      text: (b.innerText || b.value || '').replace(/\s+/g, ' ').trim().slice(0, 40),
      cls: b.className && b.className.toString ? b.className.toString().slice(0, 80) : '',
      id: b.id || '',
      disabled: !!b.disabled,
      tag: b.tagName
    }));
  const banner = [...document.querySelectorAll('[class*=banner],[class*=notice],[class*=status],.ir-banner')]
    .filter(vis).map(b => ({cls: b.className.toString().slice(0,60), text: (b.innerText||'').replace(/\s+/g,' ').trim().slice(0,120)}));
  const boxes = [...document.querySelectorAll('input[type=checkbox]')]
    .map(c => ({checked: c.checked, id: c.id, name: c.name, cls: c.className.toString().slice(0,60)}));
  return JSON.stringify({
    url: location.href, title: document.title, ready: document.readyState,
    buttons: btns, banners: banner, checkboxes: boxes,
    bodyText: (document.body ? document.body.innerText : '').replace(/\s+/g,' ').trim().slice(0, 700)
  });
})()
"""

for route in ("/dashboard/irrigation", "/dashboard/page1"):
    c = Chrome()
    print("\n" + "=" * 74)
    print("NAVIGATE", route)
    c.goto(HOST + route)
    got = None
    for i in range(25):
        v = c.js(PROBE)
        if v:
            d = json.loads(v)
            if d.get("bodyText"):
                got = d
                if any(b["text"].lower().startswith(("start", "stop")) for b in d["buttons"]):
                    break
        time.sleep(1)
    if not got:
        print("  nothing rendered in 25 s")
    else:
        print("  url:", got["url"], "| title:", got["title"], "| ready:", got["ready"])
        print("  BUTTONS:")
        for b in got["buttons"]:
            print("    [%s] %-22r cls=%r id=%r disabled=%s" % (b["i"], b["text"], b["cls"], b["id"], b["disabled"]))
        print("  BANNERS:")
        for b in got["banners"]:
            print("    cls=%r text=%r" % (b["cls"], b["text"]))
        print("  CHECKBOXES:", json.dumps(got["checkboxes"]))
        print("  BODY:", got["bodyText"][:500])
    c.close()
