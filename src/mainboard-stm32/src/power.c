#include "power.h"
#include "stm32f10x_conf.h"

#define POWER_GPIO            GPIOA
#define POWER_ODROID_PIN      GPIO_Pin_11 /* Wyjście -> sterowanie przetwornicą od odroida */
#define POWER_IGNON_PIN       GPIO_Pin_0  /* Wejście -> czy włączony jest zapłon */
#define POWER_RADIOON_PIN     GPIO_Pin_5  /* Wejście -> czy włączone jest radio */
#define POWER_DISPLAY_PIN     GPIO_Pin_1  /* Wyjście -> sterowanie zasilaniem ekranu */

#define DISPLAY_LEVEL2PWM(x)  ((x) * 4 - 1)
static uint8_t _display_brightness = 255;
static uint8_t _display_state = DISABLE;

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
		TIM_SetCompare2(TIM2, DISPLAY_LEVEL2PWM(_display_brightness));
	}
	else {
		TIM_SetCompare2(TIM2, 0);
	}
	_display_state = state;
}

void power_display_brightness(uint8_t level) {
	_display_brightness = level;
	if (_display_state == ENABLE) {
		TIM_SetCompare2(TIM2, DISPLAY_LEVEL2PWM(_display_brightness));
	}
}

void power_standby(void) {
	PWR_EnterSTANDBYMode();
	while(1);
}

void power_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	TIM_TimeBaseInitTypeDef tim_init;
	TIM_OCInitTypeDef oc_init;

	/* Konfigurujemy wyjścia */
	GPIO_StructInit(&gpio_initstruct);
	gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio_initstruct.GPIO_Pin = POWER_ODROID_PIN;;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(POWER_GPIO, &gpio_initstruct);

	GPIO_StructInit(&gpio_initstruct);
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Pin =  POWER_DISPLAY_PIN;
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

	/* Timer do PWMa (T2C2) */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseStructInit(&tim_init);
	tim_init.TIM_CounterMode = TIM_CounterMode_Up;
	tim_init.TIM_Prescaler = 48;
	tim_init.TIM_Period = 1000 - 1;
	TIM_TimeBaseInit(TIM2, &tim_init);

	TIM_OCStructInit(&oc_init);
	oc_init.TIM_OCMode = TIM_OCMode_PWM1;
	oc_init.TIM_OutputState = TIM_OutputState_Enable;
	oc_init.TIM_Pulse = 0;
	TIM_OC2Init(TIM2, &oc_init);

	TIM_Cmd(TIM2, ENABLE);

	/* Konfiguracja trybu uśpienia */
	RCC_APB1PeriphClockCmd(RCC_APB1ENR_PWREN, ENABLE);
	DBGMCU_Config(DBGMCU_STANDBY, ENABLE); /* Włączamy debugowanie podczas uśpienia */
	PWR_WakeUpPinCmd(ENABLE); /* Włączamy pin WakeUP (PA0) */
	PWR_BackupAccessCmd(ENABLE); /* Zezwolenie na dostep do "Backup domain" */
}
