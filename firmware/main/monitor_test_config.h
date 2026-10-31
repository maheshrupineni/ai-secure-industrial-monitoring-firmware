#ifndef INDUSTRIAL_MONITOR_TEST_CONFIG_H
#define INDUSTRIAL_MONITOR_TEST_CONFIG_H

/*
 * Development-only queue stress scenario.
 * Set to 1 to inject one validation pause per boot.
 * Keep disabled for normal operation.
 */
#define MONITOR_TEST_VALIDATION_STALL_ENABLED  0
#define MONITOR_TEST_STALL_SEQUENCE            5U
#define MONITOR_TEST_STALL_DURATION_MS         12000U

#endif /* INDUSTRIAL_MONITOR_TEST_CONFIG_H */
