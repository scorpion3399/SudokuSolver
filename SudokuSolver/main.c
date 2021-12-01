/*
 * SudokuSolver.c
 *
 * Created: 24-Nov-21 9:09:04 PM
 * Author : JASON and PANOS
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 1843200UL
#endif

// #ifndef BAUDRATE
#define BAUD 9600
// #endif

#define BAUD_PRESCALE F_CPU/16/BAUD - 1
#define BUFSZ 256


#define NLeds 8 // num of LEDs, for the progess of the solver
#define cMaxCnt 151-1 // Max Timer/Counter0 val.
// #define cMaxCnt2 151-1 // Max Timer/Counter2 val.
#define pLedOut PORTB
#define pLedDdr DDRB

// TODO: Throw them all in a struct. Has some size optimizations.
// Or maybe execution time. (Who knows)
// (Although some variables are volatile which changes)

// This must hold:  UINT8_MAX + 1 is evenly divisible by BUFSZ
// Otherwise, the producers and consumers do not work correctly.
uint8_t rcv_buff[BUFSZ];
volatile uint8_t rcv_prod = 0;
volatile uint8_t rcv_cons = 0;
uint8_t transm_buff[BUFSZ];
volatile uint8_t transm_cons = 0;
volatile uint8_t transm_prod = 0;

volatile uint8_t sudoku[9][9] = {
	{3, 0, 6, 5, 0, 8, 4, 0, 0}, 
	{5, 2, 0, 0, 0, 0, 0, 0, 0}, 
	{0, 8, 7, 0, 0, 0, 0, 3, 1}, 
	{0, 0, 3, 0, 1, 0, 0, 8, 0}, 
	{9, 0, 0, 8, 6, 3, 0, 0, 5}, 
	{0, 5, 0, 0, 9, 0, 6, 0, 0}, 
	{1, 3, 0, 0, 0, 0, 2, 5, 0}, 
	{0, 0, 0, 0, 0, 0, 0, 7, 4}, 
	{0, 0, 5, 2, 0, 6, 3, 0, 0}
};

// subroutines

// Interrupt Service routines
void TIMER0_COMPA_vect();
void TIMER2_COMPA_vect();
// void TIMER2_COMP_vect();
void USART_RX_vector();
void USART_TX_vector();
void initUART();

int main(void)
{
	// Setup stack
	SPL = RAMEND;
#ifdef SPH
	SPH = RAMEND>>8;
#endif
	// Port settings
	pLedDdr = 0xFF; // set PORTA as output
	pLedOut = 0xFF; // LEDs off

	// Timer settings
	TCCR0B |= (1<<CS02); ; // presc val. 256
	OCR0A = cMaxCnt; // max tim/cnt0 value 150
	TIMSK0 |= (1<<OCIE0A); // enable TIM0_COMP interrupt
	// TCCR2 = (1<<CS22)|(1<<CS21); // presc val. 1024
	// OCR2 = cMaxCnt2; // max tim/cnt2 value

	// UART init
	initUART();

	sei();

	for(;;)
	{
		/* This is an eternal loop*/
	}

}

void initUART()
{
	UCSR0B |= (1<<RXEN0)|(1<<TXEN0); // Enable reception and transmission circuitry
	UCSR0C |= (1<<UCSZ01)|(1<<UCSZ00); // Use 8-bit character sizes
	// Load upper 8-bits of the baud rate value into the high byte of the UBRR register
	UBRR0H |= BAUD_PRESCALE>>8;
	// Load lower 8-bits of the baud rate value into the low byte of the UBRR register
	UBRR0L |= BAUD_PRESCALE;
	UCSR0B |= (1<<RXCIE0)|(1<<TXCIE0); // Enable the USART RXC and TXC interrupts
}


ISR(TIMER0_COMPA_vect, ISR_NAKED)
{
	// save SREG
	uint8_t save_sreg = SREG;

	uint8_t cnt_progress = 0; // store the number of unsolved clues

	for (uint8_t r = 0; r < 9; ++r)
	{
		for (uint8_t c = 0; c < 9; ++c)
		{
			if (sudoku[r][c] != 0)
				cnt_progress++;
		}
	}

	cnt_progress = 0.1 * cnt_progress; // cnt_progress / 10
	// Refresh screen
	// if LEDs are off and cnt_progress sudoku has completed more than 10 clues
	if (pLedOut == 0xFF && cnt_progress >= 1) {
		pLedOut = (0xFF << cnt_progress);
	}
	// LEDs were on, so now need to be off
	// or less than 10% is completed.
	else {
		pLedOut = 0xFF;
	}
	// reset TCNT0
	TCNT0 = 0;
	// restore SREG
	SREG = save_sreg;
	// return from interrupt
	reti();
}

