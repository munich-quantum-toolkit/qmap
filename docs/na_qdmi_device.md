# Neutral-atom QDMI device

MQT QMAP's neutral-atom (NA) provider materializes its complete device model
when a QDMI session is initialized. Sites, zones, operations, units, and
calibration are owned by that session. Multiple stable IDs may therefore open
the same provider library and prefix with different device descriptions at the
same time.

## Configuration sources

The provider selects the first available source in this order:

1. QDMI CUSTOM1 containing NUL-terminated inline JSON;
2. QDMI CUSTOM2 containing a NUL-terminated JSON file path;
3. `MQT_QMAP_QDMI_NA_CONFIG_JSON`;
4. `MQT_QMAP_QDMI_NA_CONFIG_FILE`;
5. `mqt-qmap-qdmi-na-device.json` beside the provider shared library.

When direct QDMI callers populate both explicit slots, inline JSON wins. Setting
both NA environment sources is an error. Configuration cannot be changed after
successful initialization. A failed initialization does not commit partial
state, so the same allocated session can be corrected and initialized again.

Driver users should use the typed `device-config` registry field or the
`device_config` and `device_config_file` Python arguments described in MQT
Core's {doc}`QDMI configuration guide <core:qdmi/configuration>`. Direct QDMI v1
clients use CUSTOM1 and CUSTOM2.

## Schema and validation

Every description is a strict JSON object with `"schema-version": 1`. The
bundled example is `json/na/mqt-qmap-qdmi-na-device.json`, and the C++ value
model is {cpp-api:class}`na::Device`. Required top-level and nested fields
cannot be omitted, unknown fields are rejected, and validation covers:

- non-empty names and positive device capacity;
- finite positive unit scales and supported units;
- lattice geometry, unique generated coordinates, sufficient sites, and bounded
  site/pair expansion;
- operation arity, regions, fidelities, and interaction data;
- shuttling units and decoherence values.

Applications that need to validate a description without opening a provider
session can call `na::readJSON`; it is the same strict parser used at runtime.

Handles returned by a session are valid only for that session. Queries reject
sites or operations owned by another session.
