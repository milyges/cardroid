#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include "stm32f10x_conf.h"
#include "cdcemu.h"
#include "usart.h"
#include "rtc.h"

#define RECV_TIMEOUT       10 /* Czas w * 10ms na potwierdzenie od HU (100ms) */
#define QUEUE_LEN          32 /* Długośc kolejki odbieranych danych */
#define PING_TIMEOUT      300 /* Czas, po którym uznajemy że coś się zawiesiło i resetujemy emulator */

static uint8_t _frame_id;
static enum ChangerEmulatorCDState _cd_state;
static enum ChangerEmulatorState _changer_state;
static enum ChangerEmulatorTrayState _tray_state;
static uint8_t _current_cd;
static uint8_t _current_track;
static uint8_t _random_enabled;
static uint8_t _cd_bitmap;
static volatile int _recv_timeout; /* Czas na pakiet od HU w * 10ms */
static volatile int _ping_timeout;
static volatile uint8_t _ack_recvied = 0;
static time_t _last_packet_time;


/* Konwersja liczby na BCD */
static inline uint8_t _bcd(uint8_t num) {
    return ((num / 10) << 4) | (num % 10);
}

static uint8_t _checksum(uint8_t * packet, uint8_t packetlen) {
	uint8_t checksum = 0;
	int i;
	for(i = 0; i < packetlen; i++) {
		checksum ^= packet[i];
	}
	return checksum;
}

/* Kolejka przychodzących danych z HU */
static volatile struct {
	uint8_t in_ptr;
	uint8_t out_ptr;
	uint8_t data[QUEUE_LEN];
} _recv_queue;

static inline uint8_t _queue_empty(void) {
	return _recv_queue.in_ptr == _recv_queue.out_ptr;
}

static inline uint8_t _queue_full(void) {
	return ((_recv_queue.in_ptr + 1) % QUEUE_LEN) == _recv_queue.out_ptr;
}

static inline void _queue_put(uint8_t data) {
	if (!_queue_full()) {
		_recv_queue.data[_recv_queue.in_ptr] = data;
		_recv_queue.in_ptr = (_recv_queue.in_ptr + 1) % QUEUE_LEN;
	}
}

static inline int _queue_get(void) {
	uint8_t data;
	if (_queue_empty())
		return -1;
	data = _recv_queue.data[_recv_queue.out_ptr];
	_recv_queue.out_ptr = (_recv_queue.out_ptr + 1) % QUEUE_LEN;
	return data;
}

/* Reset stanu emulatora */
static void _cdc_reset(void) {
	//uprintf("+DEBUG:CDC Reset\n");
	_frame_id = 0x00;
	_changer_state = cdcStateBooting1;
	_current_cd = 1;
	_current_track = 1;
	_cd_state = cdStateNoCD;
	_tray_state = trayStateCDReady;
	_random_enabled = 0;
	_cd_bitmap = 0xFC;
	_recv_timeout = -1;
}

/* Wysyłanie danych do HU */
static void _cdc_putc(uint8_t c) {
	//uprintf("+DEBUG: CDC Send 0x%X\n", c);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) ;
	USART_SendData(USART1, (uint16_t)c);
}

static void _cdc_packet_send(uint8_t * data, uint8_t datalen) {
	uint8_t frame[32];
	int i, retries = 1;

	frame[0] = 0x3D; /* Nagłówek ramki */
	frame[1] = _frame_id++; /* ID Ramki */
	frame[2] = datalen; /* Długość danych */

	for(i = 0; i < datalen; i++) {
		frame[3 + i] = data[i]; /* Kopiujemy dane do wysłania */
	}

	/* Obliczamy sumę kontrolną i wysyłamy dane */
	frame[datalen + 3] = _checksum(frame, datalen + 3);

	while(retries-- > 0) {
		for(i = 0; i < datalen + 4;i++ )
			_cdc_putc(frame[i]);

		/* Czekamy na potwierdzenie od HU */
		_ack_recvied = 0;
		_recv_timeout = RECV_TIMEOUT;
		while(_recv_timeout > 0) __NOP();
		if (_ack_recvied) {
			break;
		}
		else {
			uprintf("+DEBUG: _cdc_packet_send: timeout (no ack), retries=%d\n", retries);
			_cdc_reset();
		}
	}
	//return _ack_received;
}

/* Wysyłanie odpowiednich typów komunikatów */
static void _cdc_packet_status(void) {
	_cdc_packet_send((uint8_t []){ 0x20, _cd_state, _tray_state, 0x09, 0x05, _current_cd }, 6);
}

static void _cdc_packet_cd_summary(void) {
	/* 99 ścieżkowa płyta o długości 60:00:00 */
	_cdc_packet_send((uint8_t []){
		0x46, /* ID 0x46 = CD_SUMMARY */
		0x99, /* Tracks count */
		0x01,
		0x00,
		0x60,
		0x00,
		0x00 } , 7);
}

