# HTTPS OTA and rollback

Automatic OTA and deliberate health-rejection injection are both disabled in the shipped configuration. Leave them disabled for normal operation.

The 4 MB flash layout uses nvs at 0x9000, otadata at 0xd000, phy_init at 0xf000, ota_0 at 0x10000 and ota_1 at 0x1f0000. Each application slot is 0x1e0000 bytes. A separate worker waits for network/time readiness, validates candidate project/version and pinned image digest, writes the inactive slot, and restarts. A pending image must obtain ten matched healthy monitoring records within the configured health window or request rollback. A rejected-image check prevents automatically reinstalling the known rejected candidate.

Earlier demonstrations observed 0.1.0 -> 0.1.1 with confirmation, and deliberate 0.1.2 rejection followed by rollback. They used earlier binaries. The repository hosting those candidates was subsequently deleted; the historical URL in firmware_update_config.h no longer works. It is retained as historical configuration, with the automatic flag zero. Do not claim the current default build performs a downloadable update.

To repeat with a final candidate, first establish authorized HTTPS hosting, build a separately versioned compatible image, review the exact expected version and image digest using the same digest scheme as the downloader, then conduct update, confirmation and deliberate rollback tests with retained evidence. Do not enable the flag while the old URL/pin remains. No new hosting is created by release cleanup.

TLS certificate/hostname validation and an image pin are present. Firmware signing, Secure Boot and flash encryption are not configured. The earlier CPU1 idle-watchdog observation during download remains unexplained. A backtrace into idle wait did not identify a blocking application task. Negative update/TLS cases and final sensor-image update reliability need further verification.
