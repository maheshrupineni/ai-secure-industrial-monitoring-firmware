#ifndef INDUSTRIAL_FIRMWARE_UPDATE_H
#define INDUSTRIAL_FIRMWARE_UPDATE_H

#include <stdint.h>

/* Call once before creating monitoring tasks. */
void firmware_update_boot_check(void);

/* Call only from the communication task for a healthy matched result. */
void firmware_update_note_healthy(uint32_t sequence);

/* Call once after monitoring tasks have been created. */
void firmware_update_start_demo(void);

#endif