static void _cdc_packet_tray_status(void) {
	_cdc_packet_send((uint8_t []){ 0x26, 0x05, _current_cd, _cd_bitmap, _cd_bitmap } , 5);
}

static void _cdc_packet_random_status(void) {
	if (_random_enabled) {
		_cdc_packet_send((uint8_t []){ 0x25, 0x07 }, 2);
	}
	else {
		_cdc_packet_send((uint8_t []){ 0x25, 0x03 }, 2);
	}
}

static void _cdc_packet_cd_operation(void) {
	_cdc_packet_send((uint8_t []){ 0x21, _cd_state }, 2);
}

static void _cdc_packet_track_change(enum ChangerEmulatorTrackState type, int num) {
	int tmp = type;

	if ((type == trackChangeEntering) && (num == 1)) {
		tmp |= 0x05;
	}

	_cdc_packet_send((uint8_t []){
		0x27, /* TRACK_CHANGE */
		tmp,
		_bcd(num),
		0x22 }, 4);
}

static void _cdc_packet_playing_status(void) {
	//uprintf("+DEBUG:_cdc_packet_playing_status()\n");
	_cdc_packet_send((uint8_t []){
		0x47, /* PLAYING */
		_bcd(_current_track),
		0x01,
		0x00,
		0x00,
		0x01,
		0x00,
		0x00,
		0x00,
		0x01,
		0x00 }, 11);
}


/* Wysłanie odpowiedniego pakietu, w zależności od stanu emulatora */
static void _cdc_send_state(void) {
	switch (_changer_state) {
		case cdcStateBooting1: {
			_cdc_packet_send((uint8_t []){ 0x11, 0x60, 0x06 }, 3);
			break;
		}
		case cdcStateBooting2: {
			_cdc_packet_send((uint8_t []){ 0x15, 0x00, 0x25 }, 3);
			break;
		}
		case cdcStatePause:
		case cdcStateStandby: {
			_cdc_packet_status();
			break;
		}
		case cdcStatePlaying: {
			_cdc_packet_playing_status();
			_cdc_packet_status();
			break;
		}
	}
}

/* Przerwanie od portu szeregowego zmieniarki, wrzuca dane do kolejki */
void USART1_IRQHandler(void) {
	uint8_t c;

	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
		c = USART_ReceiveData(USART1);
		_queue_put(c);
		if (c == 0xC5) {
			_ack_recvied = 1;
			_recv_timeout = -1;
		}
		_ping_timeout = PING_TIMEOUT;
	}
}

/* Tick wykonywany jest co ~10ms, służy do timeoutów */
void cdcemu_tick(void) {
	if (_recv_timeout >= 0) {
		_recv_timeout--;
	}

	if (_changer_state != cdcStateBooting1) {
		if (--_ping_timeout <= 0) {
			uprintf("+DEBUG:CDC Ping timeout\n");
			_cdc_reset();
		}
	}
}

/* Funkcja czeka RECV_TIMEOUT aż pojawią się jakieś dane w kolejce */
static uint8_t _wait_for_data(void) {
	_recv_timeout = RECV_TIMEOUT;
	while((_recv_timeout > 0) && (_queue_empty())) __NOP();
	return !_queue_empty();
}

/* Funkcja przetwarzająca odebrane pakiety od HU */
static void _cdc_packet_recv(uint8_t * data, uint8_t datalen) {
	switch(data[0]) {
		case 0x13: { /* PLAY */
			_cd_state = cdStateLoadingTrack;
			_cdc_packet_cd_operation();
			_cdc_send_state();
			_changer_state = cdcStatePlaying;
			uprintf("+CDC:PLAY\n");
			break;
		}
		case 0x17: { /* NEXT */
			uprintf("+CDC:NEXT\n");
			break;
		}
		case 0x19: { /* STOP */
			_cd_state = cdStateLoadingTrack;
			_cdc_packet_cd_operation();
			_cdc_packet_status();

			_cd_state = cdStateNoCD;
			_cdc_packet_cd_operation();
			_cdc_packet_status();

			_tray_state = trayStateCDReady;
			_cdc_packet_tray_status();
			_cdc_packet_status();

			_changer_state = cdcStateStandby;

			uprintf("+CDC:STOP\n");
			break;
		}
		case 0x1C: { /* PAUSE */
			_changer_state = cdcStatePause;
			_cdc_packet_cd_operation();
			_cdc_packet_status();

			uprintf("+CDC:PAUSE\n");
			break;
		}
		case 0x22: { /* PREV */
			uprintf("+CDC:PREV\n");
			break;
		}
		case 0x26: { /* LOAD_CD */
			uprintf("+CDC:LOAD,%d\n", data[1]);
			break;
		}
		case 0x86: { /* CD_SUMMARY */
			_cdc_packet_cd_summary();
			break;
		}
		default: {
			uprintf("+DEBUG:CDC Unknown command 0x%02X\n", data[0]);
		}
	}
}

