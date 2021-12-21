/*
 * SudokuSolver.c
 *
 * Created: 24-Nov-21 9:09:04 PM
 * Author : JASON and PANOS
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>


#ifndef F_CPU
#define F_CPU 10000000UL
#endif // F_CPU

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE F_CPU/16/USART_BAUDRATE - 1
#define BUFSZ 256

#define cMaxCnt 151-1 // Max Timer/Counter0 val.
// #define cMaxCnt2 151-1 // Max Timer/Counter2 val.
#define pLedOut PORTA
#define pLedDdr DDRA

// This must hold:  UINT8_MAX + 1 is evenly divisible by BUFSZ
// Otherwise, the producers and consumers do not work correctly.
uint8_t rcv_buff[BUFSZ];
volatile uint8_t rcv_prod;
volatile uint8_t rcv_cons;
volatile uint8_t transm_cons;
volatile uint8_t transm_prod;
volatile uint8_t transm_char;

// Pointers to the next clue that will be
// sent back to the PC.
volatile uint8_t row_position = 1;
volatile uint8_t col_position = 1;
// if one means the solution of sudoku stopped
volatile uint8_t stopSolved;

typedef struct implications {
	uint8_t x;
	uint8_t y;
	uint8_t possible_clues[9];
} implication;

// The Sudoku matrix
uint8_t sudoku[9][9] = {
	{0,0,5,3,0,0,0,0,0},
	{8,0,0,0,0,0,0,2,0},
	{0,7,0,0,1,0,5,0,0},
	{4,0,0,0,0,5,3,0,0},
	{0,1,0,0,7,0,0,0,6},
	{0,0,3,2,0,0,0,8,0},
	{0,6,0,5,0,0,0,0,9},
	{0,0,4,0,0,0,0,3,0},
	{0,0,0,0,0,9,7,0,0}
};

uint8_t sectors[9][4] = {
	{0, 3, 0, 3}, {3, 6, 0, 3}, {6, 9, 0, 3},
	{0, 3, 3, 6}, {3, 6, 3, 6}, {6, 9, 3, 6},
	{0, 3, 6, 9}, {3, 6, 6, 9}, {6, 9, 6, 9}
};

// constants
const uint8_t tx_OK[4] = { 0x4F, 0x4B, 0x0D, 0x0A };

// Subroutines
 void initUART();
 void process();
 uint8_t checkSudoku();
 void storeClue();
 void clear_table();
 void play_game();
 void send_table();
 void debug_table();
 void send_response_OK();

// Routines to solve Sudoku
uint8_t valid(uint8_t, uint8_t, uint8_t);
uint8_t solve_opt();
uint8_t find_empty_cell(uint8_t *, uint8_t *);
void makeImplications(uint8_t, uint8_t, uint8_t, implication *);
uint8_t count_elements(uint8_t [9], uint8_t *);
void undoImplications(implication *);

//Variables for sudoku
int backtracks;
int backtracks_opt;

int position;

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
	TCCR0 = (1<<CS02)|(1<<CS00); ; // presc val. 256
	OCR0 = cMaxCnt; // max tim/cnt0 value 150
	TIMSK |= (1<<OCIE0); // enable TIM0_COMP interrupt

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
	
	for (uint8_t r = 0; r < 9; r++)
	{
		for (uint8_t c = 0; c < 9; c++)
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

USART_RXC_vect_RETI:
	SREG = save_sreg;

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
void initUART()
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


/**
 * 
 * Subroutine: transmit()
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This functions transmits a byte, found on the global variable
 * trans_char using polling.
 * 
 */
void transmit()
{
	while ( (UCSRA & 0x20) != 0x20 ); // Wait for empty transmit buffer
	UDR  = transm_char; // Sending character as a response
}


/**
 * 
 * Subroutine: read_cmd();
 * 
 * Input: none
 * 
 * Returns: nothing
 * 
 * Description: Reads a line from the rcv_buff and stores it in cmd_buff
 * 
 */
// void read_cmd()
// {
// 	uint8_t i = 0;
// 	while (1)
// 	{
// 		if (i >= 8 || (cmd_buff[i-1] == 0xD && cmd_buff[i] == 0xA && rcv_buff[rcv_cons] == 0xD))
// 			break;

