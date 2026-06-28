#include <razorback/telemetry.h>

void
Telemetry_ClearContext(TelemetryContextCarrier_t **context)
{
    if (context != NULL) {
        *context = NULL;
    }
}

double
Telemetry_GetMonotonicTimeSeconds(void)
{
    return 0.0;
}

void
Telemetry_RecordRuntimeDependencyState(
    const char *dependency,
    const char *status,
    const char *reasonCode
)
{
    (void)dependency;
    (void)status;
    (void)reasonCode;
}

void
Telemetry_RecordRuntimeWorkflowState(
    const char *workflow,
    const char *state,
    const char *reasonCode
)
{
    (void)workflow;
    (void)state;
    (void)reasonCode;
}

void
Telemetry_RecordRuntimeReadinessState(
    const char *state,
    const char *outcome,
    const char *reasonCode
)
{
    (void)state;
    (void)outcome;
    (void)reasonCode;
}

void
Telemetry_RecordRuntimeStartupDuration(
    double durationSeconds,
    const char *outcome,
    const char *reasonCode
)
{
    (void)durationSeconds;
    (void)outcome;
    (void)reasonCode;
}

void
Telemetry_RecordRuntimeShutdownDrainDuration(
    double durationSeconds,
    const char *service,
    const char *outcome,
    const char *reasonCode
)
{
    (void)durationSeconds;
    (void)service;
    (void)outcome;
    (void)reasonCode;
}

void
Telemetry_RecordRuntimeTelemetryFlushOutcome(
    const char *service,
    const char *outcome,
    const char *reasonCode
)
{
    (void)service;
    (void)outcome;
    (void)reasonCode;
}
