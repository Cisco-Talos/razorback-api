#include <razorback/telemetry.h>

void
Telemetry_ClearContext(TelemetryContextCarrier_t **context)
{
    if (context != NULL) {
        *context = NULL;
    }
}
