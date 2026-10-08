"""Deploy one generated flow into its existing Node-RED tab, via the editor UI.

This is the operator's own sequence, driven over CDP, with no admin-API writes:
select the tab, delete the nodes the generator is about to replace, Import the
JSON, resolve the conflict prompt with "View nodes..." then "Import selected",
then Deploy.

Two rules keep it safe:
  - The shared config singletons (ui-base, ui-theme, mqtt-broker, influxdb,
    ui-page) have no z, are used by every tab, and are left untouched: their
    conflict rows carry a "replace" toggle which is deliberately left off.
  - The generators ship no tab node, so the tab itself, its environment
    variables and its sidebar position always survive.

usage: deploy_flow.py <flow.json> <tab-id> [--no-deploy] [--expect-tabs N]
"""
import argparse
import json
import sys
import time

sys.path.insert(0, "/home/kamolovp/NX-devel/Electronics/Esp32/Projects/Telemetry-Weather-station/IrrigationAutomationFirmware/irrtest")
from cdp import Chrome  # noqa: E402

EDITOR = "http://192.168.2.217:1880/"

ALL_IDS_JS = """(()=>{const o={};
  RED.nodes.eachNode(function(n){o[n.id]=1});
  RED.nodes.eachConfig(function(n){o[n.id]=1});
  RED.nodes.eachGroup(function(n){o[n.id]=1});
  return JSON.stringify(o)})()"""

TAB_IDS_JS = """(()=>{const tab=__TAB__;const o=[];
  RED.nodes.eachNode(function(n){if(n.z===tab)o.push(n.id)});
  RED.nodes.eachConfig(function(n){if(n.z===tab)o.push(n.id)});
  RED.nodes.eachGroup(function(n){if(n.z===tab)o.push(n.id)});
  return JSON.stringify(o)})()"""


def j(expr):
    """Wrap an expression body so the result is always a JSON-safe primitive."""
    return "(()=>{%s})()" % expr


def ids_on_tab(c, tab):
    return json.loads(c.js(TAB_IDS_JS.replace("__TAB__", json.dumps(tab))))


def click_js(c, sel):
    """jQuery trigger, not a CDP mouse click: mouse events never reach the
    jQuery UI button handlers in this editor."""
    c.js(j("$('%s').trigger('click'); return 1" % sel))


