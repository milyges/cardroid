#include <stddef.h>
#include "stm32f10x.h"
#include "usart.h"
#include "power.h"
#include "displayemu.h"
#include "rtc.h"
#include "cdcemu.h"
#include "pilot.h"
#include <string.h>

#define SHUTDOWN_TIME    60 /* Czas do wysłania do odroida żądania zamknięcia systemu */
#define SHUTDOWN_DELAY   30 /* Czas od wysłania żądania do odcięcia zasilania */

static void _write_power_status(void) {
	uint8_t ignon, radioon;
	ignon = power_ign_on();
	radioon = power_radio_on();
	if (!ignon) { /* Zapłon + radio wyłączone */
		uprintf("+POWER:0,0\n");
	}
	else if (!radioon) { /* Zapłon włączony, radio wyłączone */
		uprintf("+POWER:1,0\n");
	}
	else { /* Radio + zapłon włączone */
		uprintf("+POWER:1,1\n");
	}
}

/* Przerwanie uruchamiane co 1ms */
void SysTick_Handler(void) {
	displayemu_tick();
	cdcemu_tick();
}

int main(void) {
	char cmd[64];
	uint8_t oldignon = 0, oldradioon = 0, oldkey = 0;
	uint8_t ignon, radioon, key;
	time_t shutdown_time;

	SysTick_Config(SystemCoreClock / 100);

	power_init(); /* Zarządzanie zasilaniem */
	usart_init(); /* Port szeregowy do komunikacji z ODROID */
	uprintf("\n+INFO:CarDroid starup\n");
	rtc_init(); /* Inicjalizacja RTC */

	/* Sprawdzamy czy włączone jest radio albo zapłon */
	if (!power_ign_on()) {
		/* Jeżeli nie to usypiamy */
		power_standby();
	}

	cdcemu_init(); /* Emulator zmieniarki */

	power_odroid_set(ENABLE); /* Włączamy zasilanie odroida */

	displayemu_init(); /* Emulator wyświetlacza */
	pilot_init();

	while(1) {
		if (usart_readcommand(cmd, sizeof(cmd))) { /* Nowe polecenie */
			if (!strcmp(cmd, "+DR\n")) { /* Wyślij dane z ekranu (radio text, ikony, itp) ponownie */
				displayemu_refresh();
			}
			else if (!strcmp(cmd, "+PS\n")) { /* Wyślij stan zasilania */
				_write_power_status();
			}
			else if (!strcmp(cmd, "+POFF\n")) { /* Wyłącz zasilanie odroida po ~30 sekundach */

			}
			else if (!strcmp(cmd, "+BOFF\n")) { /* Wyłącz podświetlenie ekranu */

			}
			else if (!strcmp(cmd, "+BON\n")) { /* Włącz podświetlenie ekranu */

			}
			else {
				uprintf("+ERROR: Unknown command %s\n", cmd);
			}
		}

		/* Sprawdzamy zmianę stanu zasilania */
		ignon = power_ign_on();
		radioon = power_radio_on();
		if ((ignon != oldignon) || (radioon != oldradioon)) {
			oldignon = ignon;
			oldradioon = radioon;
			_write_power_status();

			if ((!ignon) && (!radioon)) {
				power_display_set(DISABLE); /* Wyłączamy podświetlenie ekranu */
				shutdown_time = rtc_time() + SHUTDOWN_TIME;
			}
			else {
				power_display_set(ENABLE); /* Włączamy podświetlenie ekranu */
			}
		}

		if ((!oldignon) && (!oldradioon) && (rtc_time() >= shutdown_time)) {
			/* Radio/zapłon nie były włączone przez SHUTDOWN_TIME, wysyłamy do odroida żądanie zamknięcia systemu */
			uprintf("+SHUTDOWN\n");

			displayemu_stop(); /* Wyłączamy emulację wyświetlacza */

			/* Rozpoczynami odliczanie opóźnienia do odcięcia zasilania */
			shutdown_time = rtc_time() + SHUTDOWN_DELAY;
			while(rtc_time() < shutdown_time) {
				//__WFE();
			}

			/* Sprawdzamy czy wszystko nadal wyłączone i usypiamy */
			if (!power_ign_on()) {
				uprintf("+DEBUG: Power off.\n");
				power_odroid_set(DISABLE);
				power_standby();
			}
			else { /* Jeżeli między czasie wróciło zasilanie resetujemy procesor */
				NVIC_SystemReset();
			}
			while(1);
		}

		key = pilot_getkey();
		if (oldkey != key) {
			if ((key & PILOT_KEY_SOURCE_L) == PILOT_KEY_SOURCE_L) {
				//uprintf("+DEBUG: PILOT_KEY_SOURCE_L\n");
				displayemu_sendkey(DISPLAY_KEY_SRC_LEFT);
			}
			else if ((key & PILOT_KEY_SOURCE_R) == PILOT_KEY_SOURCE_R) {
				//uprintf("+DEBUG: PILOT_KEY_SOURCE_R\n");
				displayemu_sendkey(DISPLAY_KEY_SRC_RIGHT);
			}
			else if ((key & PILOT_KEY_LOAD) == PILOT_KEY_LOAD) {
				//uprintf("+DEBUG: PILOT_KEY_LOAD\n");
				displayemu_sendkey(DISPLAY_KEY_LOAD);
			}
			else if ((key & PILOT_KEY_VOLUP) == PILOT_KEY_VOLUP) {
				//uprintf("+DEBUG: PILOT_KEY_VOLUP\n");
				displayemu_sendkey(DISPLAY_KEY_VOLUME_UP);
			}
			else if ((key & PILOT_KEY_VOLDOWN) == PILOT_KEY_VOLDOWN) {
				//uprintf("+DEBUG: PILOT_KEY_VOLDOWN\n");
				displayemu_sendkey(DISPLAY_KEY_VOLUME_DOWN);
			}
			else if ((key & PILOT_KEY_PAUSE) == PILOT_KEY_PAUSE) {
				//uprintf("+DEBUG: PILOT_KEY_PAUSE\n");
				displayemu_sendkey(DISPLAY_KEY_PAUSE);
			}
			else if ((key & PILOT_ROLL_NEXT) == PILOT_ROLL_NEXT) {
				//uprintf("+DEBUG: PILOT_ROLL_NEXT\n");
				displayemu_sendkey(DISPLAY_KEY_ROLL_DOWN);
			}
			else if ((key & PILOT_ROLL_PREV) == PILOT_ROLL_PREV) {
				//uprintf("+DEBUG: PILOT_ROLL_NEXT\n");
				displayemu_sendkey(DISPLAY_KEY_ROLL_UP);
			}
			oldkey = key;
		}

		cdcemu_loop();
		displayemu_loop();
		//__WFE();
	}
}

/*
 * Minimal __assert_func used by the assert() macro
 * */
void __assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
  while(1)
  {}
}

/*
 * Minimal __assert() uses __assert__func()
 * */
void __assert(const char *file, int line, const char *failedexpr)
{
   __assert_func (file, line, NULL, failedexpr);
}
