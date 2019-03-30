#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include "stm32f10x_conf.h"
#include "cdcemu.h"
#include "usart.h"

enum ChangerEmulatorState {
    cdcStateBooting1 = 1,
    cdcStateBooting2,
    cdcStateStandby,
    cdcStatePlaying,
    cdcStatePause
};

enum ChangerEmulatorCDState {
    cdStateNoCD = 0x01,
    cdStatePaused = 0x03,
    cdStateLoadingTrack = 0x04,
    cdStatePlaying = 0x05,
    cdStateCueing = 0x07,
    cdStateRewinding = 0x09,
    cdStateSearchingTrack = 0x0A
};

enum ChangerEmulatorTrayState {
    trayStateNoTray = 0x02,
    trayStateCDReady = 0x03,
    trayStateLoadingCD = 0x04,
    trayStateUnloadingCD = 0x05
};

enum ChangerEmulatorTrackState {
    trackChangeEntering = 0x10,
    trackChangeEntered = 0x14,
    trackChangeLeavingCD = 0x40,
    trackChangeLeaving = 0x80
};

static uint8_t _frame_id = 0x00;
static int16_t _timeout;
static enum ChangerEmulatorState _changer_state;
static enum ChangerEmulatorCDState _cd_state;
static enum ChangerEmulatorTrayState _tray_state;
static int _current_cd;
static int _current_track;
static uint8_t _random_enabled;
static uint8_t _cd_bitmap;
static time_t _track_time;
static time_t _cd_time;

void USART1_IRQHandler(void) {
	uint8_t c;
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
		c = USART_ReceiveData(USART1);
		uprintf("+DEBUG: CDC Recv 0x%02X", c);
	}
}

static void _cdc_putc(uint8_t c) {
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) ;
	USART_SendData(USART1, (uint16_t)c);
}

static void _cdc_send_packet(uint8_t * data, uint8_t datalen) {
	uint8_t checksum = 0x00;
	uint8_t frame[64];
	int i;

	frame[0] = 0x3D; /* Nagłówek ramki */
	frame[1] = _frame_id++; /* ID Ramki */
	frame[2] = datalen; /* Długość danych */
	for (i = 0; i < datalen; i++) { /* Kopiujemy dane */
		frame[3 + i] = data[i];
	}

	uprintf("+DEBUG:CDC Send\n");
	/* Obliczamy sumę kontrolną i jednocześnie wysyłamy dane */
	for(i = 0; i < datalen + 3; i++) {
		checksum ^= frame[i];
		_cdc_putc(frame[i]);
	}

	_cdc_putc(checksum);
}


static void _cdc_reset(void) {
	uprintf("+DEBUG: CDC Resetting state\n");
	_frame_id = 0x00;
	_changer_state = cdcStateBooting1;
	_current_cd = 1;
	_current_track = 1;
	_cd_state = cdStateNoCD;
	_tray_state = trayStateCDReady;
	_random_enabled = 0;
	_cd_bitmap = 0xFC;
	_cd_time = 0;
	_track_time = 0;
}

static void _send_current_state(void) {
	switch(_changer_state) {
		case cdcStateBooting1: {
			_cdc_send_packet((uint8_t []){ 0x11, 0x60, 0x06 }, 3);
			break;
		}
	}
}

void cdcemu_tick(void) {
	static uint16_t _tick_cnt = 0;

	if (_tick_cnt++ < 100) {
		return;
	}
	_tick_cnt = 0;

	uprintf("+DEBUG: CDC Tick\n");
	if (_changer_state != cdcStateBooting1) {
		_timeout--;
		if (_timeout <= 0) {
			uprintf("+CDCCMD:STOP\n");
			_cdc_reset();
		}
	}

	_send_current_state();
}

/* Zmieniarka podłączona do PA9 (TX) i PA10 (RX) USART1 */
void cdcemu_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	USART_InitTypeDef usart_initstruct;

	/* Inicjujemy GPIO */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); /* Zegar */

	GPIO_StructInit(&gpio_initstruct); /* PA9 (USART TX): AF, PP, 50MHz */
	gpio_initstruct.GPIO_Pin = GPIO_Pin_9;
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);

	GPIO_StructInit(&gpio_initstruct); /* PA10 (USART RX): IN, PULL UP */
	gpio_initstruct.GPIO_Pin = GPIO_Pin_10;
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);

	/* Inicujemy USART1, 9600bps, 8bit, 1 bit stopu */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); /* Zegar */

	USART_StructInit(&usart_initstruct);
	usart_initstruct.USART_BaudRate = 9600;
	usart_initstruct.USART_WordLength = USART_WordLength_8b;
	usart_initstruct.USART_StopBits = USART_StopBits_1;
	usart_initstruct.USART_Parity = USART_Parity_No;
	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &usart_initstruct);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); /* Przerwanie od RX włączone */
	NVIC_EnableIRQ(USART1_IRQn);
	USART_Cmd(USART1, ENABLE);

	_cdc_reset();
}
