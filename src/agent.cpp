/*********************************************
 *  agent.cpp
 *  UNSW Session 1, 2015
 *  Gabriel Low & Yilser Kebabran
*/

#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "pipe.h"

#define map_size 80
#define world_size map_size * 2 - 1

int   pipe_fd;
FILE* in_stream;
FILE* out_stream;

char view[5][5];

class World {
	int posX, posY; // cartesian coordinates
	int direction; // 0 = north, 1 = east, 2 = south, 3 = west
	int seenXMin, seenXMax, seenYMin, seenYMax; // rectangular bounds of visible area in cartesian coordinates
public:
	char map[world_size][world_size];
	
	World();
	void updateMap(char view[5][5]);
	void move(char command);
	void print();
	
	int getPositionX() { return posX; }
	int getPositionY() { return posY; }
	int getVisibleWidth() { return seenXMax - seenXMin; }
	int getVisibleHeight() { return seenYMax - seenYMin; }
};

World::World() {
	posX = 0; // starts at (0, 0)
	posY = 0;
	direction = 2; // starts facing south
	
	seenXMin = 0;
	seenXMax = 0;
	seenYMin = 0;
	seenYMax = 0;
	
	for (int i = 0; i < world_size; ++i) {
		for (int j = 0; j < world_size; ++j) {
			map[i][j] = '?';
		}
	}
}

void World::updateMap(char view[5][5]) {
	// Update seen area
	seenXMin = posX - 2 < seenXMin ? posX - 2 : seenXMin;
	seenXMax = posX + 2 > seenXMax ? posX + 2 : seenXMax;
	seenYMin = posY - 2 < seenYMin ? posY - 2 : seenYMin;
	seenYMax = posY + 2 > seenYMax ? posY + 2 : seenYMax;
	
	// Offset for array
	int x = posX + 78;
	int y = posY + 82;
	
	// Update map
	for (int i = 0; i < 5; ++i) {
		for (int j = 0; j < 5; ++j) {
			map[x + j][y - i] = view[i][j];
		}
	}
	
	// world map ignores player
	// TODO: needs account for boat
	map[posX + 80][posY + 80] = ' ';
}

void World::move(char command) {
	if (command == 'F' || command == 'f') { // Step forward
		if (direction == 0) {
			++posY;
		} else if (direction == 1) {
			++posX;
		} else if (direction == 2) {
			--posY;
		} else {
			--posX;
		}
	} else if (command == 'L' || command == 'l') { // Turn left
		direction = (direction + 3) % 4;
	} else if (command == 'R' || command == 'r') { // Turn right
		direction = (direction + 1) % 4;
	} else if (command == 'C' || command == 'c') { // Chop
		// TODO
	} else if (command == 'B' || command == 'b') { // BOOOOOOOOOOM!
		// TODO
	} else {
		printf("Y U DO DIS?");
	}
}

void World::print() {
	int arrayXMin = seenXMin + 80;
	int arrayXMax = seenXMax + 80;
	int arrayYMin = seenYMin + 80;
	int arrayYMax = seenYMax + 80;
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	for (int j = arrayYMax; j >= arrayYMin; --j) {
		putchar('?');
		for (int i = arrayXMin; i <= arrayXMax; ++i) {
			putchar(map[i][j]);
		}
		printf("?\n");
	}
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	printf("\n");
}

char world[world_size][world_size];
char lastMove;
int posX;
int posY;
int heading;

void transpose(char (&tran)[5][5]) {
	int i, j;
	char swap;
	for (i = 1; i < 5; ++i) {
		for (j = 0; j < i; ++j) {
			swap = tran[i][j];
			tran[i][j] = tran[j][i];
			tran[j][i] = swap;
		}
	}
}

void reverseRows(char (&rev)[5][5]) {
	int i, start, end;
	char swap;
	for (i = 0; i < 5; ++i) {
		start = 0;
		end = 4;
		while (start < end) {
			swap = rev[i][start];
			rev[i][start] = rev[i][end];
			rev[i][end] = swap;
			++start;
			--end;
		}
	}
}

void reverseCols(char (&rev)[5][5]) {
	int i, start, end;
	char swap;
	for (i = 0; i < 5; ++i) {
		start = 0;
		end = 4;
		while (start < end) {
			swap = rev[start][i];
			rev[start][i] = rev[end][i];
			rev[end][i] = swap;
			++start;
			--end;
		}
	}
}

void rotateCW(char (&rot)[5][5]) {
	transpose(rot);
	reverseRows(rot);
}

void rotateCCW(char (&rot)[5][5]) {
	transpose(rot);
	reverseCols(rot);
}

void rotate180(char (&rot)[5][5]) {
	reverseCols(rot);
	reverseRows(rot);
}

void print_view(char (&dis)[5][5]) {
	int i, j;
	
	printf("\n+-----+\n");
	for (i = 0; i < 5; i++) {
		putchar('|');
		for (j = 0; j < 5; j++) {
			putchar(dis[i][j]);
		}
		printf("|\n");
	}
	printf("+-----+\n");
}

void print_map() {
	int i, j;
	
	for (i = 0; i < world_size; i++) {
		putchar('|');
		for (j = 0; j < world_size; j++) {
			if ((i == posX) && (j == posY)) {
				putchar('X');
			} else {
				putchar(world[i][j]);
			}
		}
		printf("|\n");
	}
}

char getAction(char view[5][5], World &world) {
	char copy[5][5];
	memcpy(copy, view, sizeof (char) * 5 * 5);
	switch (heading) {
		case 1:
			rotateCW(copy);
			copy[2][2] = '>';
			break;
		case 2:
			rotate180(copy);
			copy[2][2] = 'v';
			break;
		case 3:
			rotateCCW(copy);
			copy[2][2] = '<';
			break;
		default:
			copy[2][2] = '^';
			break;
	}
	
	print_view(copy);
	world.updateMap(copy);
	world.print();
	printf("%d %d", world.getPositionX(), world.getPositionY());
	
	if (view[1][2] == '~' || view[1][2] == '*'|| view[1][2] == 'T') {
		lastMove = 'r';
		heading = (heading + 1)%4;
	} else {
		lastMove = 'f';
	}
	
	getchar();
	world.move(lastMove);
	return lastMove;
}

int main(int argc, char *argv[]) {
	char action;
	int sd;
	int ch;
	int i, j;
	lastMove = '?';
	World w1 = World();
	posX = map_size;
	posY = map_size;
	heading = 2;
	
	if (argc < 3) {
		printf("Usage: %s -p port\n", argv[0] );
		exit(1);
	}
	
	// open socket to Game Engine
	sd = tcpopen(atoi(argv[2]));
	
	pipe_fd    = sd;
	in_stream  = fdopen(sd,"r");
	out_stream = fdopen(sd,"w");
	
	for (i = 0; i < world_size; ++i) {
		for (j = 0; j < world_size; ++j) {
			world[i][j] = '?';
		}
	}
	
	while (1) {
		// scan 5-by-5 wintow around current location
		for (i = 0; i < 5; ++i) {
			for (j = 0; j < 5; ++j) {
				if ((i != 2) || (j != 2)) {
					ch = getc( in_stream );
					if (ch == -1) {
						exit(1);
					}
					view[i][j] = ch;
				}
			}
		}
		
		action = getAction(view, w1);
		putc(action, out_stream);
		fflush(out_stream);
	}
	
	return 0;
}