// 		cmd_buff[i] = rcv_buff[rcv_cons];
// 		i++;
// 		rcv_cons++;
// 	}
// }


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
void process()
{
	// "AT\r\n\r"
	if (rcv_buff[rcv_cons] == 0x41  && rcv_buff[rcv_cons+1] == 0x54 && rcv_buff[rcv_cons+2] == 0x0D && rcv_buff[rcv_cons+3] == 0x0A && rcv_buff[rcv_cons+4] == 0x0D)
	{
		rcv_cons = rcv_cons + 5; // Update rcv consumer.	
		// respond with "OK\CR\LF"
		send_response_OK();
	}
	
	// "C\r\n\r", clears the Sudoku table and notifies PC ("OK\r\n") when it is done.
	else if (rcv_buff[rcv_cons] == 0x43 && rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D)
	{
		clear_table();
		rcv_cons += 4;
		send_response_OK();
	}

	// "N<x><y><val>\r\n\r", stores <val> in sudoku[<x>-1][<y>-1] and notifies PC ("OK\r\n") when it is done.
	else if (rcv_buff[rcv_cons] == 0x4E && rcv_buff[rcv_cons+4] == 0x0D && rcv_buff[rcv_cons+5] == 0x0A &&  rcv_buff[rcv_cons+6] == 0x0D)
	 {
		storeClue(); // stores the clue and partially updates the rcv_cons (by 2 positions).
		rcv_cons += 4; // Update rcv consumer.
		send_response_OK(); // respond with "OK\CR\LF"
	 }
	 
	// "P\r\n\r", solves the Sudoku and notifies PC ("OK\r\n") when it is done.
	else if (rcv_buff[rcv_cons] == 0x50 && rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D)
	{
		play_game(); // solves the Sudoku
		rcv_cons += 4; // Update rcv consumer.
		stopSolved = 0; // reset stopSolved, so that next time that a board is loaded and starts 
		// the solver, it is not stopped (unless explicitly specified).
	}
	
	// "S\r\n\r", checks if the Sudoku is correctly solved.
	else if (rcv_buff[rcv_cons] == 0x53 && rcv_buff[rcv_cons+1] == 0x0D &&	rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D)
	{
		// col_position = 1; // reset the col
		// row_position = 1;
		checkSudoku();
	}

	// "T\r\n\r", sends the next clue in sudoku by returning N<x><y><val>\r\n
	else if (rcv_buff[rcv_cons] == 0x54 && rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D)
	{
		send_table();
		rcv_cons += 4;
		send_response_OK();
		if (col_position == 1 && row_position == 1)
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
	
	// "B\r\n\r", stops solving the Sudoku and stop the transmission of clues.
	// The latter can be translated to "reset the counters that transmit the table"
	// so that when a new T instruction comes, it starts from the beginning.
	else if (rcv_buff[rcv_cons] == 0x42 && rcv_buff[rcv_cons+1] == 0x0D && rcv_buff[rcv_cons+2] == 0x0A && rcv_buff[rcv_cons+3] == 0x0D )
	{
		stopSolved = 1;
		// col_position = 1;
		// row_position = 1;
		rcv_cons += 4;
		send_response_OK();
	}
	
	// "D<x><y>\r\n\r", sends the data in sudoku[<x>-1][<y>-1] by returning N<x><y><val>\r\n
	else if (rcv_buff[rcv_cons] == 0x44 && rcv_buff[rcv_cons+3] == 0x0D && rcv_buff[rcv_cons+4] == 0x0A && rcv_buff[rcv_cons+5] == 0x0D )
	{
		debug_table();
		rcv_cons += 6;
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
void storeClue()
{
	// array indices are from 0-8, but the cmd indices are from 0x31-0x39
	// use of postfix is necessary because rcv_cons++ will return 
	// rcv_buff[rcv_cons] and then increment rcv_cons.
	uint8_t x = (rcv_buff[++rcv_cons] & 0x0F) - 1;
	uint8_t y = (rcv_buff[++rcv_cons] & 0x0F) - 1;
	sudoku[x][y] = (rcv_buff[++rcv_cons] & 0x0F);
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
uint8_t checkSudoku()
{
	uint8_t checksum1[9], checksum2[9] = {1,2,3,4,5,6,7,8,9};
	// uint8_t checksum1[9] = {1,2,3,4,5,6,7,8,9}; uint8_t checksum2[9] = {1,2,3,4,5,6,7,8,9};
	uint8_t i, j, k, c, r = 0; // uint8_t i = 0; uint8_t j = 0;

	uint8_t retv = 0; // if it stays zero it means that its correct

	// check every ROW and COLUMN
	for (i = 0; i <= 8; i++)
	{
		// check
		for (j = 0; j <= 8; j++)
		{
			checksum1[sudoku[j][i]-1] = 0; // checking cols
			checksum2[sudoku[i][j]-1] = 0; // checking rows
		}
		for (k = 0; k <= 8; k++)
		{
			retv |= checksum1[k]; // store to retv
			retv |= checksum2[k]; // store to retv
			checksum1[k] = k+1; // reinitialize checksum
			checksum2[k] = k+1; // reinitialize checksum
		}
	}

	for (c = 0; c <= 2; c++)
	{
		for (r = 0; r <= 2; r++)
		{
			for (j = 3*c; j <= (c+1)*3-1; j++)
			{
				// check
				for (i = 3*r; i <= (r+1)*3-1; i++)
				{
					checksum1[sudoku[i][j]-1] = 0;
				}
			}
			for (k = 0; k <= 8; k++)
			{
				retv |= checksum1[k]; // store to retv
				checksum1[k] = k+1; // reinitialize checksum
			}
		}
	}

	return retv;
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
void clear_table()
{
	for (uint8_t i = 0; i < 9; i++)
	{
		for (uint8_t j = 0; j < 9; j++)
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
 void play_game()
{
	solve_opt();
	backtracks = 0;
	
	if (checkSudoku() == 0)
	{
		send_response_OK();
		transm_char = 0x44;
		transmit();
		transm_char = tx_OK[2];
		transmit();
		transm_char = tx_OK[3];
		transmit();
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
 void send_table()
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
	col_position++;

	if (col_position == 10)
	{
		col_position = 1;
		row_position++;
	}

	if (row_position == 10)
	{
		row_position = 1;
		col_position = 1;
	}
}

/**
 * 
 * Subroutine: debug_table
 * 
 * Input: None
 * 
 * Returns: None
 * 
 * Description: This routine sends the value of a cell x,y to PC.
 * 
 */
void debug_table()
{
	uint8_t x = (rcv_buff[rcv_cons+1] & 0x0F);
	uint8_t y = (rcv_buff[rcv_cons+2] & 0x0F);
	
	transm_char = 0x4E;
	transmit();
	transm_char = 0x30+(rcv_buff[rcv_cons+1] & 0x0F);
	transmit();
	transm_char = 0x30+(rcv_buff[rcv_cons+2] & 0x0F);
	transmit();
	transm_char = 0x30+sudoku[x-1][y-1];
	transmit();
	transm_char = 0x0D;
	transmit();
	transm_char = 0x0A;
	transmit();
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
void send_response_OK()
{
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

uint8_t valid(uint8_t row, uint8_t column, uint8_t guess)
{
	uint8_t corner_x = row / 3 * 3;
	uint8_t corner_y = column / 3 * 3;

	for (uint8_t x = 0; x < 9; ++x)
	{
		if (sudoku[row][x] == guess) return 0;
		if (sudoku[x][column] == guess) return 0;
		if (sudoku[corner_x + (x / 3)][corner_y + (x % 3)] == guess) return 0;
	}

	return 1;
}


uint8_t find_empty_cell(uint8_t *row, uint8_t *column)
{
	for (uint8_t x = 0; x < 9; x++)
	{
		for (uint8_t y = 0; y < 9; y++)
		{
			if (sudoku[x][y] == 0)
			{
				*row = x;
				*column = y;

				return 1;
			}
		}
	}

	return 0;
}


// uint8_t solve()
// {

// 	// if (stopSolved == 1)
// 	// 	return 0;

// 	uint8_t row, column = 255;
// 	uint8_t guess;

// 	if (find_empty_cell(&row, &column) == 0) return 1;

// 	for (guess = 1; guess < 10; guess++)
// 	{
// 		if (valid(row, column, guess))
// 		{
// 			sudoku[row][column] = guess;

// 			if (solve()) return 1;

// 			backtracks++;
// 			sudoku[row][column] = 0;
// 		}
// 	}

// 	return 0;
// }


void makeImplications(uint8_t row, uint8_t col, uint8_t guess, implication* imply)
{
	imply[position].x = row;
	imply[position].y = col;
	imply[position].possible_clues[0] = guess;
	position++;

	sudoku[row][col] = guess;
	uint8_t index = 0;
	
	uint8_t value;

	uint8_t possible_clues[9];
	implication impl[9];

	// Removing clues from possible clues which has already been in the ith sector with clue (row,col)

	for (uint8_t i = 0; i < 9; i++)
	{
		possible_clues[0] = 1;
		possible_clues[1] = 2;
		possible_clues[2] = 3;
		possible_clues[3] = 4;
		possible_clues[4] = 5;
		possible_clues[5] = 6;
		possible_clues[6] = 7;
		possible_clues[7] = 8;
		possible_clues[8] = 9;

		for (uint8_t x = sectors[i][0]; x < sectors[i][1]; x++)
		{
			for (uint8_t y = sectors[i][2]; y < sectors[i][3]; y++)
			{
				if (sudoku[x][y] != 0)
					for (uint8_t m = 0; m < 9; m++)
					{
						if (possible_clues[m] == sudoku[x][y])
							possible_clues[m] = 0;
					}
			}
		}

		// Setting the possible clues for each clue (x,y) in the ith sector
		index = 0;
		for (uint8_t x = sectors[i][0]; x < sectors[i][1]; x++)
		{
			for (uint8_t y = sectors[i][2]; y < sectors[i][3]; y++)
			{
				if (sudoku[x][y] == 0)
				{
					// store the tuple in x,y, elements
					impl[index].x = x;
					impl[index].y = y;
					for(uint8_t m = 0; m < 9; m++)
					{
						impl[index].possible_clues[m] = possible_clues[m];
					}
					index++;
				}
			}

		}

		// For each sector
		for(uint8_t j = 0; j < index; j++)
		{
			// Finding the set of clues on the row corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			
			for(uint8_t y = 0; y < 9; y++)
			{
				for(uint8_t m = 0; m < 9; m++)
				{
					if (impl[j].possible_clues[m] == sudoku[impl[j].x][y])
						impl[j].possible_clues[m] = 0;
				}
			}

			// Finding the set of clues on the column corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			
			for(uint8_t x = 0; x < 9; x++)
			{
				for(uint8_t m = 0;m < 9; m++)
				{
					if (impl[j].possible_clues[m] == sudoku[x][impl[j].y])
						impl[j].possible_clues[m] = 0;
				}
			}

			// Check if in the set of possible values there is only one clue

			if (count_elements(impl[j].possible_clues, &value) == 1)
				if (valid(impl[j].x, impl[j].y, value))
				{
					sudoku[impl[j].x][impl[j].y] = value;
					imply[position].x = impl[j].x;
					imply[position].y = impl[j].y;
					imply[position].possible_clues[0] =value;
					position++;
				}
		}
	}
}

uint8_t count_elements(uint8_t array[9], uint8_t * element)
{
	uint8_t counter = 0;

	for(uint8_t i = 0; i < 9; i++)
	{
		if (array[i] != 0)
		{
			counter++;
			*element = array[i];
		}
	}
	return counter;
}


void undoImplications(implication* impl)
{
	for (uint8_t i = 0; i < 81; i++)
	{
		sudoku[impl[i].x][impl[i].y] = 0;
	}
}

uint8_t solve_opt() 
{
	uint8_t row;
	uint8_t column;

	implication* impl = malloc(440);

	if(find_empty_cell(&row, &column) == 0) return 1;

	for (uint8_t guess = 1; guess < 10; guess++)
	{
		if (valid(row, column, guess) == 1)
		{
			if (impl != NULL)
			{
				makeImplications(row, column, guess, impl);
				if (solve_opt()) return 1;
				backtracks_opt++;
				undoImplications(impl);
			} else {
				sudoku[row][column] = guess;				
				
				if (solve_opt()) return 1;

				backtracks_opt++;
				sudoku[row][column] = 0;
			}
		}
	}

	if (impl != NULL) free(impl);
	
	return 0;
	
}
