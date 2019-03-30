#include <stdint.h>
#include "usart.h"
#include "stm32f10x_conf.h"
#include <string.h>

#define CAN_ID_SYNC_DISPLAY        0x3CF  /* Pakiet synchronizacyjny, wyświetlacz -> HU */
#define CAN_ID_SYNC_HU             0x3DF  /* Pakiet synchronizacyjny, HU -> wyświetlacz */
#define CAN_ID_DISPLAY_CONTROL     0x1B1  /* Konfiguracja wyświetlacza, HU -> wyświetlacz */
#define CAN_ID_DISPLAY_STATUS      0x1C1  /* Zmiana stanu wyświetlacza, wyświetlacz -> HU */
#define CAN_ID_SET_TEXT            0x121  /* Ustawienie tekstu na wyświetlaczu, HU -> wyświetlacz */
#define CAN_ID_KEY_PRESSED         0x0A9  /* Informacja o naciśnięciu klawisza, wyświetlacz -> HU */

#define CAN_ID_REPLY_FLAG          0x400  /* Flaga ustawiana dla odpowiedzi */

#define CAN_TIMEOUT                200    /* 2sek */

/* Bajty wypełniające */
#define DISPLAY_FILL_BYTE          0xA3
#define HU_FILL_BYTE               0x81

static enum DisplaySyncState {
    displaySyncOK = 0x01,   /* Synchronizacja OK */
    displaySyncPending1,    /* Synchronizacja w trakcie negocjacji, etap 1 */
	displaySyncPending2,    /* Synchronizacja w trakcie negocjacji, etap 2 */
    displaySyncLost         /* Synchronizacja utracona */
} _sync_state = displaySyncLost;

static uint8_t _display_enabled = 0;
static uint8_t _menu_visible = 0;
static char _radioText[32];
static uint8_t _radioIcons;
static int16_t _timeout = CAN_TIMEOUT;

/* Funkcja wysyła stan wyświetlacza do odroida */
static inline void _display_write_status(void) {
	uprintf("+RADIO:%s\n", _display_enabled ? "ON" : "OFF");
}

static inline void _display_write_radiotext(void) {
	uprintf("+RADIOTEXT:%s\n", _radioText);
}

static inline void _display_write_icons(void) {
	uprintf("+RADIOICONS:%u\n", _radioIcons);
}

void _display_can_send(uint16_t id, uint8_t data[8]) {
	CanTxMsg msg;
	int i;

	msg.StdId = id;
	msg.ExtId = 0;
	msg.RTR = CAN_RTR_DATA;
	msg.IDE = CAN_ID_STD;
	msg.DLC = 8;

	for(i = 0; i < 8; i++) {
		msg.Data[i] = data[i];
	}

	//uprintf("_can_send: id=0x%X, data={ %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x }\n", msg.StdId, msg.Data[0], msg.Data[1],
	//					msg.Data[2], msg.Data[3], msg.Data[4], msg.Data[5], msg.Data[6], msg.Data[7]);


	uint8_t box = CAN_Transmit(CAN1, &msg);
	if (box == CAN_TxStatus_NoMailBox) {
		uprintf("no box\n");
	}

	/*uint8_t status;
	uint8_t retry = 10;
	uint8_t box;
	while(--retry > 0) {
		uint8_t box = CAN_Transmit(CAN1, &msg);
		while ((status = CAN_TransmitStatus(CAN1, box)) == CAN_TxStatus_Pending) ;
		if (status == CAN_TxStatus_Ok)
			break;
	}*/
}