ISR(TIMER2_COMPA_vect, ISR_NAKED)
{
	// save SREG
	uint8_t save_sreg = SREG;

	// common response.
	uint8_t tx_OK[4] = {0x4F,0x4B,0x0D,0x0A};

	switch (rcv_buff[rcv_cons])
	{
	case 0x41: // 'A', "AT\r\n", which just returns OK
		// uint8_t cmd[3] = {0x54,0x0D,0x0A}; // char cmd[3] = "T\r\n";
		// cmd bytes are hardcoded because, there can't be a var
		// definition inside a case. Maybe global or PROGMEM string?
		if (rcv_buff[rcv_cons+1] == 0x54 &&
			rcv_buff[rcv_cons+2] == 0x0D &&
			rcv_buff[rcv_cons+3] == 0x0A)
		{	// Update rcv consumer.
			rcv_cons = rcv_cons + 4;
			// respond with "OK\CR\LF"
			transm_buff[transm_prod] = tx_OK[0];
			transm_buff[++transm_prod] = tx_OK[1];
			transm_buff[++transm_prod] = tx_OK[2];
			transm_buff[++transm_prod] = tx_OK[3];
		} else {
			// Eat everything until an '\LF' is found because the cmd
			// is not correct.
			do { } while ( rcv_buff[++rcv_cons] != 0x4A );
		}
		// work is done
		break;
	
	case 0x4E: // 'N', "N<x><y><val>\r\n", which stores a clue and returns OK
		if (rcv_buff[rcv_cons] == 0x54 &&
			rcv_buff[rcv_cons+4] == 0x0D &&
			rcv_buff[rcv_cons+5] == 0x0A)
		{
			// array indices are from 0-8, but the cmd indices are from 0x31-0x39
			// use of postfix is necessary because rcv_cons++ will return 
			// rcv_buff[rcv_cons] and then increment rcv_cons.
			uint8_t i = rcv_buff[((++rcv_cons) & 0x0F)-0x31];
			uint8_t j = rcv_buff[((++rcv_cons) & 0x0F)-0x31];
			sudoku[i][j] = (rcv_buff[++rcv_cons] & 0x0F)-0x30;
			// Update rcv consumer.
			rcv_cons = rcv_cons + 2;
			// respond with "OK\CR\LF"
			transm_buff[transm_prod] = tx_OK[0];
			transm_buff[transm_prod++] = tx_OK[1];
			transm_buff[transm_prod++] = tx_OK[2];
			transm_buff[transm_prod++] = tx_OK[3];
		} else {
		// Eat everything until an '\LF' is found because the cmd
		// is not correct.
		do { } while ( rcv_buff[++rcv_cons] != 0x4A );
		}
		break;
	
	case 0x53: // 'S', "S\r\n"
		break;
		
	case 0x4F: // 'O', "OK\r\n"
		break;
		
	default: // no matching cmd, eat bytes
		do { } while ( rcv_buff[++rcv_cons] != 0x4A );
		break;
	}
	
	
	SREG = save_sreg;
	// return from interrupt
	reti();
}

ISR(USART_RX_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG;

	// If BUFSZ is reached we have to process some data before we
	// receive new. So reti and possibly trigger the process intrpt.
	// Change to USART_RXC_vect_TRIG to trigger intrpt.
	if (rcv_prod - rcv_cons == BUFSZ)
		goto USART_RXC_vect_RETI;

// 	rcv_buff[rcv_prod%BUFSZ] = UDR;
// 	++rcv_prod;
	rcv_buff[rcv_prod++] = UDR; // works iff BUFSZ == UINT8_MAX+1


/*	// This code will trigger the Timer2 Comp intrpt. It will only happen
	// on buffer overflow. Until everything works this is disabled.
	goto USART_RXC_vect_RETI;
	
USART_RXC_vect_TRIG:
	trigger it
	sei();
	TCNT2 = MaxCnt;
	_NOP();
	cli();
*/
USART_RXC_vect_RETI:
	SREG = save_sreg;

	reti();
}


ISR(USART_TX_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG; // Storing the value of status register

	if(transm_cons == 0)
		goto USART_TXC_vector_RETI;

	// Sending character as a response
	// Increasing the position of pointer in buffer transm_buffer
	UDR0  = transm_buff[transm_cons++];
	
USART_TXC_vector_RETI:

	SREG = save_sreg; // Loading the value of status register

	reti();

}
