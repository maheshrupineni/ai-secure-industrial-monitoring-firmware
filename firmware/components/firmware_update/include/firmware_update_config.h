#ifndef INDUSTRIAL_FIRMWARE_UPDATE_CONFIG_H
#define INDUSTRIAL_FIRMWARE_UPDATE_CONFIG_H

/* Development-only automatic update. Keep disabled in normal builds. */
#define MONITOR_OTA_DEMO_ENABLED 0
#define MONITOR_OTA_URL "https://raw.githubusercontent.com/maheshrupineni/industrial-monitor/1dda4df3fbaad5107dba661e1f8eb74ee449a557/industrial_monitor.bin"
#define MONITOR_OTA_EXPECTED_VERSION "0.1.2"
#define MONITOR_OTA_EXPECTED_IMAGE_SHA256 "421eab26c2f8b89809fe399c1c52ceb3197b2068024f53e60f1f494893dffe97"

/* Development-only rollback injection; keep disabled in normal builds. */
#define MONITOR_OTA_TEST_REJECT_HEALTH 0
#endif
