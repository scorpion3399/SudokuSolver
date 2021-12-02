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
uint8_t transm_buff[BUFSZ];
volatile uint8_t rcv_prod = 0;
volatile uint8_t rcv_cons = 0;
volatile uint8_t transm_cons = 0;
volatile uint8_t transm_prod = 0;

// Pointers to the next clue that will be
// sent back to the PC.
volatile uint8_t row_position = 8;
volatile uint8_t col_position = 8;

// The Sudoku matrix
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

// constants
const uint8_t tx_OK[4] = {0x4F,0x4B,0x0D,0x0A};

// Subroutines
static inline void initUART();
static inline void checkSudoku();
static inline void storeClue();
static inline void clear_table();
static inline void reset_LED();
static inline void play_game();
static inline void send_table();
static inline void send_response_OK(uint8_t tx_OK[4]);

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

	// infinite loop
    for(;;)
	{
		/* This is an eternal loop*/
	}
}





// ********************** Interrupt Service Routines ********************** //

/**
 *
 * Subroutine: Interrupt Service Routine for Timer 0 Compare
 * 
 * Input: None
 * 
 * Returns: None
 *
 * Description: This ISR displays the progress of the sudoku solver in the LED00-LED07.
 * 
 */
ISR(TIMER0_COMP_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG; // save SREG

	uint8_t cnt_progress = 0; // store the number of unsolved clues

	for (uint8_t r = 8; r >= 0; r--)
	{
		for (uint8_t c = 8; c >= 0; c--)
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
	
	TCNT0 = 0; // reset TCNT0
	
	SREG = save_sreg; // restore SREG
	
	reti(); // return from interrupt
}


/**
 *
 * Subroutine: Interrupt Service Routine for Timer 2 Compare
 * 
 * Input: None
 * 
 * Returns: None
 *
 * Description: This ISR process each command receiving from USART.
 *
 */

ISR(TIMER2_COMP_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG; // save SREG
	
	switch(rcv_buff[rcv_cons])
	{
		case 0x41: // 'A', "AT\r\n", sends response "OK\r\n"
			
			// uint8_t cmd[3] = {0x54,0x0D,0x0A}; // char cmd[3] = "T\r\n";
			// cmd bytes are hardcoded because, there can't be a var
			// definition inside a case. Maybe global or PROGMEM string?
			if (rcv_buff[rcv_cons+1] == 0x54 &&
				rcv_buff[rcv_cons+2] == 0x0D &&
				rcv_buff[rcv_cons+3] == 0x0A)
			{	// Update rcv consumer.
				rcv_cons = rcv_cons + 4;
				// respond with "OK\CR\LF"
				send_response_OK();

			} else {
				// Eat everything until an '\LF' is found because the cmd
				// is not correct.
				do { } while ( rcv_buff[++rcv_cons] != 0x4A );
			}
			break;

		case 0x43: // 'C' "C\r\n"
		// "C\r\n", clears the Sudoku table and notifies PC ("OK\r\n")

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A)
			{
				rcv_cons += 3;

				clear_table();
				send_response_OK();
			}

			break;

		case 0x4E: // 'N', "N<x><y><val>\r\n", which stores a clue and returns OK
			
			storeClue();
			break;

		case 0x50: // 'P' "P\r\n"

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A)
			{
				rcv_cons += 3;

				play_game();
				send_response_OK();
			}

			break;

		case 0x53: // 'S', "S\r\n"

			break;

		case 0x54: // 'T' "T\r\n"

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A)
			{
				rcv_cons += 3;

				send_table();
				send_response_OK();
			}

			break;
		
		case 0x4F: // 'O', "OK\r\n"
			
			break;
		
		default: // no matching cmd, eat bytes
			
			do { } while ( rcv_buff[++rcv_cons] != 0x4A );
			break;
	}
	
	SREG = save_sreg; // restore SREG
	
	reti(); // return from interrupt
}


/**
 * 
 * Subroutine: Interrupt Service Routine for USART receive
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This ISR receives the data from USART.
 * 
 */

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

