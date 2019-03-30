#include "rtc.h"
#include "stm32f10x_conf.h"
#include "usart.h"

/* Przerwanie wykonywane co 1sek */
void RTC_IRQHandler(void) {
	if(RTC_GetITStatus(RTC_IT_SEC) != RESET) {


		RTC_ClearITPendingBit(RTC_IT_SEC);
		RTC_WaitForLastTask();
	}
}

time_t rtc_time(void) {
	return RTC_GetCounter();
}

void rtc_settime(time_t time) {
	RTC_WaitForLastTask();
	RTC_SetCounter(time);
	RTC_WaitForLastTask();
}

void rtc_setalarm(time_t time) {
	RTC_WaitForLastTask();
	RTC_SetAlarm(time);
	RTC_WaitForLastTask();
}

void rtc_init(void) {
	NVIC_InitTypeDef nvic_initstruct;

	RCC_APB1PeriphClockCmd (RCC_APB1Periph_BKP, ENABLE);

	PWR_BackupAccessCmd(ENABLE); /* włączenie dostępu do rejestrów chronionych */
	BKP_DeInit();
	RTC_EnterConfigMode();
	RCC_LSICmd(ENABLE); /* Włączenie oscylatora LSI i poczekanie aż będzie gotowy */
	while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI); /* LSI bedzie zrodlem sygnalu zegarowego dla RTC */
	RCC_RTCCLKCmd(ENABLE); /* Wlacz taktowanie RTC */
	RTC_WaitForSynchro(); /* Czekaj na synchronizacje z APB1 */
	RTC_WaitForLastTask();

	/* 40000Hz/40000 = 1Hz */
	RTC_SetPrescaler(39999);
	/* Czekaj na wykonanie polecenia */
	RTC_WaitForLastTask();
	RTC_ExitConfigMode();

	/* Konfiguracja przerwań od RTC, przerwanie co 1 sek... */
	nvic_initstruct.NVIC_IRQChannel = RTC_IRQn;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_initstruct);
	RTC_ITConfig(RTC_IT_SEC, ENABLE);
	RTC_WaitForLastTask();

	/* ..., przerwanie alarmu */
	/*nvic_initstruct.NVIC_IRQChannel = RTCAlarm_IRQn;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_initstruct);
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
	RTC_WaitForLastTask();*/
}
