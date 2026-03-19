# Razorback JSON Schemas

These schemas describe the canonical JSON values emitted by the
`JsonBuffer_Put_*()` helpers in `src/json_buffer.c`.

Each schema models the JSON value stored under a field name, not the outer
parent object that contains that field.

Examples:

- `uuid.schema.json` describes the object stored by `JsonBuffer_Put_UUID()`
- `hash.schema.json` describes the object stored by `JsonBuffer_Put_Hash()`
- `event.schema.json` describes the object stored by `JsonBuffer_Put_Event()`

The `JsonBuffer_Get_*()` functions may accept a slightly more permissive input
set than these schemas allow. The schemas intentionally capture the canonical
serialized form produced by this library.
