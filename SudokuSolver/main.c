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


#define NLeds 8 // num of LEDs, for the progess of the solver
#define cMaxCnt 151-1 // Max Timer/Counter0 val.
// #define cMaxCnt2 151-1 // Max Timer/Counter2 val.
#define pLedOut PORTA
#define pLedDdr DDRA

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

// Subroutines
static inline void initUART();
static inline void checkSudoku();
static inline void storeClue();

// Interrupt Service routines
void TIMER0_COMP_vect();
void TIMER2_COMP_vect();
void USART_RXC_vector();
void USART_TXC_vector();

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
	TCCR0 = (1<<CS02); ; // presc val. 256
	OCR0 = cMaxCnt; // max tim/cnt0 value 150
	TIMSK |= (1<<OCIE0); // enable TIM0_COMP interrupt
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

ISR(TIMER0_COMP_vect, ISR_NAKED)
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

ISR(TIMER2_COMP_vect, ISR_NAKED)
{
	// save SREG
	uint8_t save_sreg = SREG;

	// common response.
	uint8_t tx_OK[4] = {0x4F,0x4B,0x0D,0x0A};

	switch (rcv_buff[rcv_cons])
	{
	case 0x41: // 'A', "AT\r\n", which just returns OK
		sendOK();
		// work is done
		break;
	
	case 0x4E: // 'N', "N<x><y><val>\r\n", which stores a clue and returns OK
		storeClue();
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


ISR(USART_RXC_vect, ISR_NAKED)
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


ISR(USART_TXC_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG; // Storing the value of status register

	if(transm_cons == 0)
		goto USART_TXC_vector_RETI;

	UDR  = transm_buff[transm_cons]; // Sending character as a response

	transm_cons = (transm_cons+1)%BUFSZ; // Increasing the position of pointer in buffer transm_buffer
	
USART_TXC_vector_RETI:

	SREG = save_sreg; // Loading the value of status register

	reti();

}




// ***********  SUBROUTINES *********** //


/**
 * 
 * Subroutine: initUART();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: To begin
 * 
 */
static inline void initUART()
{
	UCSRB = (1<<RXEN)|(1<<TXEN); // Enable reception and transmission circuitry
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); // Use 8-bit character sizes
	// Load upper 8-bits of the baud rate value into the high byte of the UBRR register
	UBRRH = BAUD_PRESCALE>>8;
	// Load lower 8-bits of the baud rate value into the low byte of the UBRR register
	UBRRL = BAUD_PRESCALE;
	UCSRB |= (1<<RXCIE)|(1<<TXCIE); // Enable the USART RXC and TXC interrupts
}



/**
 * 
 * Subroutine: sendOK();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: Jusy send an OK.
 * 
 */
static inline void sendOK()
{
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
}


/**
 * 
 * Subroutine: storeClue();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: Checks that the sudoku matrix is corect.
 * 
 */
static inline storeClue()
{

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
}



/**
 * 
 * Subroutine: checkSudoku();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: Checks that the sudoku matrix is corect.
 * 
 */
static inline void checkSudoku()
{
	if (rcv_buff[rcv_cons+1] == 0x0D &&
	rcv_buff[rcv_cons+2] == 0x0A)
	{
		uint8_t checksum[9] = {1,2,3,4,5,6,7,8,9};
		uint8_t test = 0;

		for (uint8_t i = 8; i >= 0; i--)
		{
			for (uint8_t j = 8; j >= 0; j--)
			{
				checksum[sudoku[j][i]-1] = 0;
			}

			j = 9;

			do {
			 	test |= checksum[j];
			} while (--j >= 0);
		}

		checksum[9] = {1,2,3,4,5,6,7,8,9};

		for (uint8_t i = 8; i >= 0; i--)
		{
			for (uint8_t j = 8; j >= 0; j--)
			{
				sudoku[i][j]
				
			}

			do {
				checksum[]
			} while (i >= 0);
		}

	}
}