/* Funkcja wywoływana z funkcji main(),
 * główny kod emulatora: odbiera dane, buduje z nich pakiety,
 * wysyła potwierdzenia oraz wywołuje pozostałe funkcje */
void cdcemu_loop(void) {
	uint8_t c, checksum;
	uint8_t buf[32];
	int i;
	static time_t last_time = 0;
	static uint8_t last_frame_id = 0xFF;

	if (!_queue_empty()) { /* Są jakieś dane w kolejce */
		_last_packet_time = rtc_time();
		c = _queue_get();
		//uprintf("+DEBUG:cdcemu_loop 0x%02X\n", c);
		if (c == 0xC5) { /* Potwierdzenie odebrania od HU */
			if (_changer_state == cdcStateBooting1) {
				_changer_state = cdcStateBooting2;
			}
			else if (_changer_state == cdcStateBooting2) {
				_changer_state = cdcStateStandby;
				_cdc_packet_status();
				_cdc_packet_random_status();
				_cdc_packet_tray_status();
				_cdc_packet_random_status();
			}
		}
		else if (c == 0x3D) { /* Początek polecenia */
			i = 0;
			buf[0] = c; /* Zapisujemy ID ramki */

			if (!_wait_for_data()) {
				uprintf("+DEBUG: CDC can't get frame id, timeout.\n");
				//_cdc_reset();
				return;
			}
			buf[1] = _queue_get(); /* Frame ID */

			if (!_wait_for_data()) {
				uprintf("+DEBUG: CDC can't get frame length, timeout.\n");
				//_cdc_reset();
				return;
			}
			buf[2] = _queue_get(); /* Data len */

			/* Odbieramy dane */
			for(i = 0; i < buf[2]; i++) {
				if (!_wait_for_data()) {
					uprintf("+DEBUG: CDC can't get frame data[%d], timeout.\n", i);
					//_cdc_reset();
					return;
				}
				buf[3 + i] = _queue_get();
			}

			/* Suma kontrolna */
			if (!_wait_for_data()) {
				uprintf("+DEBUG: CDC can't get frame checksum, timeout.\n");
				//_cdc_reset();
				return;
			}
			checksum = _queue_get();

			/* Sprawdzamy sumę kontrolną */
			if (checksum != _checksum(buf, buf[2] + 3)) {
				uprintf("+CDC: checksum invalid, ignoring packet\n");
				return;
			}
			//uprintf("+DEBUG:CDC: Got packet: id=%02x, len=%d, checksum=%02x, data[0]=%02x\n", buf[0], buf[2], checksum, buf[3]);

			/* Wysyłamy potwierdzenie odbioru do HU */
			_cdc_putc(0xC5);

			if (buf[1] != last_frame_id) { /* Duplikaty, ignorujemy */
				last_frame_id = buf[1];
				_cdc_packet_recv(&buf[3], buf[2]);
			}
		}
	}
	else if (last_time < rtc_time()) { /* Jeżeli minęła 1 sek i nie mamy danych od HU, wysyłamy status */
		if ((_changer_state == cdcStatePlaying) && ((_cd_state == cdStateLoadingTrack) || (_cd_state == cdStateSearchingTrack))) {
			//uprintf("+DEBUG:CDC: CD -> Playing\n");
			_cd_state = cdStatePlaying;
			_cdc_packet_track_change(trackChangeEntering, _current_track);
			_cdc_packet_cd_operation();
		}

		_cdc_send_state();

		last_time = rtc_time();
	}
}

/* Zatrzymujemy odtwarzanie po wyłączeniu radia */
void cdcemu_radiopower_off(void) {
	uprintf("+CDC:STOP\n");
}

/* Inicjalizacja sprzeu, zmieniarka podłączona do PA9 (TX) i PA10 (RX) USART1 */
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
	gpio_initstruct.GPIO_Mode = GPIO_Mode_IPU;
	gpio_initstruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio_initstruct);

	/* Inicujemy USART1, 9600bps, 8bit, 1 bit stopu */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); /* Zegar */

	USART_StructInit(&usart_initstruct);
	usart_initstruct.USART_BaudRate = 9600;
	usart_initstruct.USART_WordLength = USART_WordLength_9b;
	usart_initstruct.USART_StopBits = USART_StopBits_1;
	usart_initstruct.USART_Parity = USART_Parity_Even;
	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &usart_initstruct);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); /* Przerwanie od RX włączone */
	NVIC_EnableIRQ(USART1_IRQn);
	USART_Cmd(USART1, ENABLE);

	_cdc_reset();
}