def wait_editor(c, expect_tabs=None, timeout=90):
    t0 = time.time()
    n = -1
    while time.time() - t0 < timeout:
        try:
            n = c.js(j("if(typeof RED==='undefined')return -1;"
                       "let k=0;RED.nodes.eachWorkspace(function(){k++});return k"))
        except Exception:
            n = -1
        if isinstance(n, int) and n > 0 and (expect_tabs is None or n == expect_tabs):
            time.sleep(1)
            return n
        time.sleep(1)
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("flow")
    ap.add_argument("tab")
    ap.add_argument("--no-deploy", action="store_true")
    ap.add_argument("--expect-tabs", type=int, default=None)
    a = ap.parse_args()

    nodes = json.load(open(a.flow))
    text = json.dumps(nodes, ensure_ascii=False)
    want_tab = [n["id"] for n in nodes if n.get("z")]
    # Config singletons the generator reuses keep their ids; they are not replaced.
    want_conf = [n["id"] for n in nodes if not n.get("z")
                 and n.get("type") not in ("global-config", "ui-base", "ui-theme")]

    c = Chrome()
    if not c.js(j("return typeof RED!=='undefined' && "
                  "!!document.getElementById('red-ui-header-button-deploy')")):
        c.goto(EDITOR + "#flow/" + a.tab)
        time.sleep(5)
    print("editor tabs:", wait_editor(c, a.expect_tabs))
    print("generated: %d nodes (%d on the tab, %d reused config)"
          % (len(nodes), len(want_tab), len(want_conf)))

    # --- 1. select the tab and remove what the import is about to replace -----
    c.js("RED.workspaces.show(%r)" % a.tab)
    time.sleep(1)
    active = c.js("RED.workspaces.active()")
    if active != a.tab:
        raise SystemExit("could not activate tab %s (active=%s)" % (a.tab, active))
    before = ids_on_tab(c, a.tab)
    print("tab owns %d ids before delete" % len(before))
    c.js(j("const tab=%s;const objs=[];"
           "RED.nodes.eachNode(function(n){if(n.z===tab)objs.push(n)});"
           "RED.nodes.eachConfig(function(n){if(n.z===tab)objs.push(n)});"
           "RED.nodes.eachGroup(function(n){if(n.z===tab)objs.push(n)});"
           "RED.view.select({nodes:objs});return 1" % json.dumps(a.tab)))
    c.js("RED.actions.invoke('core:delete-selection')")
    time.sleep(1.5)
    print("tab owns %d ids after delete" % len(ids_on_tab(c, a.tab)))

    # --- 2. import -----------------------------------------------------------
    c.js("RED.actions.invoke('core:show-import-dialog')")
    time.sleep(1.5)
    c.js(j("const t=document.getElementById('red-ui-clipboard-dialog-import-text');"
           "t.value='';t.focus();return 1"))
    c.cmd("Input.insertText", text=text)
    c.js(j("const t=document.getElementById('red-ui-clipboard-dialog-import-text');"
           "t.dispatchEvent(new KeyboardEvent('keyup',{bubbles:true}));return 1"))
    time.sleep(0.6)
    if not c.js(j("const b=document.getElementById('red-ui-clipboard-dialog-ok');"
                  "return !(b.disabled||/disabled/.test(b.className))")):
        raise SystemExit("Import button never enabled - nothing imported")
    click_js(c, "#red-ui-clipboard-dialog-ok")
    time.sleep(2.0)
    print("notifications:", c.js(j(
        "const o=[];document.querySelectorAll('.red-ui-notification').forEach(function(n){"
        "o.push((n.innerText||'').replace(/\\s+/g,' ').slice(0,70))});return JSON.stringify(o)")))

    # --- 3. resolve the conflict prompt, leaving the shared singletons alone --
    pos = json.loads(c.js(j(
        "for(const x of document.querySelectorAll('.red-ui-notification button,.red-ui-notification a')){"
        "const t=(x.innerText||'').trim();if(t.indexOf('View')===0){const b=x.getBoundingClientRect();"
        "return JSON.stringify({x:b.left+b.width/2,y:b.top+b.height/2})}}return 'null'")))
    if isinstance(pos, dict):
        print("conflict prompt: clicking View nodes")
        for t in ("mousePressed", "mouseReleased"):
            c.cmd("Input.dispatchMouseEvent", type=t, x=float(pos["x"]), y=float(pos["y"]),
                  button="left", clickCount=1)
        time.sleep(2.0)
        rows = c.js(j(
            "const l=document.getElementById('red-ui-clipboard-dialog-import-conflicts-list');"
            "if(!l)return 'none';"
            "return JSON.stringify([...l.querySelectorAll('li')].map(function(li){"
            "const cb=li.querySelector('input[type=checkbox]');"
            "const rep=[...li.querySelectorAll('label')].map(function(x){return (x.innerText||'').trim()}).join('');"
            "return (li.innerText||'').replace(/\\s+/g,' ').trim().slice(0,32)"
            "+' checked='+(cb?cb.checked:'-')+(rep?' ['+rep+']':'')}))"))
        print("conflict rows:", rows)
        click_js(c, "#red-ui-clipboard-dialog-import-conflict")
        time.sleep(3.0)
    else:
        print("no conflict prompt - imported directly")

    # --- 4. verify the editor model before anything reaches the server -------
    # `after` is an id -> 1 map of every node, config and group in the workspace, so a
    # missing id means the import did not land. Checking the name against the whole
    # file rather than just the tab catches a config node the dialog dropped.
    after = json.loads(c.js(ALL_IDS_JS))
    want_all = [n["id"] for n in nodes if isinstance(n, dict) and n.get("id")]
    missing = [i for i in want_tab if i not in after]
    absent = [i for i in want_all if i not in after]
    print("model: %d ids" % len(after))
    print("expected tab ids present: %d/%d" % (len(want_tab) - len(missing), len(want_tab)))
    print("missing from the tab:", missing or "none")
    print("expected ids absent from the workspace:", absent or "none")
    print("reused config present: %d/%d" % (len([i for i in want_conf if i in after]), len(want_conf)))
    print("nodes still guarding on context:", c.js(j(
        "const o=[];RED.nodes.eachNode(function(n){"
        "if(n.func && n.func.indexOf(\"context.get('topic')\")>=0)o.push(n.id)});return JSON.stringify(o)")))
    print("dirty:", c.js("RED.nodes.dirty()"))
    if missing:
        raise SystemExit("import incomplete - NOT deploying")
    if a.no_deploy:
        print("--no-deploy: left staged in the editor")
        c.close()
        return

    # --- 5. deploy ----------------------------------------------------------
    print("deploy mode modified-flows:",
          c.js("document.getElementById('deploymenu-item-flow').className.includes('active')"))
    click_js(c, "#red-ui-header-button-deploy")
    for _ in range(30):
        time.sleep(1)
        try:
            if c.js("RED.nodes.dirty()") is False:
                break
        except Exception:
            pass
    print("dirty after deploy:", c.js("RED.nodes.dirty()"))
    print("notifications:", c.js(j(
        "const o=[];document.querySelectorAll('.red-ui-notification').forEach(function(n){"
        "o.push((n.innerText||'').replace(/\\s+/g,' ').slice(0,70))});return JSON.stringify(o)")))
    c.close()


if __name__ == "__main__":
    main()
