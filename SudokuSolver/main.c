/*
 * SudokuSolver.c
 *
 * Created: 24-Nov-21 9:09:04 PM
 * Author : JASON and PANOS
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 1843200UL
#endif // F_CPU

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE F_CPU/16/USART_BAUDRATE - 1
#define BUFSZ 256

#define NLeds 8 // num of LEDs, for the progess of the solver
#define cMaxCnt 151-1 // Max Timer/Counter0 val.
// #define cMaxCnt2 151-1 // Max Timer/Counter2 val.
#define pLedOut PORTA
#define pLedDdr DDRA

//Definig type and struct for sudoku 
#define mytype_t uint8_t

typedef struct implications {
	mytype_t x;
	mytype_t y;
	mytype_t possible_clues[9];
} implication;

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
volatile uint8_t transm_char;

// Pointers to the next clue that will be
// sent back to the PC.
volatile uint8_t row_position = 9;
volatile uint8_t col_position = 9;
// if zero it means the Sudoku is solved
volatile uint8_t test = 0;

// The Sudoku matrix
uint8_t sudoku[9][9] = {
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
static inline void process();
static inline void checkSudoku();
static inline void storeClue();
static inline void clear_table();
static inline void play_game();
static inline void send_table();
static inline void send_response_OK();

// Routines to solve Sudoku
mytype_t valid(mytype_t[][9], mytype_t, mytype_t, mytype_t);
mytype_t solve(mytype_t[][9]);
mytype_t find_empty_cell(mytype_t[][9], mytype_t *, mytype_t *);
void makeImplications(mytype_t puzzle[][9],mytype_t row,mytype_t col,mytype_t guess,implication* imply);
mytype_t count_elements(mytype_t array[9],mytype_t* element);
void undoImplications(mytype_t puzzle[][9],implication* impl);

//Variables for sudoku
int backtracks, backtracks_opt = 0;

int position = 0;

mytype_t sectors[9][4] = {
	{0, 3, 0, 3},{3, 6, 0, 3}, {6, 9, 0, 3},
	{0, 3, 3, 6}, {3, 6, 3, 6}, {6, 9, 3, 6},
	{0, 3, 6, 9}, {3, 6, 6, 9}, {6, 9, 6, 9}
};


// Interrupt Service routines
void TIMER0_COMP_vect();
void USART_RXC_vect();
void USART_TXC_vect();


int main(void)
{
	// Setup stack
	SPL = (uint8_t) RAMEND;
#ifdef SPH
	SPH = RAMEND>>8;
#endif
	// Port settings
	pLedDdr = 0xFF; // set PORTA as output
	pLedOut = 0xFF; // LEDs off

	// Timer settings
	//TCCR0 = (1<<CS02); ; // presc val. 256
	//OCR0 = cMaxCnt; // max tim/cnt0 value 150
	//TIMSK |= (1<<OCIE0); // enable TIM0_COMP interrupt

	// UART init
	initUART();

	sei();

	// infinite loop
	for(;;)
	{
		process();
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

//  rcv_buff[rcv_prod%BUFSZ] = UDR;
//  ++rcv_prod;
	rcv_buff[rcv_prod++] = UDR; // works iff BUFSZ == UINT8_MAX+1

/*  // This code will trigger the Timer2 Comp intrpt. It will only happen
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

	if(transm_cons == transm_prod)
		goto USART_TXC_vector_RETI;

	// Wait for empty transmit buffer
	// while ( !( (UCSRA & 0x20) == 0x20) );
	while ( (UCSRA & 0x20) != 0x20 );

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
 * Description: To begin UART reception and transmission.
 * 
 */
static inline void initUART()
{
	// Load upper 8-bits of the baud rate value into
	// the high byte of the UBRR register
	UBRRH = (BAUD_PRESCALE)>>8;
	// Load lower 8-bits of the baud rate value into
	// the low byte of the UBRR register
	UBRRL = BAUD_PRESCALE;
	 // Enable reception and transmission circuitry
	UCSRB = (1 << RXEN) | (1 << TXEN);
	 // Use 8-bit character sizes
	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
	// Enable the USART RXC and TXC interrupts
	UCSRB |= (1 << RXCIE); // | (1 << TXCIE);
}



static inline void transmit()
{

	// Wait for empty transmit buffer
	// while ( !( (UCSRA & 0x20) == 0x20) );
	while ( (UCSRA & 0x20) != 0x20 );

	UDR  = transm_char; // Sending character as a response
}




