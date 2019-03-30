#ifndef __USART_H
#define __USART_H

void uprintf(const char * fmt, ...);
char * usart_readcommand(char * buf, int bufsz);
void usart_init(void);


#endif /* __USART_H */
