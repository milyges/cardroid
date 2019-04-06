#include "stm32f10x_conf.h"
#include "pilot.h"

/*
 * Podłączenie pilota:
 * PB10 -> COL0
 * PB11 -> COL1
 * PB2  -> COL2
 * PA7  -> ROW0
 * PB1  -> ROW1
 * PB0  -> ROW2
*/
#define COL0_GPIO   GPIOB
#define COL0_PIN    GPIO_Pin_10
#define COL1_GPIO   GPIOB
#define COL1_PIN    GPIO_Pin_11
#define COL2_GPIO   GPIOB
#define COL2_PIN    GPIO_Pin_2
#define ROW0_GPIO   GPIOA
#define ROW0_PIN    GPIO_Pin_7
#define ROW1_GPIO   GPIOB
#define ROW1_PIN    GPIO_Pin_1
#define ROW2_GPIO   GPIOB
#define ROW2_PIN    GPIO_Pin_0

#define ROW_DOWN(x) GPIO_WriteBit(x ## _GPIO, x ## _PIN, Bit_RESET)
#define ROW_UP(x) GPIO_WriteBit(x ## _GPIO, x ## _PIN, Bit_SET)
#define COL_READ(x) (GPIO_ReadInputDataBit(x ## _GPIO, x ## _PIN) == Bit_SET)

static inline void _pilot_delay(void) {
	int i;
	for (i = 0; i < 4000; i++) __NOP();
}

uint8_t pilot_getkey(void) {
	int8_t roll_state = -1;
	static uint8_t old_roll_state = -1;
	uint8_t key = 0;

	ROW_DOWN(ROW0); /* Wiersz 0 - rolka */
	_pilot_delay(); /* Opóźnienie na ustabilizowanie się stanów logicznych */
	if (!COL_READ(COL0))
		roll_state = 0;
	if (!COL_READ(COL1))
		roll_state = 1;
	if (!COL_READ(COL2))
		roll_state = 2;
	ROW_UP(ROW0);

	ROW_DOWN(ROW1);
	_pilot_delay();
	if (!COL_READ(COL0))
		key |= PILOT_KEY_VOLUP;
	if (!COL_READ(COL1))
		key |= PILOT_KEY_VOLDOWN;
	if (!COL_READ(COL2))
		key |= PILOT_KEY_LOAD;
	ROW_UP(ROW1);

	ROW_DOWN(ROW2);
	_pilot_delay();
	if (!COL_READ(COL0))
		key |= PILOT_KEY_PAUSE;
	if (!COL_READ(COL1))
		key |= PILOT_KEY_SOURCE_L;
	if (!COL_READ(COL2))
		key |= PILOT_KEY_SOURCE_R;
	ROW_UP(ROW2);

	/* Obsługa rolki */
	if ((roll_state != old_roll_state) && (roll_state != -1)) {
		if (old_roll_state >= 0) {
			if (((old_roll_state + 1) % 3) == roll_state)
				key |= PILOT_ROLL_PREV;
			else
				key |= PILOT_ROLL_NEXT;
		}
		old_roll_state = roll_state;
	}
	return key;
}

void pilot_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE); /* Zegar */

	/* Kolumnty jako wejścia z podciągnięciem do VCC */
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	gpio_initstruct.GPIO_Pin = COL0_PIN;
	GPIO_Init(COL0_GPIO, &gpio_initstruct);
	gpio_initstruct.GPIO_Pin = COL1_PIN;
	GPIO_Init(COL1_GPIO, &gpio_initstruct);
	gpio_initstruct.GPIO_Pin = COL2_PIN;
	GPIO_Init(COL2_GPIO, &gpio_initstruct);

	/* Wiersze jako wyjścia Open Drain */
	gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_OD;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	gpio_initstruct.GPIO_Pin = ROW0_PIN;
	GPIO_Init(ROW0_GPIO, &gpio_initstruct);
	gpio_initstruct.GPIO_Pin = ROW1_PIN;
	GPIO_Init(ROW1_GPIO, &gpio_initstruct);
	gpio_initstruct.GPIO_Pin = ROW2_PIN;
	GPIO_Init(ROW2_GPIO, &gpio_initstruct);
	ROW_UP(ROW0);
	ROW_UP(ROW1);
	ROW_UP(ROW2);
}
