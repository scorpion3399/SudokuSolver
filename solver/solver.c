#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

typedef struct benchmark {
	int solved; // 1 if solved
	double time; // time needed for solving
	int backtracks; // backtracks
} bench;

typedef struct implications {
	int x;
	int y;
	int possible_clues[9];
} implication;

int valid(int [9][9], int, int, int);
int find_empty_cell(int [9][9], int *, int *);
int solve(int [9][9]);
void makeImplications(int [9][9], int, int, int, implication *);
void undoImplications(int [9][9], implication *);
int count_elements(int [9], int *);
// void remove_element(int [9], int);
// int find_element(int [9], int);
// void array_copy(int [9], int [9]);
int solve_opt(int [9][9]);
void print_grid(int [9][9]);
int checkSudoku(int [9][9]);

// to test the sudoku solving algorithms
bench test(int [9][9], int);

// the puzzles
int easy[9][9];
int easy2[9][9];
int inter[9][9];
int inter2[9][9];
int diff[9][9];
int hard[9][9];
int easy_opt[9][9];
int easy2_opt[9][9];
int inter_opt[9][9];
int inter2_opt[9][9];
int diff_opt[9][9];
int hard_opt[9][9];

// to count the backtracks, a way to define the peformance of the algorithm
int backtracks, backtracks_opt;

int position;

int allocs, frees; // to count the number of mallocs and frees

int sectors[9][4] = {
	{0, 3, 0, 3}, {3, 6, 0, 3}, {6, 9, 0, 3},
	{0, 3, 3, 6}, {3, 6, 3, 6}, {6, 9, 3, 6},
	{0, 3, 6, 9}, {3, 6, 6, 9}, {6, 9, 6, 9} 
};





