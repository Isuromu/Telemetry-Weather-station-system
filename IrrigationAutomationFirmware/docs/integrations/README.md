# Integrations

This directory holds contracts and concise references for components that are
outside the current firmware repository: mobile applications, local or remote
servers, and their interfaces to the Raspberry Pi gateway.

It intentionally keeps those materials separate from firmware, PlatformIO, and
generated Node-RED dashboard sources. Integration code may later live in its
own repository while retaining these protocol references.

- `watering-plan/` contains the executable watering-plan contract and staged
  gateway implementation roadmap.
- `mobile/` contains the mobile product-design reference, the React Native
  watering-plan implementation guide, and the farm spatial-data model used by
  map views.