/**
 * 
 * Subroutine: process();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: To process commands.
 * 
 */
static inline void process()
{
	switch(rcv_buff[rcv_cons])
	{
		case 0x41: // 'A', "AT\r\n", sends response "OK\r\n"
		
			// uint8_t cmd[3] = {0x54,0x0D,0x0A}; // char cmd[3] = "T\r\n";
			// cmd bytes are hardcoded because, there can't be a var
			// definition inside a case. Maybe global or PROGMEM string?
			if (rcv_buff[rcv_cons+1] == 0x54 && rcv_buff[rcv_cons+2] == 0x0D && rcv_buff[rcv_cons+3] == 0x0A && rcv_buff[rcv_cons+4] == 0x0D )
			{   // Update rcv consumer.
				rcv_cons = rcv_cons + 5; // Update rcv consumer.
				
				// respond with "OK\CR\LF"
				send_response_OK();

			} else {
				// Eat everything until an '\LF' is found because the cmd
				// is not correct.
				//do { } while ( rcv_buff[++rcv_cons] != 0x0A );
			}
			break;

		case 0x43: // 'C' "C\r\n"
		// "C\r\n", clears the Sudoku table and notifies PC ("OK\r\n")

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D )
			{
				rcv_cons += 4;
				clear_table();
				send_response_OK();
			}

			break;

		case 0x4E: // 'N', "N<x><y><val>\r\n", which stores a clue and returns OK
			
			storeClue();
			
			break;

		case 0x50: // 'P' "P\r\n"

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D )
			{
				rcv_cons += 4;

				play_game();
				send_response_OK();
			}

			break;

		case 0x53: // 'S', "S\r\n", check if the Sudoku is correctly solved.
			checkSudoku();
			break;

		case 0x54: // 'T' "T\r\n"

			if(rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D )
			{
				rcv_cons += 4;

				send_table();
				send_response_OK();
				if (col_position == 9 && row_position == 9)
				{
					// store "D\r\n" in the transmit buff
					transm_char = 0x44;
					transmit();
					transm_char = tx_OK[2];
					transmit();
					transm_char = tx_OK[3];
					transmit();
				}
			}

			break;
		
		default: // no matching cmd, eat bytes
			
			//do { } while ( rcv_buff[++rcv_cons] != 0x0A );
			break;
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
static inline void storeClue()
{
	if (rcv_buff[rcv_cons+4] == 0x0D && rcv_buff[rcv_cons+5] == 0x0A &&  rcv_buff[rcv_cons+6] == 0x0D)
	{
		
		DDRB = 0xFF;
		PORTB = 0x0F;
		// array indices are from 0-8, but the cmd indices are from 0x31-0x39
		// use of postfix is necessary because rcv_cons++ will return 
		// rcv_buff[rcv_cons] and then increment rcv_cons.
		uint8_t x = (rcv_buff[++rcv_cons] & 0x0F) - 1;
		uint8_t y = (rcv_buff[++rcv_cons] & 0x0F) - 1;
		sudoku[x][y] = (rcv_buff[++rcv_cons] & 0x0F);
		// Update rcv consumer.
		rcv_cons= rcv_cons+4;
		// respond with "OK\CR\LF"
		send_response_OK();
	} else {
		// Eat everything until an '\LF' is found because the cmd
		// is not correct.
		//do { } while ( rcv_buff[++rcv_cons] != 0x0A );
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
	// first check that the cmd is correct (ends in \r\n in this case)
	if (rcv_buff[rcv_cons+1] == 0x0D &&
		rcv_buff[rcv_cons+2] == 0x0A)
	{
		uint8_t checksum[9] = {1,2,3,4,5,6,7,8,9};
		uint8_t i, j = 0; // uint8_t i = 0; uint8_t j = 0;

		// check every COLUMN
		for (i = 8; i >= 0; i--)
		{
			for (j = 8; j >= 0; j--)
			{
				checksum[sudoku[j][i]-1] = 0;
			}

			j = 9;

			do {
				// If any checksum array elem is other than zero,
				// then an 
				test |= checksum[j-1];
			} while (--j >= 0);
		}

		// check every ROW
		for (j = 8; j >= 0; j--)
		{
			// reinitialize checksum
			for (i = 8; i >= 0; i--)
			{
				checksum[i] = i+1;
			}
			
			for (i = 8; i >= 0; i--)
			{
				checksum[sudoku[j][i]-1] = 0;
			}

			i = 9;

			do {
				// If any checksum array elem is other than zero,
				// then an
				test |= checksum[i-1];
			} while (--i >= 0);
		}

		// Check every Sudoku BOX (3x3)                      |
		// access pattern: ( left to right ---->, up to down | )
		//                                                   V
		// (9,9),(8,9),(7,9) | (9,8),(8,8),(7,8) | (9,7),(8,7),(7,7) 
		// (6,9),(5,9),(4,9) | (6,8),(5,8),(4,8) | (6,7),(5,7),(4,7)
		// (3,9),(2,9),(1,9) | (3,8),(2,8),(1,8) | (3,7),(2,7),(1,7)
		
		// (9,6),(8,6),(7,6) | (9,5),(8,5),(7,5) | (9,4),(8,4),(7,4)
		// (6,6),(5,6),(4,6) | (6,5),(5,5),(4,5) | (6,4),(5,4),(4,4)
		// (3,6),(2,6),(1,6) | (3,5),(2,5),(1,5) | (3,4),(2,4),(1,4)
		
		// (9,3),(8,3),(7,3) | (9,2),(8,2),(7,2) | (9,1),(8,1),(7,1)
		// (6,3),(5,3),(4,3) | (6,2),(5,2),(4,2) | (6,1),(5,1),(4,1)
		// (3,3),(2,3),(1,3) | (3,2),(2,2),(1,2) | (3,1),(2,1),(1,1)

		for (uint8_t c = 2; c >= 0; c--)
		{
			for (uint8_t r = 2; r >= 0; r--)
			{
				for (j = (c+1)*3-1; j >= 3*c; j--)
				{
					for (i = (r+1)*3-1; i >= 3*r; i--)
					{
						checksum[sudoku[i][j]-1] = 0;
					}
				}

				i = 9;

				do {
					// If any checksum array elem is other than zero,
					// then an
					test |= checksum[i-1];
				} while (--i >= 0);
			
				// reinitialize checksum
				for (i = 8; i >= 0; i--)
				{
					checksum[i] = i+1;
				}
			}
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
	uint8_t i, j = 0;
	
	for (i = 8; i >= 0; i--)
	{
		for (j = 8; j >= 0; j--)
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
	uint8_t result = solve(sudoku);

	if(result == 1){
		DDRB = 0xFF;
		PORTB = 0x00;
	}

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

	transm_char = 0x30+row_position;
	transmit();
	transm_char = 0x30+col_position;
	transmit();
	transm_char = 0x30+sudoku[row_position-1][col_position-1];
	transmit();
	transm_char = 0x0D;
	transmit();
	transm_char = 0x0A;
	transmit();
	// Increasing the global positions

	col_position--;

	if(col_position == 0)
	{
		col_position = 9;
		row_position--;
	}

	if(row_position == 0)
	{
		row_position = 9;
		col_position = 9;
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

	transm_char = tx_OK[0];
	transmit();
	transm_char = tx_OK[1];
	transmit();
	transm_char = tx_OK[2];
	transmit();
	transm_char = tx_OK[3];
	transmit();

}



// Sudoku solving Routines

mytype_t valid(mytype_t puzzle[][9], mytype_t row, mytype_t column, mytype_t guess) {
	mytype_t corner_x = row / 3 * 3;
	mytype_t corner_y = column / 3 * 3;

	for (mytype_t x = 0; x < 9; ++x)
	{
		if (puzzle[row][x] == guess) return 0;
		if (puzzle[x][column] == guess) return 0;
		if (puzzle[corner_x + (x / 3)][corner_y + (x % 3)] == guess) return 0;
	}
	return 1;
}

mytype_t find_empty_cell(mytype_t puzzle[][9], mytype_t *row, mytype_t *column) {
	for (mytype_t x = 0; x < 9; x++)
	{
		for (mytype_t y = 0; y < 9; y++)
		{
			if (!puzzle[x][y])
			{
				*row = x;
				*column = y;

				return 1;
			}
		}
	}
	return 0;
}

/*

void makeImplications(mytype_t puzzle[][9],mytype_t row,mytype_t col,mytype_t guess,implication* imply){

	imply[position].x = row;
	imply[position].y = col;
	imply[position].possible_clues[0] = guess;
	position++;

	puzzle[row][col] = guess;
	mytype_t index = 0;
	
	mytype_t value;

	mytype_t possible_clues[9];
	implication impl[9];

	// Removing clues from possible clues which has already been in the ith sector with clue (row,col)

	for(mytype_t i = 0;i < 9;i++){

		possible_clues[0] = 1;
		possible_clues[1] = 2;
		possible_clues[2] = 3;
		possible_clues[3] = 4;
		possible_clues[4] = 5;
		possible_clues[5] = 6;
		possible_clues[6] = 7;
		possible_clues[7] = 8;
		possible_clues[8] = 9;

		for(mytype_t x = sectors[i][0];x < sectors[i][1];x++){

			for(mytype_t y = sectors[i][2];y < sectors[i][3];y++){

				if(puzzle[x][y] != 0)
				//remove_element(possible_clues,puzzle[x][y]);
				// Removing element puzzle[x][y] from possibles_clues
				for(mytype_t m = 0;m < 9;m++){

					if(possible_clues[m] == puzzle[x][y])
					possible_clues[m] = 0;
				}

				
			}

		}



		// Setting the possible clues for each clue (x,y) in the ith sector
		index = 0;
		for(mytype_t x = sectors[i][0];x < sectors[i][1];x++){

			for(mytype_t y = sectors[i][2];y < sectors[i][3];y++){

				if(puzzle[x][y] == 0){
					// store the tuple in x,y, elements
					impl[index].x = x;
					impl[index].y = y;
					//array_copy(impl[index].possible_clues,possible_clues);
					for(mytype_t m = 0;m < 9;m++){
						//if(possible_clues[m] != 0)
						impl[index].possible_clues[m] = possible_clues[m];
					}

					index++;
				}
			}

		}
		// For each sector
		
		for(mytype_t j = 0; j < index; j++){

			// Finding the set of clues on the row corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			
			for(mytype_t y = 0; y < 9;y++){

				//	if(find_element(impl[j].possible_clues,puzzle[impl[j].x][y]))
				//	remove_element(impl[j].possible_clues,puzzle[impl[j].x][y]);

				for(mytype_t m = 0;m < 9;m++){

					if(impl[j].possible_clues[m] == puzzle[impl[j].x][y])
					impl[j].possible_clues[m] = 0;
				}



			}

			// Finding the set of clues on the column corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			
			for(mytype_t x = 0; x < 9;x++){

				//if(find_element(impl[j].possible_clues,puzzle[x][impl[j].y]))
				//remove_element(impl[j].possible_clues,puzzle[x][impl[j].y]);

				for(mytype_t m = 0;m < 9;m++){

					if(impl[j].possible_clues[m] == puzzle[x][impl[j].y])
					impl[j].possible_clues[m] = 0;
				}


				
			}

			// Check if in the set of possible values there is only one clue

			if(count_elements(impl[j].possible_clues,&value) == 1)
			if(valid(puzzle, impl[j].x, impl[j].y, value)){
				puzzle[impl[j].x][impl[j].y] = value;
				imply[position].x = impl[j].x;
				imply[position].y = impl[j].y;
				imply[position].possible_clues[0] =value;
				position++;
			}
		}
	}

}

mytype_t count_elements(mytype_t array[9],mytype_t* element){

	mytype_t counter = 0;

	for(mytype_t i = 0;i < 9;i++){

		if(array[i] != 0 ){
			counter++;
			*element = array[i];
		}
	}

	return counter;
}


void undoImplications(mytype_t puzzle[][9],implication* impl){

	for(mytype_t i = 0;i < 81;i++){

		puzzle[impl[i].x][impl[i].y] = 0;

	}


}

*/

mytype_t solve(mytype_t puzzle[][9]) {
	
	mytype_t row, column;

	if(!find_empty_cell(puzzle, &row, &column)) return 1;

	for (mytype_t guess = 1; guess < 10; guess++) {
		if (valid(puzzle, row, column, guess)) {
			puzzle[row][column] = guess;

			if(solve(puzzle)) return 1;

			backtracks++;
			puzzle[row][column] = 0;
		}
	}

	return 0;
}


/*
mytype_t solve_opt(mytype_t puzzle[][9]) {
	mytype_t row;
	mytype_t column;

	implication* impl = malloc(700);

	if(!find_empty_cell(puzzle, &row, &column)) return 1;

	for (mytype_t guess = 1; guess < 10; guess++) {
		if (valid(puzzle, row, column, guess)) {

			//puzzle[row][column] = guess;

			makeImplications(puzzle,row,column,guess,impl);

			if(solve(puzzle)) return 1;

			backtracks_opt++;
			undoImplications(puzzle,impl);
			//puzzle[row][column] = 0;
		}
	}

	free(impl);
	return 0;
	
}
*/

