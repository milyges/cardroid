#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "stm32f10x_conf.h"

#define CMD_QUEUE_SIZE 3
static char _cmd_queue[CMD_QUEUE_SIZE][64];
static int _cmd_queue_in, _cmd_queue_out;

static inline char _cmd_queue_is_full(void) {
	return (_cmd_queue_in + 1) % CMD_QUEUE_SIZE == _cmd_queue_out;
}

static inline char _cmd_queue_is_empty(void) {
	return _cmd_queue_in == _cmd_queue_out;
}

static inline void _cmd_queue_push(char * s) {
	strcpy(_cmd_queue[_cmd_queue_in], s);
	_cmd_queue_in = (_cmd_queue_in + 1) % CMD_QUEUE_SIZE;
}

static inline char * _cmd_queue_get(char * dst, int dstsz) {
	strncpy(dst, _cmd_queue[_cmd_queue_out], dstsz);
	_cmd_queue_out = (_cmd_queue_out + 1) % CMD_QUEUE_SIZE;
	return dst;
}

/* Odroid podpięty jest do PA2 (USART 2 TX) i PA3(USART 2 RX) */
static void _usart_putc(char c) {
	while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) ;
	USART_SendData(USART2, (uint16_t)c);
}

void USART2_IRQHandler(void) {
	static char buf[64] = { 0, };
	static int bufpos = 0;

	uint8_t c;
	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) { /* Są dane do odczytania */
		c = USART_ReceiveData(USART2);
		if (c == '\r')
			return;

		if (c == '+') { /* Początek polecenia */
			bufpos = 0;
			memset(buf, 0x00, sizeof(buf));
		}

		if (bufpos < sizeof(buf) - 1)
			buf[bufpos++] = c;

		if (c == '\n') { /* Koniec polecenia */
			buf[bufpos++] = '\0';
			if (!_cmd_queue_is_full()) {
				_cmd_queue_push(buf);
			}
		}
	}
}

void uprintf(const char * fmt, ...) {
	char buf[128+1];
	char * s;
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, va);
	va_end(va);


	s = buf;
	while(*s) {
		_usart_putc(*s++);
	}
}

char * usart_readcommand(char * buf, int bufsz) {
	if (_cmd_queue_is_empty()) {
		return NULL;
	}

	return _cmd_queue_get(buf, bufsz);
}

void usart_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	USART_InitTypeDef usart_initstruct;

	/* Inicjujemy GPIO */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); /* Zegar */

	GPIO_StructInit(&gpio_initstruct); /* PA2 (USART TX): AF, PP, 50MHz */
	gpio_initstruct.GPIO_Pin = GPIO_Pin_2;
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);

	GPIO_StructInit(&gpio_initstruct); /* PA3 (USART RX): IN, PULL UP */
	gpio_initstruct.GPIO_Pin = GPIO_Pin_3;
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);

	/* Inicujemy USART2, 19200bps, 8bit, 1 bit stopu */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); /* Zegar */

	USART_StructInit(&usart_initstruct);
	usart_initstruct.USART_BaudRate = 19200;
	usart_initstruct.USART_WordLength = USART_WordLength_8b;
	usart_initstruct.USART_StopBits = USART_StopBits_1;
	usart_initstruct.USART_Parity = USART_Parity_No;
	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &usart_initstruct);
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); /* Przerwanie od RX włączone */
	NVIC_EnableIRQ(USART2_IRQn);
	USART_Cmd(USART2, ENABLE);
}
