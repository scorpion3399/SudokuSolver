
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define mytype_t u_int8_t

mytype_t valid(mytype_t[][9], mytype_t, mytype_t, mytype_t);
mytype_t solve(mytype_t[][9]);
mytype_t find_empty_cell(mytype_t[][9], mytype_t *, mytype_t *);

mytype_t puzzle[9][9] = {
	{1, 7, 4, 0, 9, 0, 6, 0, 0},
	{0, 0, 0, 0, 3, 8, 1, 5, 7},
	{5, 3, 0, 7, 0, 1, 0, 0, 4},
	{0, 0, 7, 3, 4, 9, 8, 0, 0},
	{8, 4, 0, 5, 0, 0, 3, 6, 0},
	{3, 0, 5, 0, 0, 6, 4, 7, 0},
	{2, 8, 6, 9, 0, 0, 0, 0, 1},
	{0, 0, 0, 6, 2, 7, 0, 3, 8},
	{0, 5, 3, 0, 8, 0, 0, 9, 6}
};

int main()
{
	mytype_t row = 0;
	mytype_t column = 0;

	if (solve(puzzle)) {
		printf("\n+-----+-----+-----+\n");
		for (mytype_t x = 0; x < 9; ++x) {
			for (mytype_t y = 0; y < 9; ++y) printf("|%d", puzzle[x][y]);
			printf("|\n");
			if (x % 3 == 2) printf("+-----+-----+-----+\n");
		}
	}
	else {
		printf("\n\nNO SOLUTION FOUND\n\n");
	}

	return 0;
}

mytype_t valid(mytype_t puzzle[][9], mytype_t row, mytype_t column, mytype_t guess) {
	mytype_t corner_x = row / 3 * 3;
	mytype_t corner_y = column / 3 * 3;

	for (mytype_t x = 0; x < 9; ++x)
	{
		if (puzzle[row][x] == guess) return 0;
		if (puzzle[x][column] == guess) return 0;
		if (puzzle[corner_x + (x % 3)][corner_y + (x / 3)] == guess) return 0;
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

mytype_t solve(mytype_t puzzle[][9]) {
	mytype_t row;
	mytype_t column;

	if(!find_empty_cell(puzzle, &row, &column)) return 1;

	for (mytype_t guess = 1; guess < 10; guess++) {
		if (valid(puzzle, row, column, guess)) {
			puzzle[row][column] = guess;

			if(solve(puzzle)) return 1;
			puzzle[row][column] = 0;
		}
	}
	return 0;
}