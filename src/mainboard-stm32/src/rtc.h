#ifndef __RTC_H
#define __RTC_H

#include <time.h>

void rtc_init(void);
time_t rtc_time(void);
void rtc_settime(time_t time);

#endif /* __RTC_H */
