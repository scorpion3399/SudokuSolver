
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define mytype_t u_int8_t

typedef struct implications{

	mytype_t x;
	mytype_t y;
	mytype_t possible_clues[9];
}implication;

mytype_t valid(mytype_t[][9], mytype_t, mytype_t, mytype_t);
mytype_t solve(mytype_t[][9]);
mytype_t find_empty_cell(mytype_t[][9], mytype_t *, mytype_t *);
void makeImplications(mytype_t puzzle[][9],mytype_t row,mytype_t col,mytype_t guess,implication* imply);
void remove_element(mytype_t array[9],mytype_t element);
mytype_t find_element(mytype_t array[9],mytype_t element);
mytype_t count_elements(mytype_t array[9],mytype_t* element);
void undoImplications(mytype_t puzzle[][9],implication* impl);
void array_copy(mytype_t array_dst[9],mytype_t array_src[9]);


 int position = 0;

mytype_t sectors[9][4] = { 
	{0, 3, 0, 3},{3, 6, 0, 3}, {6, 9, 0, 3},
    {0, 3, 3, 6}, {3, 6, 3, 6}, {6, 9, 3, 6},
    {0, 3, 6, 9}, {3, 6, 6, 9}, {6, 9, 6, 9} 
};

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

void makeImplications(mytype_t puzzle[][9],mytype_t row,mytype_t col,mytype_t guess,implication* imply){

imply[position].x = row;
imply[position].y = col;
imply[position].possible_clues[0] = guess;
position++;

puzzle[row][col] = guess;
mytype_t length = 9;
mytype_t index = 0;
	
mytype_t value;

mytype_t possible_clues[9];
implication impl[9];

// Removing clues from possible clues which has already been in the ith sector with clue (row,col)

for(mytype_t i = 0;i < length;i++){

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
						possible_clues[m] = -1;
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
	
	for(mytype_t j = 0; j < length; j++){

 // Finding the set of clues on the row corresponding to j clue in  ith sector
// and removing them from the set of possible clues of implication
		
		for(mytype_t y = 0; y < 9;y++){

		//	if(find_element(impl[j].possible_clues,puzzle[impl[j].x][y]))
	   //	remove_element(impl[j].possible_clues,puzzle[impl[j].x][y]);

			for(mytype_t m = 0;m < 9;m++){

				if(impl[j].possible_clues[m] == puzzle[impl[j].x][y])
					impl[j].possible_clues[m] = -1;
			}



		}

 // Finding the set of clues on the column corresponding to j clue in  ith sector
// and removing them from the set of possible clues of implication
		
		for(mytype_t x = 0; x < 9;x++){

			//if(find_element(impl[j].possible_clues,puzzle[x][impl[j].y]))
			//remove_element(impl[j].possible_clues,puzzle[x][impl[j].y]);

			for(mytype_t m = 0;m < 9;m++){

				if(impl[j].possible_clues[m] == puzzle[x][impl[j].y])
					impl[j].possible_clues[m] = -1;
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

void remove_element(mytype_t array[9],mytype_t element){

	for(mytype_t i = 0;i < 9;i++){

		if(array[i] == element)
			array[i] = 0;
	}

}


mytype_t find_element(mytype_t array[9],mytype_t element){

	for(mytype_t i = 0;i < 9;i++){

		if(array[i] == element)
			return 1;
	}

	return 0;

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

void array_copy(mytype_t array_dst[9],mytype_t array_src[9]){

	for(mytype_t i = 0;i < 9;i++){
		if(array_src[i] != 0)
			array_dst[i] = array_src[i];
	}

}


void undoImplications(mytype_t puzzle[][9],implication* impl){

	for(mytype_t i = 0;i < 81;i++){

		puzzle[impl[i].x][impl[i].y] = 0;

	}


}


mytype_t solve(mytype_t puzzle[][9]) {
	mytype_t row;
	mytype_t column;

	implication* impl = malloc(700);

	if(!find_empty_cell(puzzle, &row, &column)) return 1;

	for (mytype_t guess = 1; guess < 10; guess++) {
		if (valid(puzzle, row, column, guess)) {

			//puzzle[row][column] = guess;

			 makeImplications(puzzle,row,column,guess,impl);

			if(solve(puzzle)) return 1;
			undoImplications(puzzle,impl);
			//puzzle[row][column] = 0;
		}
	}
	return 0;
}





