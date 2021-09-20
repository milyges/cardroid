#ifndef __PILOT_H
#define __PILOT_H

#include <stdint.h>

#define PILOT_KEY_SOURCE_L   (1 << 0)
#define PILOT_KEY_SOURCE_R   (1 << 1)
#define PILOT_KEY_LOAD       (1 << 2)
#define PILOT_KEY_VOLUP      (1 << 3)
#define PILOT_KEY_VOLDOWN    (1 << 4)
#define PILOT_KEY_PAUSE      (1 << 5)
#define PILOT_ROLL_NEXT      (1 << 6)
#define PILOT_ROLL_PREV      (1 << 7)

uint8_t pilot_getkey(void);
void pilot_init(void);

#endif /* __PILOT_H */