/**
 * 
 * Subroutine: Interrupt Service Routine transmit
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This ISR transmits the data in PC.
 * 
 */

ISR(USART_TXC_vect, ISR_NAKED)
{
	uint8_t save_sreg = SREG; // Storing the value of status register

	if(transm_cons == 0)
		goto USART_TXC_vector_RETI;

	UDR  = transm_buff[transm_cons++]; // Sending character as a response

	// transm_cons = (transm_cons+1)%BUFSZ; // Increasing the position of pointer in buffer transm_buffer
	
USART_TXC_vector_RETI:

	SREG = save_sreg; // Loading the value of status register

	reti();

}



// *********** SUBROUTINES *********** //


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



// /*
//  * 
//  * Subroutine: sendOK();
//  * 
//  * Input: none
//  * 
//  * Returns: nothing
//  * 
//  * Description: Jusy send an OK.
//  * 
//  */
 
// static inline void sendOK()
// {

// }


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
static inline void storeClue()
{
	if (rcv_buff[rcv_cons] == 0x54 &&
		rcv_buff[rcv_cons+4] == 0x0D &&
		rcv_buff[rcv_cons+5] == 0x0A)
	{
		// array indices are from 0-8, but the cmd indices are from 0x31-0x39
		// use of postfix is necessary because rcv_cons++ will return 
		// rcv_buff[rcv_cons] and then increment rcv_cons.
		uint8_t x = (rcv_buff[++rcv_cons] & 0x0F) - 0x31;
		uint8_t y = (rcv_buff[++rcv_cons] & 0x0F) - 0x31;
		sudoku[x][y] = (rcv_buff[++rcv_cons] & 0x0F) - 0x30;
		// Update rcv consumer.
		rcv_cons = rcv_cons + 2;
		// respond with "OK\CR\LF"
		send_response_OK();
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


/**
 * 
 * Subroutine: clear_table
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This routine clears the table of sudoku in all cells, setting each cell equal to 0.
 * 
 */
static inline void clear_table()
{
	for (uint8_t i = 8; i >= 0; i--)
	{
		for (uint8_t j = 8; j >= 0; j--)
		{
			sudoku[i][j] = 0;
		}
	}
}


/**
 *
 * Subroutine: play_game
 * 
 * Input: None
 * 
 * Return: None
 * 
 * Description: 
 * 
 */

static inline void play_game()
{

}


/**
 * 
 * Subroutine: send_table
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This routine sends the solution of sudoku as table in PC.
 * 
 */

static inline void send_table()
{
	// Sending each time a cell in the form N<X><Y><CR><LF>

		transm_buff[transm_prod] = 0x30+row_position;
		transm_prod++;
		transm_buff[transm_prod] = 0x30+col_position;
		transm_prod++;
		transm_buff[transm_prod] = 0x30+sudoku[row_position][col_position]; // This needs a small modification
		transm_prod++;
		transm_buff[transm_prod] = 0x0D;
		transm_prod++;
		transm_buff[transm_prod] = 0x0A;
		transm_prod++;

	// Increasing the global positions

		col_position--;

		if(col_position == -1){

			col_position = 8;
			row_position--;
		}

		if(row_position == -1){
			row_position = 8;
			col_position = 8;
		}

}

/**
 * 
 * Subroutine: send_response_OK
 * 
 * Input: tx_OK
 * 
 * Returns: None
 * 
 * Description:
 * This routine sends response OK<CR><LF> in PC in order to notify it that
 * the command has being executed successfully.
 * 
 */

static inline void send_response_OK()
{
	// transm_buff[transm_prod] = tx_OK[0];
	// transm_prod++;
	// transm_buff[transm_prod] = tx_OK[1];
	// transm_prod++;
	// transm_buff[transm_prod] = tx_OK[2];
	// transm_prod++;
	// transm_buff[transm_prod] = tx_OK[3];
	// transm_prod++;

	transm_buff[transm_prod] = tx_OK[0];
	transm_buff[++transm_prod] = tx_OK[1];
	transm_buff[++transm_prod] = tx_OK[2];
	transm_buff[++transm_prod++] = tx_OK[3];
}
