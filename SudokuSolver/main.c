/*
 * SudokuSolver.c
 *
 * Created: 24-Nov-21 9:09:04 PM
 * Author : JASON and PANOS
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 10000000UL
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE F_CPU/16/USART_BAUDRATE - 1
#define BUFSZ 256

uint8_t rcv_buff[BUFSZ];
uint8_t rcv_prod = 0;
uint8_t rcv_cons = 0;
uint8_t transm_buff[BUFSZ];
int transm_consumer = 0;
int transm_producer = 0;


void USART_RXC_vector();
void USART_TXC_vector();
void initUART();

int main(void)
{
	initUART();
	sei();
	// inf loop
    while (1);
}

void initUART()
{
	UCSRB = (1<<RXEN)|(1<<TXEN); // Enable transmission and reception circuitry
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); // Use 8-bit character sizes
	// Load upper 8-bits of the baud rate value into the high byte of the UBRR register
	UBRRH = BAUD_PRESCALE>>8;
	// Load lower 8-bits of the baud rate value into the low byte of the UBRR register
	UBRRL = BAUD_PRESCALE;
	UCSRB |= (1<<RXCIE)|(1<<TXCIE); // Enable the USART RXC and TXC interrupt flags
}

ISR(USART_RXC_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG;
	
	if (rcv_cons == BUFSZ)
		goto USART_RXC_vect_RETI;
		
	rcv_buff[(rcv_prod+rcv_cons)%BUFSZ] = UDR;
	rcv_cons++;

USART_RXC_vect_RETI:
	SREG = save_sreg;
	
	reti();
}


ISR(USART_TXC_vector, ISR_NAKED)
{
	uint8_t save_sreg;

	save_SREG = SREG; // Storing the value of status register

	if(transm_consumer - transm_producer == 0)
		goto USART_TXC_vector_RETI

	UDR  = transm_buff[transm_consumer]; // Sending character as a response

	transm_consumer = (transm_consumer +1)%BUFSZ; // Increasing the position of pointer in buffer transm_buffer
	
USART_TXC_vector_RETI:

	SREG = save_sreg; // Loading the value of status register

	reti();

}