void _display_register_id(uint16_t id) {
	_display_can_send(id, (uint8_t []){ 0x70, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE,
					DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
}

/* Funkcja wysyła potwierdzenie do radia:
 * msg - wiadomość którą potwierdzamy
 * last - czy spodziewamy się kolejnych części, czy ta była ostatnia
*/
void _display_send_reply(CanRxMsg * msg, uint8_t last) {
	if (last) {
		_display_can_send(msg->StdId | CAN_ID_REPLY_FLAG, (uint8_t []){ 0x74, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE,
													DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE,
													DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
	}
	else {
		_display_can_send(msg->StdId | CAN_ID_REPLY_FLAG, (uint8_t []){ 0x30, 0x00, 0x01,
															DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE,
															DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
	}
}

static void _display_recv_sync(CanRxMsg * msg) {
	if (msg->Data[0] == 0x79) { /* PING od Radia, odsyłamy PONG ;) */
		_display_can_send(CAN_ID_SYNC_DISPLAY, (uint8_t []){ 0x69, 0x00, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE,
										DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
		_timeout = CAN_TIMEOUT;
		if (_sync_state == displaySyncPending1) {
			_sync_state = displaySyncPending2;
			_display_register_id(CAN_ID_DISPLAY_STATUS);

		}
		else if(_sync_state == displaySyncPending2) {
			_sync_state = displaySyncOK;
			_display_register_id(CAN_ID_KEY_PRESSED);
		}
	}
	else if (msg->Data[0] == 0x7A) { /* Początek synchronizacji */
		_display_can_send(CAN_ID_SYNC_DISPLAY, (uint8_t []){ 0x61, 0x11, 0x00, DISPLAY_FILL_BYTE,
										DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
		_sync_state = displaySyncPending1;
	}
}

static void _display_recv_settext(CanRxMsg * msg) {
	static char buf[32];
	char text[16] = { '\0', };
	static int bufpos;
	static int iconsmask = 0xFF, mode = 0xFF, chan = 0xFF, location;
	static char longtext[128];
	char c;
	int ptr, textidx = 0;
	uint8_t max, idx, selected, fullscreen;


	if (msg->Data[0] == 0x70) { /* HU daje znać że będzie używać tej funkcji */
		_display_send_reply(msg, 1);
	}
	else if (msg->Data[0] == 0x04) { /* Ustaw ikony */
		/* TODO */
		_display_send_reply(msg, 1);
	}
	else if (msg->Data[0] == 0x10) { /* Ustaw tekst */
		bufpos = 0;
		buf[0] = '\0';

		if (msg->Data[1] == 0x1C) { /* Tekst + ikony */
			iconsmask = msg->Data[3];
			mode = msg->Data[5];
			ptr = 6;
		}
		else if (msg->Data[1] == 0x19) { /* Tylko tekst */
			ptr = 3;
		}

		chan = msg->Data[ptr++] & 7;
		location = msg->Data[ptr++];
		for ( ; ptr < 8; ptr++) {
			buf[bufpos++] = msg->Data[ptr];
		}

		_display_send_reply(msg, 0);
	}
	else if (msg->Data[0] > 0x20) { /* Ciąg dalszy poprzednich danych */
		ptr = 1;
		while(ptr < 8) {
			buf[bufpos] = msg->Data[ptr++];
			if (!buf[bufpos])
				break;

			bufpos++;
		}

		if (ptr < 8) { /* Koniec danych */
			_display_send_reply(msg, 1);

			/* Obramiamy dane */
			for (ptr = 0; ptr < 12; ptr++) {
				c = (char)(buf[9 + ptr] & 0x7F);
				switch (c) {
					case 0x07: text[textidx++] = '^'; break;
				    case 0x08: text[textidx++] = 'v'; break;
				    case 0x09: text[textidx++] = '>'; break;
				    case 0x0A: text[textidx++] = '<'; break;
					default: text[textidx++] = c;
				}

			}
			text[textidx] = '\0';

			max = (location >> 5) & 7;
			idx = (location >> 2) & 7;
			selected = ((location & 0x01) == 0x01);
			fullscreen = ((location & 0x02) == 0x02);

			if (iconsmask != 0xFF) {
				_radioIcons = iconsmask;
				iconsmask = 0xFF;
				_display_write_icons();
			}

			if ((max > 0) && (fullscreen) && (!selected)) { /* Tekst migacjący (np. NEWS -> RMF FM -> NEWS...) */

			}

			if ((max > 0) && (!fullscreen)) { /* Tryb menu */
				if (!_menu_visible) {
					_menu_visible = 1;
					uprintf("+MENU:ON,%d\n", max + 1);
				}

				uprintf("+MENU:ITEM,%d,%s,%d\n", idx, text, selected);
			}
			else { /* Menu niewidoczne */
				if (_menu_visible) {
					_menu_visible = 0;
					uprintf("+MENU:OFF\n");
				}
			}

			if ((max == 0) && (selected)) {
				strcpy(_radioText, text);
				_display_write_radiotext();
			}
			else if ((fullscreen) && (selected)) { /* Tryb pełnoekranowy */
				if (idx == 0) {
					memset(longtext, 0x00, sizeof(longtext));
				}

				strncat(longtext, text, 8);

				if (idx == max) {
					strcpy(_radioText, longtext);
					_display_write_radiotext();
				}
			}
		}
		else {
			_display_send_reply(msg, 0);
		}
	}
}

void _display_recv_displayctrl(CanRxMsg * msg) {
	if (msg->Data[0] != 0x70) { /* Status wyświetlacza */
		_display_enabled = (msg->Data[2] == 0x02);
	}
	_display_send_reply(msg, 1);
	_display_write_status();
	/* Wysyłamy do HU potwierdzenie wykonania akcji */
	_display_can_send(CAN_ID_DISPLAY_STATUS, (uint8_t []){ 0x02, 0x64, 0x0F, DISPLAY_FILL_BYTE,
				      DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE, DISPLAY_FILL_BYTE });
}

void USB_LP_CAN1_RX0_IRQHandler(void) {
	CanRxMsg msg;
	if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) == SET) {
		CAN_Receive(CAN1, 0, &msg);

		//uprintf("_can_recv: id=0x%X, data={ %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x }\n", msg.StdId, msg.Data[0], msg.Data[1],
		//						msg.Data[2], msg.Data[3], msg.Data[4], msg.Data[5], msg.Data[6], msg.Data[7]);

		switch(msg.StdId) {
			case CAN_ID_SYNC_HU: _display_recv_sync(&msg); break;
			case CAN_ID_SET_TEXT: _display_recv_settext(&msg); break;
			case CAN_ID_DISPLAY_CONTROL: _display_recv_displayctrl(&msg); break;
		}
	}
}

void displayemu_tick(void) {
	if (--_timeout == 0) { /* Zmniejszamy timeout, jeżeli osiągnie 0 to znaczy że straciliśmy synchronizację z CAN */
		_sync_state = displaySyncLost;
		_timeout = -1;
	}
}

/* Wysyłamy jeszcze raz wszystkie dane do odroida (potrzebne po bootowaniu, po zwisie aplikacji, itp) */
void displayemu_refresh(void) {
	_display_write_status();
	_display_write_radiotext();
	_display_write_icons();
}

void displayemu_init(void) {
	GPIO_InitTypeDef gpio_initstruct;
	CAN_InitTypeDef can_initstruct;
	CAN_FilterInitTypeDef can_filterinitstruct;
	NVIC_InitTypeDef nvic_initstruct;

	/* Zegary */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

	/* GPIO dla CAN: włączamy i remapujemy na PB 8/9 */
	GPIO_StructInit(&gpio_initstruct); /* TX -> OUT, AF, PP */
	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_9;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio_initstruct);

	GPIO_StructInit(&gpio_initstruct); /* RX -> INPUT, PULL UP */
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;
	gpio_initstruct.GPIO_Pin = GPIO_Pin_8;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio_initstruct);

	GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);

	/* CAN 1Mbps */
	CAN_DeInit(CAN1);
	CAN_StructInit(&can_initstruct);
	can_initstruct.CAN_TTCM = DISABLE;
	can_initstruct.CAN_ABOM = DISABLE;
	can_initstruct.CAN_AWUM = DISABLE;
	can_initstruct.CAN_NART = ENABLE;
	can_initstruct.CAN_RFLM = ENABLE;
	can_initstruct.CAN_TXFP = DISABLE;
	can_initstruct.CAN_Mode = CAN_Mode_Normal;
	can_initstruct.CAN_SJW = CAN_SJW_1tq;
	can_initstruct.CAN_BS1 = CAN_BS1_11tq;
	can_initstruct.CAN_BS2 = CAN_BS2_4tq;
	can_initstruct.CAN_Prescaler = 3;
	CAN_Init(CAN1, &can_initstruct);
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

	/* Filtrowanie */
	can_filterinitstruct.CAN_FilterNumber = 0;
	can_filterinitstruct.CAN_FilterMode = CAN_FilterMode_IdMask;
	can_filterinitstruct.CAN_FilterScale = CAN_FilterScale_32bit;
	can_filterinitstruct.CAN_FilterIdHigh = 0;
	can_filterinitstruct.CAN_FilterIdLow = 0;
	can_filterinitstruct.CAN_FilterMaskIdHigh = 0;
	can_filterinitstruct.CAN_FilterMaskIdLow = 0;
	can_filterinitstruct.CAN_FilterFIFOAssignment = 0;
	can_filterinitstruct.CAN_FilterActivation = ENABLE;
	CAN_FilterInit(&can_filterinitstruct);

	/* Przerwania */
	nvic_initstruct.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&nvic_initstruct);

	strcpy(_radioText, "Uruchamianie...");
	_radioIcons = 0xFF;
}

/* Wyłącza emulację wyświetlacza (wyłączając CAN) */
void displayemu_stop(void) {
	NVIC_InitTypeDef nvic_initstruct;
	nvic_initstruct.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	nvic_initstruct.NVIC_IRQChannelCmd = DISABLE;
	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&nvic_initstruct);
	CAN_DeInit(CAN1);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, DISABLE);
}
