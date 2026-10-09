# Farm spatial-data model

## Purpose

The mobile field-map screen needs more than the gateway's execution registry.
It needs named field boundaries, map presentation information, and a stable
connection from a field polygon to its watering hardware. This document defines
that separation for the React Native app, temporary Raspberry Pi gateway, and
future server.

## Storage recommendation

Field geometry is not heavy. Fifteen field polygons with dozens of GPS points
each are normally only kilobytes or a few megabytes, which is trivial for a
Raspberry Pi and SQLite. The potentially heavy data is different:

- satellite or street-map raster tiles can occupy hundreds of megabytes or more
  when cached offline;
- raw GPS tracks or frequent location histories can grow indefinitely; and
- photos, drone images, and derived vegetation rasters need object storage.

Store the small canonical field catalog, including GeoJSON boundaries, on the
Raspberry Pi during the serverless prototype. This allows the local React Native
app and Node-RED dashboard to render the same field names and boundaries while
the Internet is unavailable. Do not store map tiles, drone imagery, or long
position histories on the Pi for this prototype.

When the future server exists, it becomes the authoritative catalog for farm,
field, crop, and spatial records. The Raspberry Pi keeps a synchronized,
read-only operational copy containing only enabled field geometry and hardware
bindings needed locally.

## Data ownership

| Data | Prototype authority | Future authority | Notes |
|---|---|---|---|
| Field ID, name, boundary, area | Raspberry Pi SQLite catalog | Server spatial catalog | GeoJSON geometry in WGS 84 (`EPSG:4326`). |
| Field-to-valve and sensor mapping | Raspberry Pi execution registry | Server publishes to gateway | Required to validate an executable plan. |
| Current irrigation state and telemetry | Node-RED / InfluxDB | Gateway telemetry stream plus server copy | Not geometry. |
| Map tiles and satellite imagery | React Native cache / map provider | Mobile cache / map provider | Do not make the Pi an imagery server. |
| Crop, growth, user notes, AI inputs | React Native local data | Server farm records | Not needed to command a plan. |
| GPS track history, photos, drone data | Not in prototype scope | Server/object storage | Store only if a real feature needs it. |

## Field record

Use a stable logical ID and ordinary GeoJSON. Coordinates are longitude then
latitude, as GeoJSON requires. The polygon is for display and spatial queries;
the scheduler continues to use the resolved valve IDs.

```json
{
  "field_id": "field-001",
  "name": "Olmazor 1",
  "enabled": true,
  "geometry_version": 1,
  "geometry": {
    "type": "Polygon",
    "coordinates": [[[69.0000, 41.0000], [69.0002, 41.0000], [69.0002, 41.0002], [69.0000, 41.0002], [69.0000, 41.0000]]]
  },
  "area_hectares": null,
  "watering_zone": {
    "valve_id": "valve1",
    "flow_meter_capability": "commissioned",
    "soil_sensor_id": null
  },
  "updated_at": "2026-10-09T00:00:00Z"
}
```

The coordinates and timestamp above are illustrative only. They are not site
measurements and must not be copied into deployment configuration.

## Mobile map behavior

The map screen should load field polygons from the local catalog, overlay the
latest derived field state, and use a map provider only for its visual base
layer. It should work in this order:

1. Draw stored field boundaries immediately.
2. Apply current moisture/irrigation status from the latest gateway response.
3. Load online map tiles when available; retain a normal blank or simple vector
   background when they are unavailable.
4. Cache tiles only within a mobile-controlled storage budget.

The map colour is presentation data derived from fresh sensor readings. It is
not a watering-safety decision and must show a stale/unknown state rather than
invent a moisture value.

## Update and version rules

- Field geometry changes use a new `geometry_version` and `updated_at` value.
- A deleted or disabled field remains in history but is excluded from `all
  fields` plan resolution.
- A boundary edit must not rewrite an accepted plan's resolved valve snapshot.
- A valve reassignment is an operational mapping change. It cannot take effect
  while that field has a running plan; cancel or finish the run first.
- The gateway rejects a plan that names a disabled field or has no valid valve
  mapping.

## Prototype scope

The first map implementation supports named field polygons and current status
overlays. It does not require route/trajectory tracking, offline satellite
imagery, drone layers, geofencing, or automated irrigation decisions based on
polygon area. Those can be added by the future server when there is a defined
product need.