int main()
{
	bench res[6];
	bench res_opt[6];

	res[0] = test(easy, 0);
	res[1] = test(easy2, 0);
	res[2] = test(inter, 0);
	res[3] = test(inter2, 0);
	res[4] = test(diff, 0);
	res[5] = test(hard, 0);

	res_opt[0] = test(easy_opt, 1);
	res_opt[1] = test(easy2_opt, 1);
	res_opt[2] = test(inter_opt, 1);
	res_opt[3] = test(inter2_opt, 1);
	res_opt[4] = test(diff_opt, 1);
	res_opt[5] = test(hard_opt, 1);

	printf("easy\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[0].solved, res[0].time, res[0].backtracks);
	
	printf("easy_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[0].solved, res_opt[0].time, res_opt[0].backtracks);


	printf("easy2\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[1].solved, res[1].time, res[1].backtracks);
	
	printf("easy2_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[1].solved, res_opt[1].time, res_opt[1].backtracks);


	printf("inter\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[2].solved, res[2].time, res[2].backtracks);
	
	printf("inter_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[2].solved, res_opt[2].time, res_opt[2].backtracks);


	printf("inter2\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[3].solved, res[3].time, res[3].backtracks);
	
	printf("inter2_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[3].solved, res_opt[3].time, res_opt[3].backtracks);


	printf("diff\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[4].solved, res[4].time, res[4].backtracks);
	
	printf("diff_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[4].solved, res_opt[4].time, res_opt[4].backtracks);


	printf("hard\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res[5].solved, res[5].time, res[5].backtracks);
	
	printf("hard_opt\n");
	printf("Solved = %d\tTime = %f secs\tBacktracks = %d\n", res_opt[5].solved, res_opt[5].time, res_opt[5].backtracks);


	return 0;
}



bench test(int puzzle[9][9], int opt)
{
	clock_t t;
	double time_taken;
	int retv = 0;
	bench b;
	
	// sanity check
	retv = checkSudoku(puzzle);
	if (retv == 0)
		printf("Must not be solved here.\n");

	// Calculate the time taken by solve()
	if (opt == 1)
	{
		t = clock();
		solve_opt(puzzle);
		t = clock() - t;
		time_taken = ((double) t) / CLOCKS_PER_SEC; // in seconds
		b.backtracks = backtracks_opt;
		backtracks_opt = 0;
	} else {
		t = clock();
		solve(puzzle);
		t = clock() - t;
		time_taken = ((double) t) / CLOCKS_PER_SEC; // in seconds
		b.backtracks = backtracks;
		backtracks = 0;
	}

	retv = 0;
	retv = checkSudoku(puzzle);

	b.time = time_taken;

	if (retv == 0)
	{
		b.solved = 1;
		// printf("solved correcly\n");
	}
	else
		b.solved = 0;

	retv = 0;

	return b;
}


void error_and_exit()
{
	printf("error: memory exhausted\n");
	exit(EXIT_FAILURE);
}


void *xmalloc(size_t size)
{
	void* result = malloc(size);
	int addr = (long) result;

	if (result == NULL)
	{
		error_and_exit();
	}
	
	printf("%d.\tAllocating %lu bytes at 0x%x.\n", ++allocs, size, addr);

	return result;
}

void xfree(void *ptr)
{
	int addr = (long) ptr;
	if (ptr != NULL)
	{
		free(ptr);
		printf("%d.\tFreeing bytes at 0x%x\n", ++frees, addr);
	} else {
		fprintf(stderr, "error: Invalid address.\n");
		exit(EXIT_FAILURE);
	}
}


int solve(int puzzle[9][9])
{
	int row, column;

	if (find_empty_cell(puzzle, &row, &column) == 0)
		return 1;

	for (int guess = 1; guess < 10; guess++)
	{
		if (valid(puzzle, row, column, guess) == 1)
		{
			puzzle[row][column] = guess;

			if (solve(puzzle) == 1)
				return 1;
			
			backtracks++;
			puzzle[row][column] = 0;
		}
	}

	return 0;
}


int solve_opt(int puzzle[9][9])
{
	int row, column;

	implication* impl = malloc(10000);

	if (impl == NULL)
	{
		error_and_exit();
	}


	if (find_empty_cell(puzzle, &row, &column) == 0)
		return 1;

	for (int guess = 1; guess < 10; guess++)
	{
		if (valid(puzzle, row, column, guess))
		{
			makeImplications(puzzle, row, column, guess, impl);
			// printf("SHIT\n");
			if (solve_opt(puzzle))
				return 1;

			backtracks_opt++;
			undoImplications(puzzle, impl);
		}
	}

	
	if (impl != NULL)
		free(impl);

	// position = 0;

	return 0;
}


int valid(int puzzle[9][9], int row, int column, int guess)
{
	int corner_x = row / 3 * 3;
	int corner_y = column / 3 * 3;

	for (int x = 0; x < 9; x++)
	{
		if (puzzle[row][x] == guess) return 0;
		if (puzzle[x][column] == guess) return 0;
		// The '/' and '%' are interchangeable. Either way it will iterate on
		// the Sudoku box.
		// [x/3][x%3] : (0,0),(0,1),(0,2),(1,0),...,(2,2)
		// [x%3][x/3] : (0,0),(1,0),(2,0),(0,1),...,(2,2)
		if (puzzle[corner_x + (x / 3)][corner_y + (x % 3)] == guess) return 0;
	}

	return 1;
}


int find_empty_cell(int puzzle[9][9], int *row, int *column)
{
	for (int x = 0; x < 9; x++)
	{
		for (int y = 0; y < 9; y++)
		{
			if (puzzle[x][y] == 0)
			{
				*row = x;
				*column = y;

				return 1;
			}
		}
	}

	return 0;
}


void makeImplications(int puzzle[9][9], int row, int col, int guess, implication * imply)
{
	int index = 0;
	int value;
	int possible_clues[9];
	implication impl[9];

	imply[position].x = row;
	imply[position].y = col;
	imply[position].possible_clues[0] = guess;
	position++;

	puzzle[row][col] = guess;

	// Removing clues from possible clues which has already been in the ith sector with clue (row,col)
	for (int i = 0; i < 9; i++)
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

		for (int x = sectors[i][0]; x < sectors[i][1]; x++)
		{
			for (int y = sectors[i][2]; y < sectors[i][3]; y++)
			{
				if (puzzle[x][y] != 0)
				{
					// Removing element puzzle[x][y] from possibles_clues
					for (int m = 0; m < 9; m++)
					{
						if (possible_clues[m] == puzzle[x][y])
							possible_clues[m] = 0;
					}
				}
			}
		}

		// Setting the possible clues for each clue (x,y) in the ith sector
		index = 0;
		for (int x = sectors[i][0]; x < sectors[i][1]; x++)
		{
			for (int y = sectors[i][2]; y < sectors[i][3]; y++)
			{
				if(puzzle[x][y] == 0)
				{
					// store the tuple in x,y, elements
					impl[index].x = x;
					impl[index].y = y;

					for (int m = 0; m < 9; m++)
						impl[index].possible_clues[m] = possible_clues[m];

					index++;
				}
			}
		}

		// For each sector 
		for (int j = 0; j < index; j++)
		{
			// Finding the set of clues on the row corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			for (int y = 0; y < 9; y++)
			{
				for (int m = 0; m < 9; m++)
				{
					if (impl[j].possible_clues[m] == puzzle[impl[j].x][y])
						impl[j].possible_clues[m] = 0;
				}
			}

			// Finding the set of clues on the column corresponding to j clue in  ith sector
			// and removing them from the set of possible clues of implication
			for (int x = 0; x < 9; x++)
			{
				for (int m = 0; m < 9; m++)
				{
					if (impl[j].possible_clues[m] == puzzle[x][impl[j].y])
						impl[j].possible_clues[m] = 0;
				}
			}

			// Check if in the set of possible values there is only one clue
			if (count_elements(impl[j].possible_clues, &value) == 1)
			{
				if (valid(puzzle, impl[j].x, impl[j].y, value))
				{
					puzzle[impl[j].x][impl[j].y] = value;
					imply[position].x = impl[j].x;
					imply[position].y = impl[j].y;
					imply[position].possible_clues[0] = value;
					position++;
				}
			}
		}
	}
}


int count_elements(int array[9], int* element)
{
	int counter = 0;

	for (int i = 0; i < 9; i++)
	{
		if (array[i] != 0)
		{
			counter++;
			*element = array[i];
		}
	}

	return counter;
}


void undoImplications(int puzzle[9][9], implication* impl)
{
	for(int i = 0; i < 81; i++)
	{
		puzzle[impl[i].x][impl[i].y] = 0;
	}
}


// void remove_element(int array[9], int element)
// {
// 	for(int i = 0; i < 9; i++)
// 	{
// 		if(array[i] == element)
// 			array[i] = 0;
// 	}
// }


// int find_element(int array[9], int element)
// {
// 	for(int i = 0; i < 9; i++)
// 	{
// 		if(array[i] == element)
// 			return 1;
// 	}

// 	return 0;
// }


// void array_copy(int array_dst[9], int array_src[9])
// {
// 	for (int i = 0; i < 9; i++)
// 	{
// 		if (array_src[i] != 0)
// 			array_dst[i] = array_src[i];
// 	}
// }


void print_grid(int A[9][9])
{
	printf("\n+-------+-------+-------+\n");
	for (int row = 0; row < 9; row++)
	{
		for (int col = 0; col < 9; col++)
		{
			if (col == 0) printf("| %d", A[row][col]);
			else if (col == 2 || col == 5) printf(" %d |", A[row][col]);
			else if (col == 8) printf(" %d |\n", A[row][col]);
			else printf(" %d", A[row][col]);
		}
		
		if (row % 3 == 2)
			printf("+-------+-------+-------+\n");
	}
}



int checkSudoku(int sudoku[9][9])
{
	int checksum1[9], checksum2[9] = {1,2,3,4,5,6,7,8,9};
	// int checksum1[9] = {1,2,3,4,5,6,7,8,9}; int checksum2[9] = {1,2,3,4,5,6,7,8,9};
	int i, j, k, c, r = 0; // int i = 0; int j = 0;

	int retv = 0; // if it stays zero it means that its correct

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




int easy[9][9] = {
	{1,7,4,0,9,0,6,0,0},
	{0,0,0,0,3,8,1,5,7},
	{5,3,0,7,0,1,0,0,4},
	{0,0,7,3,4,9,8,0,0},
	{8,4,0,5,0,0,3,6,0},
	{3,0,5,0,0,6,4,7,0},
	{2,8,6,9,0,0,0,0,1},
	{0,0,0,6,2,7,0,3,8},
	{0,5,3,0,8,0,0,9,6}
};

int easy2[9][9] = {
	{1,0,5,7,0,2,6,3,8},
	{2,0,0,0,0,6,0,0,5},
	{0,6,3,8,4,0,2,1,0},
	{0,5,9,2,0,1,3,8,0},
	{0,0,2,0,5,8,0,0,9},
	{7,1,0,0,3,0,5,0,2},
	{0,0,4,5,6,0,7,2,0},
	{5,0,0,0,0,4,0,6,3},
	{3,2,6,1,0,7,0,0,4}
};

int inter[9][9] = {
	{5,1,7,6,0,0,0,3,4},
	{2,8,9,0,0,4,0,0,0},
	{3,4,6,2,0,5,0,9,0},
	{6,0,2,0,0,0,0,1,0},
	{0,3,8,0,0,6,0,4,7},
	{0,0,0,0,0,0,0,0,0},
	{0,9,0,0,0,0,0,7,8},
	{7,0,3,4,0,0,5,6,0},
	{0,0,0,0,0,0,0,0,0}
};

int inter2[9][9] = {
	{5,1,7,6,0,0,0,3,4},
	{0,8,9,0,0,4,0,0,0},
	{3,0,6,2,0,5,0,9,0},
	{6,0,0,0,0,0,0,1,0},
	{0,3,0,0,0,6,0,4,7},
	{0,0,0,0,0,0,0,0,0},
	{0,9,0,0,0,0,0,7,8},
	{7,0,3,4,0,0,5,6,0},
	{0,0,0,0,0,0,0,0,0}
};

int diff[9][9] = {
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

int hard[9][9] = {
	{8,5,0,0,0,2,4,0,0},
	{7,2,0,0,0,0,0,0,9},
	{0,0,4,0,0,0,0,0,0},
	{0,0,0,1,0,7,0,0,2},
	{3,0,5,0,0,0,9,0,0},
	{0,4,0,0,0,0,0,0,0},
	{0,0,0,0,8,0,0,7,0},
	{0,1,7,0,0,0,0,0,0},
	{0,0,0,0,3,6,0,4,0}
};

int easy_opt[9][9] = {
	{1,7,4,0,9,0,6,0,0},
	{0,0,0,0,3,8,1,5,7},
	{5,3,0,7,0,1,0,0,4},
	{0,0,7,3,4,9,8,0,0},
	{8,4,0,5,0,0,3,6,0},
	{3,0,5,0,0,6,4,7,0},
	{2,8,6,9,0,0,0,0,1},
	{0,0,0,6,2,7,0,3,8},
	{0,5,3,0,8,0,0,9,6}
};

int easy2_opt[9][9] = {
	{1,0,5,7,0,2,6,3,8},
	{2,0,0,0,0,6,0,0,5},
	{0,6,3,8,4,0,2,1,0},
	{0,5,9,2,0,1,3,8,0},
	{0,0,2,0,5,8,0,0,9},
	{7,1,0,0,3,0,5,0,2},
	{0,0,4,5,6,0,7,2,0},
	{5,0,0,0,0,4,0,6,3},
	{3,2,6,1,0,7,0,0,4}
};

int inter_opt[9][9] = {
	{5,1,7,6,0,0,0,3,4},
	{2,8,9,0,0,4,0,0,0},
	{3,4,6,2,0,5,0,9,0},
	{6,0,2,0,0,0,0,1,0},
	{0,3,8,0,0,6,0,4,7},
	{0,0,0,0,0,0,0,0,0},
	{0,9,0,0,0,0,0,7,8},
	{7,0,3,4,0,0,5,6,0},
	{0,0,0,0,0,0,0,0,0}
};

int inter2_opt[9][9] = {
	{5,1,7,6,0,0,0,3,4},
	{0,8,9,0,0,4,0,0,0},
	{3,0,6,2,0,5,0,9,0},
	{6,0,0,0,0,0,0,1,0},
	{0,3,0,0,0,6,0,4,7},
	{0,0,0,0,0,0,0,0,0},
	{0,9,0,0,0,0,0,7,8},
	{7,0,3,4,0,0,5,6,0},
	{0,0,0,0,0,0,0,0,0}
};

int hard_opt[9][9] = {
	{8,5,0,0,0,2,4,0,0},
	{7,2,0,0,0,0,0,0,9},
	{0,0,4,0,0,0,0,0,0},
	{0,0,0,1,0,7,0,0,2},
	{3,0,5,0,0,0,9,0,0},
	{0,4,0,0,0,0,0,0,0},
	{0,0,0,0,8,0,0,7,0},
	{0,1,7,0,0,0,0,0,0},
	{0,0,0,0,3,6,0,4,0}
};

int diff_opt[9][9] = {
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

