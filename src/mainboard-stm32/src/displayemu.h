#ifndef __DISPLAYEMU_H
#define __DISPLAYEMU_H

/* Ikonki na wyświetlaczu */
#define DISPLAY_ICON_NO_NEWS       (1 << 0)
#define DISPLAY_ICON_NEWS_ARROW    (1 << 1)
#define DISPLAY_ICON_NO_TRAFFIC    (1 << 2)
#define DISPLAY_ICON_TRAFFIC_ARROW (1 << 3)
#define DISPLAY_ICON_NO_AFRDS      (1 << 4)
#define DISPLAY_ICON_AFRDS_ARROW   (1 << 5)
#define DISPLAY_ICON_NO_MODE       (1 << 6)
#define DISPLAY_ICON_MODE_NONE     0xFF

/* Klawisze z pilota */
#define DISPLAY_KEY_LOAD           0x0000 /* Ten na dole pilota ;) */
#define DISPLAY_KEY_SRC_RIGHT      0x0001
#define DISPLAY_KEY_SRC_LEFT       0x0002
#define DISPLAY_KEY_VOLUME_UP      0x0003
#define DISPLAY_KEY_VOLUME_DOWN    0x0004
#define DISPLAY_KEY_PAUSE          0x0005
#define DISPLAY_KEY_ROLL_UP        0x0101
#define DISPLAY_KEY_ROLL_DOWN      0x0141
#define DISPLAY_KEY_HOLD_MASK (0x80 | 0x40)

void displayemu_tick(void);
void displayemu_refresh(void);
void displayemu_init(void);
void displayemu_stop(void);

#endif /* __DISPLAYEMU_H */
