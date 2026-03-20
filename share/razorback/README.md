# Razorback JSON Schemas

These schemas describe two related JSON contracts emitted by this library:

- the canonical JSON values emitted by the `JsonBuffer_Put_*()` helpers in
  `src/json_buffer.c`
- the canonical top-level JSON message bodies emitted by the serializers in
  `src/messages/`

Each schema models the JSON value stored under a field name, not the outer
parent object that contains that field, unless the schema name starts with
`message-`. Message schemas describe the full serialized JSON body for a
specific wire format.

Examples:

- `uuid.schema.json` describes the object stored by `JsonBuffer_Put_UUID()`
- `hash.schema.json` describes the object stored by `JsonBuffer_Put_Hash()`
- `event.schema.json` describes the object stored by `JsonBuffer_Put_Event()`
- `message-cache-req.schema.json` describes the full JSON body emitted by
  `CacheReq_Serialize()`
- `message-hello.schema.json` describes the full JSON body emitted by
  `Hello_Serialize()`

The `JsonBuffer_Get_*()` functions may accept a slightly more permissive input
set than these schemas allow. The schemas intentionally capture the canonical
serialized form produced by this library.
