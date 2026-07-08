# C SDK Schemas

`schemas/razorback` must point at the pinned `razorback-schemas` repository for
dispatcher-next conformance fixtures.

Run the pinned schema and fixture conformance check with:

```bash
python3 tools/schema_conformance.py
```

The same check is wired into the C SDK `make check` workflow through
`tests/test_schema_conformance.c`.
