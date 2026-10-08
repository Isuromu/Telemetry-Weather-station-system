# Node-RED flow conventions

Every Dashboard 2.0 flow committed in `examples/` follows the rules below. They
exist because a flow export is a source file in git: it must not carry the
ChirpStack identity of the installed system, and the subscription must survive a
redeploy without an operator remembering to fix a topic by hand.

`tools/test_nodered_flows.js` enforces all of this; run it after editing any
flow.

```bash
node tools/test_nodered_flows.js
node tools/test_irrigation_dashboard.js
```

## Identity comes from environment variables

One shared application ID plus one DevEUI per device:

| Variable | Meaning |
| --- | --- |
| `IRRIGATION_APP_ID` | ChirpStack application UUID shared by all six devices |
| `WATER_DEV_EUI` | WaterLevel DevEUI |
| `SOIL_DEV_EUI` | SoilNode DevEUI |
| `MAIN_DEV_EUI` | MainValve DevEUI |
| `VALVE1_DEV_EUI` | valve_1 (pressure-control node) DevEUI |
| `VALVE2_DEV_EUI` | valve_2 DevEUI |
| `PUMP_DEV_EUI` | PumpControl DevEUI |

Read them inside a function node:

```js
const app = String(env.get("IRRIGATION_APP_ID") || "").toLowerCase();
const eui = String(env.get("MAIN_DEV_EUI") || "").toLowerCase();
if (!/^[a-f0-9-]{36}$/.test(app) || !/^[a-f0-9]{16}$/.test(eui)) {
    node.status({ fill: "red", shape: "ring", text: "IRRIGATION_APP_ID / MAIN_DEV_EUI missing" });
    return null;
}
```

- Always `.toLowerCase()`: main-valve, pump and soil decoders compare the raw
  MQTT topic against these values, and ChirpStack topics are lower case.
- Always validate before use, and fail loudly (`node.status` red or a
  `ui_error`) rather than publishing to a topic that does not exist.
- Never write a real UUID or DevEUI into a node. A test fixture that needs an
  identity builds it from the same environment variables.
- Where a placeholder literal is unavoidable (an example payload, a sample
  buffer), use all zeros — `0000000000000000` — the same convention as the
  `*LoRaSecrets.example.h` files.

Where to set the variables, in order of preference:

1. The function node's own **Environment** tab, next to the code that reads it.
2. A process environment variable for the Node-RED service (systemd unit,
   container env, or `settings.js`) — one place for every flow and node.
3. The flow or group **Environment** tab, available in Node-RED 3.1 and later.

Node-RED resolves function-node `env.get()` from those three sources in that
order, then falls back to `process.env`.

## The `mqtt in` node subscribes dynamically

An `mqtt in` node's **Topic** field is a literal string, evaluated when the flow
is deployed. It cannot resolve environment variables, flow context or
`{{mustache}}` output. Therefore a flow that must not hardcode the topic keeps
that field **empty** and drives the subscription from a function node:

```js
return { action: "subscribe", topic: topic, qos: 0 };
```

Three properties are load-bearing:

- **`"inputs": 1` on the `mqtt in` node.** Node-RED enables dynamic mode only
  when the node has an input wired: `node.isDynamic = n.hasOwnProperty("inputs")
  && n.inputs == 1`. With `"inputs": 0` and an empty topic the node never
  registers with the broker and the subscription silently does nothing. The
  editor sets this field when you drag a wire into the node; a hand-written or
  imported export must set it explicitly.
- **The subscription is issued once per deploy**, from an `inject` node with
  `once: true`, `onceDelay: "0.5"` and an empty `repeat`. Do not drive it from a
  repeating tick.
- **No `context.get("topic") === topic` idempotency guard.** Re-subscribing to
  the same topic is harmless (the MQTT node unsubscribes the old handler first),
  while flow context survives a deploy — so the guard can make a freshly
  recreated `mqtt in` node skip the only subscribe it will ever be sent, leaving
  the flow deaf until the next full restart.

Subscriptions requested before the broker connects are stored and replayed on
connect, so a slow broker start does not need a retry loop.

## Downlinks stay key-free too

The publish path mirrors the subscription: build the topic from the same
environment values and let the `mqtt out` node take the topic from the message.

```js
topic: "application/" + app + "/device/" + eui + "/command/down"
```

The `mqtt out` node's own Topic field stays empty; entering a topic there
overrides the message and reintroduces the identity into the export.

## Test fixtures

Some flows keep a manually triggered sample uplink so the decode path can be
exercised without a device. Build its payload in a function node from the
environment variables and inject it into the **debug** node only.

Never wire a fixture into the command path: a synthetic `finished` FPort 31
report advances the command queue and dispatches a real downlink to the field
device.

## Checklist for a new or updated flow

1. `mqtt in` node: Topic empty, Inputs 1.
2. One `inject` (`once: true`, `onceDelay: "0.5"`, no repeat) into one
   `Subscribe <device> events` function that returns `{action:"subscribe"}`.
3. Every identity `const` reads `env.get(...)` with `.toLowerCase()` and a
   validation guard.
4. Every `.../command/down` builder reads both values from `env.get(...)`.
5. `node tools/test_nodered_flows.js` passes — it fails on any UUID, on any
   non-zero 16-hex run in a code field, and on a missing environment read.
6. Re-importing replaces the flow: Node-RED's import creates a new tab even when
   the node ids match, so delete the previous tab before deploying.

## Related

- [PCV LoRaWAN and ChirpStack](PRESSURE_NODE_LORAWAN.md) — device profiles,
  codecs and FPort assignments.
- [Commissioning](COMMISSIONING.md) — ChirpStack and gateway setup.
