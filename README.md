# Razorback

<p align="center">
  <img width="250" height="250" src="https://raw.githubusercontent.com/Cisco-Talos/razorback-api/main/logo.png" alt='The Razorback mascot'>
</p>

<p align="center">
  Razorback® is an open source framework for an intelligence driven security solution.
  malware & other malicious threats.
</p>

## Installation Instructions

We recommend running Razorback on Ubuntu LTS.

### Quick Install

Install with default options and configuration:

```bash
## If using a git checkout (not required from release tarball).
./autojunk.sh
./configure
make
make install
```

### Long/Developer Install
Debugging can be enabled with:

```./configure --enable-debug --enable-assert```

For all avaliable options see:

```./configure --help```


### Dependencies:

Ubuntu focal:
* automake
* automake
* autoconf
* libtool
* pkg-config
* uuid-dev
* libcurl4-openssl-dev
* libssl-dev
* libconfig-dev
* libssh-dev
* libjson-c-dev
* check - Unit testing framework (Required for testing, not runtime)
* jv - JSON validator (Required for testing, not runtime)
    `go install github.com/santhosh-tekuri/jsonschema/cmd/jv@latest`

## Library Initialization

Applications using the Razorback API must call `RZB_Init_API()` exactly once
before calling any other Razorback API entry point. Razorback no longer
performs this initialization automatically from a shared-library constructor or
Windows DLL attach hook.

```c
#include <razorback/api.h>

int main(void)
{
    RZB_Init_API();
    /* ... */
}
```

## Telemetry

Razorback is instrumented with OpenTelemetry and requires `opentelemetry-cpp`
with OTLP HTTP exporter support at build time. When the application calls
`RZB_Init_API()`, Razorback initializes telemetry and creates spans for:

* queue sends
* queue receives
* message processing

Trace context is propagated across Razorback message headers using W3C Trace
Context (`traceparent` / `tracestate`). The current implementation exports
traces only; metrics and logs are not configured by this library.

### Telemetry Environment Variables

Razorback-specific behavior:

* `OTEL_SDK_DISABLED`
  Disables telemetry initialization. If this is set to `true`, `1`, or `yes`,
  Razorback leaves tracing disabled.
* `OTEL_SERVICE_NAME`
  Overrides the exported `service.name` resource attribute. If unset,
  Razorback uses `razorback-api`.

Standard OTLP HTTP trace exporter configuration:

This build always instantiates the OTLP HTTP trace exporter. Use OTLP/HTTP
configuration variables; there is no Razorback-side switch to OTLP/gRPC.

* `OTEL_EXPORTER_OTLP_ENDPOINT`
  Base OTLP endpoint. For OTLP/HTTP, the exporter derives the trace URL from
  this base endpoint.
* `OTEL_EXPORTER_OTLP_TRACES_ENDPOINT`
  Trace-specific OTLP endpoint. Use this when traces should be sent to a
  different URL than other signals.
* `OTEL_EXPORTER_OTLP_HEADERS`
  Comma-separated HTTP headers applied to all OTLP requests.
* `OTEL_EXPORTER_OTLP_TRACES_HEADERS`
  Comma-separated HTTP headers applied only to trace exports.
* `OTEL_EXPORTER_OTLP_TIMEOUT`
  Export timeout, in milliseconds, for OTLP requests.
* `OTEL_EXPORTER_OTLP_TRACES_TIMEOUT`
  Export timeout, in milliseconds, for trace exports.
* `OTEL_EXPORTER_OTLP_COMPRESSION`
  OTLP request compression setting.
* `OTEL_EXPORTER_OTLP_TRACES_COMPRESSION`
  Trace-specific OTLP request compression setting.
* `OTEL_EXPORTER_OTLP_CERTIFICATE`
  Path to the CA certificate file used to validate the OTLP server.
* `OTEL_EXPORTER_OTLP_TRACES_CERTIFICATE`
  Trace-specific CA certificate path.
* `OTEL_EXPORTER_OTLP_CLIENT_KEY`
  Path to the client private key for mTLS.
* `OTEL_EXPORTER_OTLP_TRACES_CLIENT_KEY`
  Trace-specific client private key path.
* `OTEL_EXPORTER_OTLP_CLIENT_CERTIFICATE`
  Path to the client certificate or certificate chain for mTLS.
* `OTEL_EXPORTER_OTLP_TRACES_CLIENT_CERTIFICATE`
  Trace-specific client certificate path.

Batch span processor tuning:

* `OTEL_BSP_MAX_QUEUE_SIZE`
  Maximum number of spans buffered before new spans are dropped.
* `OTEL_BSP_SCHEDULE_DELAY`
  Delay, in milliseconds, between export attempts.
* `OTEL_BSP_EXPORT_TIMEOUT`
  Export timeout, in milliseconds, used by the batch span processor.
* `OTEL_BSP_MAX_EXPORT_BATCH_SIZE`
  Maximum number of spans sent in a single export batch.

The exporter is created with the default `opentelemetry-cpp` OTLP HTTP options,
so the standard OpenTelemetry trace exporter environment variables supported by
that SDK are used directly.


## Linking to the Razorback API

PKG-CONFIG:
The install target installs a package metadata file in ${libdir}/pkgconfig (/usr/local/lib/pkkconfig by default).
The pkg-config utility can be used to acquire the correct CFLAGS and LDFLAGS needed to compile a nugget.

pkg-config --cflags razorback
pkg-config --libs razorback

The PKG_CHECK_MODULES macro can be used to acquire the CFLAGS and LDFLAGS from within a nugget configure script.

PKG_CHECK_MODULES([RZB], [razorback])

The above call will provide RZB_CFLAGS and RZB_LDFLAGS to use in the autoconf/automake process.

## Want to make a contribution?

The Razorback development team welcomes
[code contributions](https://github.com/Cisco-Talos/razorback-api),
improvements to
[our documentation](https://github.com/Cisco-Talos/razorback-documentation),
and also [bug reports](https://github.com/Cisco-Talos/razorback-api/issues).

Thanks for joining us!

## Licensing

Razorback is licensed for public/open source use under the GNU General Public
License, Version 2 (GPLv2).

See `LICENSE` for a copy of the license.
