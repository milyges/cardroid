#ifndef __POWER_H_
#define __POWER_H_

#include <stdint.h>

void power_odroid_set(uint8_t state);
uint8_t power_odroid_get(void);

uint8_t power_radio_on(void);
uint8_t power_ign_on(void);

void power_display_set(uint8_t state);
void power_display_brightness(uint8_t level);

void power_standby(void);
void power_init(void);

#endif /* __POWER_H_ */
