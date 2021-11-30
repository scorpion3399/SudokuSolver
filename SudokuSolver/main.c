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
void TIMER0_COMP_vect();
void TIMER2_COMP_vect();
// void TIMER2_COMP_vect();
void USART_RXC_vector();
void USART_TXC_vector();

// Initializations routines
void initUART();

// Processing routines

void clear_table();
void reset_LED();
void play_game();
void send_table();
void send_response_OK();



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

	// infinite loop
    while (1);
}

void initUART()
{
	UCSRC = (1<<URSEL)|(1<<UCSZ1)|(1<<UCSZ0); // Use 8-bit character sizes
	// Load upper 8-bits of the baud rate value into the high byte of the UBRR register
	UBRRH = BAUD_PRESCALE>>8;
	// Load lower 8-bits of the baud rate value into the low byte of the UBRR register
	UBRRL = BAUD_PRESCALE;
	UCSRB |= (1<<RXCIE)|(1<<TXCIE); // Enable the USART RXC and TXC interrupts
	UCSRB = (1<<RXEN)|(1<<TXEN); // Enable reception and transmission circuitry
}

/*

Subroutine : clear_table

This routine clears the table of sudoku in all cells, setting each cell equal to 0.

*/

void clear_table(){

	for(uint8_t i = 0; i < 9;i++){

		for(uint8_t j = 0; j < 9; j++){

			sudoku[i][j] = 0;

		}


	}


}

/*

Subroutine : reset_LED

Turning off the LEDs which are responsible of diplaying the progress oof sodku's solution.


*/


void reset_LED(){

	pLedDdr = 0xFF;
	pLedOut = 0xFF;


}

/*
Subroutine : play_game



*/

void play_game(){



}


/*

Subroutine : send_table

This routine sends the solution of sudoku as table in PC.


*/


void send_table(){


for(uint8_t i = 0;i < 9;i++){

	for(uint8_t j = 0;j < 9;j++){

		transm_buff[transm_prod] = sudoku[i][j]; // This needs a small modification
		transm_prod++;
	}
}

}

void send_response_OK(){

	transm_buff[transm_prod] = 0x4F
	transm_prod++;
	transm_buff[transm_prod] = 0x4B
	transm_prod++;
	transm_buff[transm_prod] = 0x0D
	transm_prod++;
	transm_buff[transm_prod] = 0x0A
	transm_prod++;


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
	rcv_cons++;

	switch(rcv_buff[rcv_cons]){


		case 0x43:

			if(rcv_buff[rcv_cons+1] == 0x0A && rcv_buff[rcv_cons+1] == 0x0D){
				rcv_cons += 2;

				clear_table();
				reset_LED();
				send_response_OK();
			}

				break;

		case 0x50:

			if(rcv_buff[rcv_cons+1] == 0x0A && rcv_buff[rcv_cons+1] == 0x0D){
				rcv_cons += 2;

				play_game();
				send_response_OK();
			}

				break;

		case 0x54:


			if(rcv_buff[rcv_cons+1] == 0x0A && rcv_buff[rcv_cons+1] == 0x0D){
				rcv_cons += 2;

				send_table();
				send_response_OK();
			}

				break;

	}



	
}

ISR(USART_RXC_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG;

	// If BUFSZ is reached we have to process some data before we receive new.
	// So reti and possibly trigger the process intrpt.
	if (rcv_prod - rcv_cons == BUFSZ)
		goto USART_RXC_vect_RETI;

	rcv_buff[rcv_prod%BUFSZ] = UDR;
	++rcv_prod;

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

/*

C
P
T

*/