#include "power.h"
#include "stm32f10x_conf.h"

#define POWER_GPIO            GPIOA
#define POWER_ODROID_PIN      GPIO_Pin_11 /* Wyjście -> sterowanie przetwornicą od odroida */
#define POWER_IGNON_PIN       GPIO_Pin_0  /* Wejście -> czy włączony jest zapłon */
#define POWER_RADIOON_PIN     GPIO_Pin_5  /* Wejście -> czy włączone jest radio */
#define POWER_DISPLAY_PIN     GPIO_Pin_1  /* Wyjście -> sterowanie zasilaniem ekranu */

void power_odroid_set(uint8_t state) {
	if (state == ENABLE) {
		GPIO_WriteBit(POWER_GPIO, POWER_ODROID_PIN, Bit_SET);
	}
	else {
		GPIO_WriteBit(POWER_GPIO, POWER_ODROID_PIN, Bit_RESET);
	}
}

uint8_t power_odroid_get(void) {
	return GPIO_ReadOutputDataBit(POWER_GPIO, POWER_ODROID_PIN) == Bit_SET;
}

uint8_t power_radio_on(void) {
	return GPIO_ReadInputDataBit(POWER_GPIO, POWER_RADIOON_PIN) == Bit_RESET;
}
uint8_t power_ign_on(void) {
	return GPIO_ReadInputDataBit(POWER_GPIO, POWER_IGNON_PIN) == Bit_SET;
}

void power_display_set(uint8_t state) {
	if (state == ENABLE) {
		GPIO_WriteBit(POWER_GPIO, POWER_DISPLAY_PIN, Bit_SET);
	}
	else {
		GPIO_WriteBit(POWER_GPIO, POWER_DISPLAY_PIN, Bit_RESET);
	}
}

void power_display_brightness(uint8_t level);

void power_standby(void) {
	PWR_EnterSTANDBYMode();
	while(1);
}

void power_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	/* Konfigurujemy wyjścia */
	GPIO_StructInit(&gpio_initstruct);
	gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio_initstruct.GPIO_Pin = POWER_ODROID_PIN | POWER_DISPLAY_PIN;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(POWER_GPIO, &gpio_initstruct);

	/* Konfigurujemy wejścia */
	GPIO_StructInit(&gpio_initstruct);
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;
	gpio_initstruct.GPIO_Pin = POWER_RADIOON_PIN;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(POWER_GPIO, &gpio_initstruct);

	GPIO_StructInit(&gpio_initstruct);
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio_initstruct.GPIO_Pin = POWER_IGNON_PIN;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(POWER_GPIO, &gpio_initstruct);

	/* Konfiguracja trybu uśpienia */
	RCC_APB1PeriphClockCmd(RCC_APB1ENR_PWREN, ENABLE);
	DBGMCU_Config(DBGMCU_STANDBY, ENABLE); /* Włączamy debugowanie podczas uśpienia */
	PWR_WakeUpPinCmd(ENABLE); /* Włączamy pin WakeUP (PA0) */
	PWR_BackupAccessCmd(ENABLE); /* Zezwolenie na dostep do "Backup domain" */
}
