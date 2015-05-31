/*********************************************
 *  agent.cpp
 *  UNSW Session 1, 2015
 *  Gabriel Low & Yilser Kabaran
*/

#include <string.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <algorithm>
#include <queue>
#include <vector>

#include "pipe.h"

#define map_size 80
#define world_size map_size * 2 - 1 + 3
#define world_center map_size + 2

int   pipe_fd;
FILE* in_stream;
FILE* out_stream;

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

class Inventory{
	bool axe, gold;
	int kaboomCount;
public:
	Inventory();
	bool getAxe() const { return axe; }
	bool getGold() const { return gold; }
	int getKaboomCount() const { return kaboomCount; }
	void setAxe(bool axe) { this->axe = axe; }
	void setGold(bool gold) { this->gold = gold; }
	void addKaboom() { ++kaboomCount; }
	void useKaboom() { --kaboomCount; }
};

Inventory::Inventory(){
	axe = false;
	gold = false;
	kaboomCount = 0;
}

class World {
	Inventory inventory;
	int posX, posY; // cartesian coordinates
	int direction; // 0 = north, 1 = east, 2 = south, 3 = west
	int bombX, bombY;
	char map[world_size][world_size];
	char access[world_size][world_size]; // -1 = cannot access, x > 0 = can access with x # of bombs
	bool boat;
	int seenXMin, seenXMax, seenYMin, seenYMax; // rectangular bounds of visible area in cartesian coordinates
	int aStarDestX, aStarDestY; // last aStar destination
	std::vector<char> aStarCache; // for caching the shortest path to a given coordinate
public:
	static const int forwardX[4];
	static const int forwardY[4];
	static bool canWalk(char tile) { return (tile == ' ' || tile == 'a' || tile == 'd' || tile == 'B' || tile == 'g'); }
	
	World();
	void updateMap(char (&view)[5][5]);
	void evalAccess();
	void move(char command);
	char aStar(int destX, int destY, bool kaboom = false);
	char explore();
	char findInterest();
	char bomb();
	int bombVal(int i, int j);
	char findTile(char target);
	void print() const;
	
	char getFront() const { return map[posX + forwardX[direction] + world_center][posY + forwardY[direction] + world_center]; }
	void clearFront() { map[posX + forwardX[direction] + world_center][posY + forwardY[direction] + world_center] = ' '; }
	char getInDirection(int angle) const { return map[posX + forwardX[(direction + angle) % 4] + world_center][posY + forwardY[(direction + angle) % 4] + world_center]; }
		
	int getPositionX() const { return posX; }
	int getPositionY() const { return posY; }
	int getVisibleWidth() const { return seenXMax - seenXMin; }
	int getVisibleHeight() const { return seenYMax - seenYMin; }
	
	char getMap(int x, int y) const { return map[x + world_center][y + world_center]; }
	bool canAccess(int x, int y, int kabooms) const { return access[x + world_center][y + world_center] != -1 && access[x + world_center][y + world_center] <= kabooms; }
	int getAccess(int x, int y) const { return access[x + world_center][y + world_center]; }
	bool isExplored(int x, int y) const {
		for (int i = x + world_center - 2; i <= x + world_center + 2; ++i) {
			for (int j = y + world_center - 2; j <= y + world_center + 2; ++j) {
				if (map[i][j] == '?') {
					return false;
				}
			}
		}
		return true;
	}
	
	void setBoat(bool boat) { this->boat = boat; }
	
	bool hasGold() const { return inventory.getGold(); }
	bool hasAxe() const { return inventory.getAxe(); }
	bool onBoat() const { return boat; }
};

const int World::forwardX[4] = {0, 1, 0, -1};
const int World::forwardY[4] = {1, 0, -1, 0};

World::World() {
	inventory = Inventory();
	posX = 0; // starts at (0, 0)
	posY = 0;
	direction = 0; // starts facing north
	bombX = 9001;
	
	boat = false;
	
	seenXMin = 0;
	seenXMax = 0;
	seenYMin = 0;
	seenYMax = 0;
	
	for (int i = 0; i < world_size; ++i) {
		for (int j = 0; j < world_size; ++j) {
			map[i][j] = '?';
			access[i][j] = -1;
		}
	}
}

void World::updateMap(char (&view)[5][5]) {
	// Update seen area
	seenXMin = posX - 2 < seenXMin ? posX - 2 : seenXMin;
	seenXMax = posX + 2 > seenXMax ? posX + 2 : seenXMax;
	seenYMin = posY - 2 < seenYMin ? posY - 2 : seenYMin;
	seenYMax = posY + 2 > seenYMax ? posY + 2 : seenYMax;
	
	// Offset for array
	int x = posX + world_center - 2;
	int y = posY + world_center + 2;
	
	// Rotate view to correct orientation
	if (direction == 1) {
		rotateCW(view);
	} else if (direction == 2) {
		rotate180(view);
	} else if (direction == 3) {
		rotateCCW(view);
	}
	
	// Update map
	// view (matrix coordinates) top left to bottom right = (0, 0), (0, 1) ... (4, 3), (4, 4)
	// map (cartesian coordinates) top left to bottom right = (-2, 2), (-1, 2) ... (1, -2), (2, -2)
	bool recheck = false;
	for (int i = 0; i < 5; ++i) {
		for (int j = 0; j < 5; ++j) {
			if (!(i == 2 && j == 2) && map[x + j][y - i] != view[i][j]) {
				recheck = true;
				map[x + j][y - i] = view[i][j];
			}
		}
	}
	
	// world map ignores player
	if (onBoat()) {
		map[posX + world_center][posY + world_center] = 'B';
	} else {
		if (map[posX + world_center][posY + world_center] != ' ') recheck = true;
		map[posX + world_center][posY + world_center] = ' ';
	}
	
	printf("update\n");
	if (recheck) evalAccess();
}

struct Coord {
	int x, y;
	char kabooms;

	Coord(int x, int y, int kabooms = 0) {
		this->x = x;
		this->y = y;
		this->kabooms = kabooms;
	}
	
	bool operator==(const Coord &other) const {
		return (x == other.x && y == other.y);
	}
	
	bool operator<(const Coord &other) const {
		return kabooms < other.kabooms;
	}
	
	bool operator>(const Coord &other) const {
		return kabooms > other.kabooms;
	}
};

void World::evalAccess() {
	std::vector<Coord> closed;
	std::priority_queue<Coord, std::vector<Coord>, std::greater<Coord> > open;
	
	Coord current(posX + world_center, posY + world_center, 0);
	open.push(current);
	
	while (!open.empty()) {
		current = open.top();
		open.pop();
		access[current.x][current.y] = current.kabooms;
		
		char currTile = map[current.x][current.y];
		for (int i = 0; i < 4; i++) {
			int newX = current.x + forwardX[i];
			int newY = current.y + forwardY[i];
			char newTile = map[newX][newY];
			
			if (canWalk(newTile)
				|| (newTile == '~' && (currTile == '~' || currTile == 'B'))
				|| (newTile == 'T')
				|| newTile == '*') {
				Coord newCoord(newX, newY, current.kabooms + (newTile == '*' || (newTile == 'T' && !hasAxe()) ? 1 : 0));
				std::vector<Coord>::iterator iter;
				for (iter = closed.begin(); iter != closed.end(); ++iter) {
					if (newCoord == *iter) break;
				}
				
				if (iter == closed.end()) {
					closed.push_back(newCoord);
					open.push(newCoord);
				}
			}
		}
	}
}

void World::move(char command) {
	if (command == 'F' || command == 'f') { // Step forward
		if (getFront() == 'a') {
			inventory.setAxe(true);
			setBoat(false);
		} else if (getFront() == 'd') {
			inventory.addKaboom();
			setBoat(false);
		} else if (getFront() == 'g') {
			inventory.setGold(true);
			setBoat(false);
		} else if (getFront() == 'B') {
			setBoat(true);
		} else if (getFront() == ' ') {
			setBoat(false);
		}
		posX = posX + forwardX[direction];
		posY = posY + forwardY[direction];
	} else if (command == 'L' || command == 'l') { // Turn left
		direction = (direction + 3) % 4;
	} else if (command == 'R' || command == 'r') { // Turn right
		direction = (direction + 1) % 4;
	} else if (command == 'C' || command == 'c') { // Chop
	} else if (command == 'B' || command == 'b') { // BOOOOOOOOOOM!
		inventory.useKaboom();
	} else {
		printf("Y U DO DIS?");
	}
}

class aStarNode {
public:
	int posX, posY, direction, destX, destY;
	std::vector<char> path;
	
	aStarNode(const int posX, const int posY, const int direction, const int destX, const int destY, const std::vector<char> &path = std::vector<char>()) {
		this->posX = posX;
		this->posY = posY;
		this->direction = direction;
		this->destX = destX;
		this->destY = destY;
		this->path = path;
	}
	
	aStarNode(const aStarNode &old, const World &world, const char move) {
		posX = old.posX;
		posY = old.posY;
		direction = old.direction;	
		destX = old.destX;
		destY = old.destY;
		path = old.path;

		if (move == 'F' || move == 'f') {
			bool canAccess = world.canAccess(posX + world.forwardX[direction], posY + world.forwardY[direction], 0);
			char on = world.getMap(posX, posY);
			char front = world.getMap(posX + world.forwardX[direction], posY + world.forwardY[direction]);
			if (canAccess && !(front == '~' && (on == ' ' || on == 'T' || on == '*' || on == 'a' || on == 'd' || on == 'g'))) {
				if (front == 'T') path.push_back('c');
				posX += world.forwardX[direction];
				posY += world.forwardY[direction];	
			}
		} else if (move == 'L' || move == 'l') {
			direction = (direction + 3) % 4;
		} else if (move == 'R' || move == 'r') {
			direction = (direction + 1) % 4;
		}
		path.push_back(move);
	}
	
	int estimate() const {
		int dX = destX - posX;
		int dY = destY - posY;
		if (posX != destX && posY != destY) {
			return (dX < 0 ? -dX : dX) + (dY < 0 ? -dY : dY) + 1;
		} else if (posX == destX) {
			return (dX < 0 ? -dX : dX) + (dY < 0 ? -dY : dY) + ((dY == 0) || (dY > 0 && direction == 0) || (dY < 0 && direction == 2) ? 0 : 1);
		} else {
			return (dX < 0 ? -dX : dX) + (dY < 0 ? -dY : dY) + ((dX > 0 && direction == 1) || (dX < 0 && direction == 3) ? 0 : 1);
		}
	}
	
	bool operator==(const aStarNode &other) const {
		return (posX == other.posX && posY == other.posY && direction == other.direction);
	}
	
	bool operator<(const aStarNode &other) const {
		return path.size() + estimate() < other.path.size() + other.estimate();
	}
	
	bool operator>(const aStarNode &other) const {
		return path.size() + estimate() > other.path.size() + other.estimate();
	}
};

// Returns the optimal move given a destination
// Does not consider picking up tools
// If unpathable, returns 0
char World::aStar(int destX, int destY, bool kaboom) {
	// Use cached path if possible
	printf("aStar %d %d\n", destX, destY);
	if (destX == aStarDestX && destY == aStarDestY) {
		for (std::vector<char>::iterator iter = aStarCache.begin(); iter != aStarCache.end(); ++iter) {
			putchar(*iter);
		}
		char move = aStarCache.back();
		aStarCache.pop_back();
		return move;
	}
		
	if (!canAccess(destX, destY, kaboom ? 1 : 0)) {
		return 0;
	}
	
	std::vector<aStarNode> closed;
	std::priority_queue<aStarNode, std::vector<aStarNode>, std::greater<aStarNode> > open;
	aStarNode current(posX, posY, direction, destX, destY);
	open.push(current);
	
	while (!open.empty()) {
		current = open.top();
		
		// YAAAY!
		if (current.estimate() == 0 || (kaboom && current.estimate() == 1)) {
			// Update cache
			
			aStarDestX = destX;
			aStarDestY = destY;
			if (kaboom) current.path.push_back('b');
			char move = current.path.front();
			std::reverse(current.path.begin(), current.path.end());
			current.path.pop_back();
			aStarCache = current.path;
			return move;
		}
		
		// Pop off open set and add to closed set
		open.pop();
		closed.push_back(current);
		
		// Add neighbours (move forward, turn left/right)
		char testMoves[3] = {'f', 'l', 'r'};
		for (int i = 0; i < 3; ++i) {
			aStarNode nextNode = aStarNode(current, *this, testMoves[i]);
			
			// Check if in closed set
			bool found = false;
			for (std::vector<aStarNode>::iterator iter = closed.begin(); iter != closed.end(); ++iter) {
				if (nextNode == *iter) {
					found = true;
					break;
				}
			}
			
			if (!found) open.push(nextNode);
		}
	}
	
	return 0;
};

char World::explore() {
	printf("explore\n");
	std::vector<Coord> closed;
	std::queue<Coord> open;
	
	Coord current(posX, posY);
	closed.push_back(current);
	open.push(current);
	//directions of movement arraged so once added to the queue the ones with least turns required
	//will be sorted closer to front if tied with others
	while (!open.empty()) {
		current = open.front();
		if (!isExplored(current.x, current.y)) {
			char move = aStar(current.x, current.y);
			if (move != 0) {
				printf("explore: %d %d\n", current.x, current.y);
				printf("end explore\n");
				return move;
			}
		}

		open.pop();
		for (int i = 0; i < 4; ++i) {
			int newX = current.x + forwardX[i];
			int newY = current.y + forwardY[i];
			if (!canAccess(newX, newY, 0)) continue;
			
			Coord nextNode(newX, newY);
			bool found = false;
			for (std::vector<Coord>::iterator iter = closed.begin(); iter != closed.end(); ++iter) {
				if (nextNode == *iter) {
					found = true;
					break;
				}
			}
			if (!found) {
				closed.push_back(nextNode);
				open.push(nextNode);
			}
		}
	}
	
	printf("end explore\n");
	return 0;
}

char World::findInterest() {
	printf("findInterest\n");
	char interests[3] = {'g','a','d'};
	char move = 0;
	for (int i = 0; i < 3 ; ++i){
		move = findTile(interests[i]);
		if (move != 0){
			return move;
		}
	}
	return move;
}

char World::bomb(){
	if (bombX != 9001) {
		char move = aStar(bombX, bombY, true);
		if (move == 'b') bombX = 9001;
		return move;
	}
	int highScore = -1;
	int highX = -1;
	int highY = -1;

	for (int j = seenYMax; j >= seenYMin; --j) {
		for (int i = seenXMin; i <= seenXMax; ++i) {
			if ((getMap(i,j) == 'T' || getMap(i,j) == '*') && getAccess(i, j) == 1){
				int kabooms = bombVal(i, j);
				printf("checking at %d %d\n",i,j);
				printf("value: %d\n", kabooms);
				if(highScore < kabooms){
					printf("getting better\n");
					highScore = kabooms;
					highX = i;
					highY = j;
				}
			}
		}
	}
	printf("bomb at %d %d with %d \n",highX, highY, highScore);
	bombX = highX;
	bombY = highY;
	return aStar(highX, highY, true);
}

int World::bombVal(int i,int j){
	std::vector<Coord> closed;
	std::queue<Coord> open;
	
	int toolsFound = 0;
	int toolsCloser = 0;
	int goldAccess = -1;
	int reduced = 0;

	Coord current(i, j);
	closed.push_back(current);
	open.push(current);
	while (!open.empty()) {
		current = open.front();
		open.pop();
		for (int k = 0; k < 4; ++k) {
			int newX = current.x + forwardX[k];
			int newY = current.y + forwardY[k];
			char on = getMap(newX, newY);
			if (getAccess(newX, newY) > getAccess(current.x, current.y)
				|| (getAccess(newX, newY) == getAccess(current.x, current.y) && 
					((on == 'T' && hasAxe()) || on == ' ' || on == 'B' || on == 'a' || on == 'g' || on == 'd'))){ 
				Coord nextNode(newX, newY);
				bool found = false;
				for (std::vector<Coord>::iterator iter = closed.begin(); iter != closed.end(); ++iter) {
					if (nextNode == *iter) {
						found = true;
						break;
					}
				}
				if (!found) {
					++reduced;
					if ( on == 'a'  || on == 'd' || on == 'g' || on == 'B') {
						printf("tool!\n");
						if ( getAccess(newX, newY) == 1 ){
							toolsFound++;
						} else {
							toolsCloser++;
						}
						if ( on == 'g' ){
							printf("gold at %d %d\n",newX,newY);
							goldAccess = getAccess(newX, newY) - 1;
						}
					}
					closed.push_back(nextNode);
					open.push(nextNode);
				}
			}
		}
	}
	if (goldAccess == 0) {
		return 9001;
	} else {
		return toolsFound * 100 + toolsCloser * 10 + reduced;
	}
}



char World::findTile(char target) {
	char move = 0;
	for (int j = seenYMax; j >= seenYMin; --j) {
		for (int i = seenXMin; i <= seenXMax; ++i) {
			if (getMap(i,j) == target){
				move =  aStar(i, j);
				if (move != 0) return move;
			}
		}
	}
	return move;
}

void World::print() const {
	int arrayXMin = seenXMin + world_center;
	int arrayXMax = seenXMax + world_center;
	int arrayYMin = seenYMin + world_center;
	int arrayYMax = seenYMax + world_center;
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	printf("\n");
	for (int j = arrayYMax; j >= arrayYMin; --j) {
		putchar('?');
		for (int i = arrayXMin; i <= arrayXMax; ++i) {
			if (i == posX + world_center && j == posY + world_center) {
				if (direction == 0) putchar('^');
				else if (direction == 1) putchar('>');
				else if (direction == 2) putchar('v');
				else putchar('<');
			} else {
				putchar(map[i][j]);
			}
		}
		printf("?\n");
	}
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	printf("\n");
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	printf("\n");
	for (int j = arrayYMax; j >= arrayYMin; --j) {
		putchar('?');
		for (int i = arrayXMin; i <= arrayXMax; ++i) {
			if (access[i][j] == -1) putchar('X');
			else putchar(access[i][j] ^ '0');
		}
		printf("?\n");
	}
	for (int i = arrayXMin - 1; i <= arrayXMax + 1; ++i) { putchar('?'); }
	printf("\n");
}

char getAction(World &world) {
	world.print();
	printf("%d %d", world.getPositionX(), world.getPositionY());
	
	// Plan:
	// If gold can be accessed by walking/boat, then access it and return gold
	// Otherwise, explore via walking/boat, chop trees if possible
	char move = 0;
	if (world.hasGold()) {
		move = world.aStar(0,0);
	}
	if (move == 0) {
		move = world.findInterest();
	}
	if (move == 0){
		move = world.explore();
	}
	if (move == 0){
		move = world.bomb();
		//BOOOOOOOOOOOOOOOOOMB
	}
	// TODO
	getchar();
	//usleep(100);
	printf("gonna: %c\n",move);
	world.move(move);
	return move;
}

int main(int argc, char *argv[]) {
	char action;
	int sd;
	int ch;
	int i, j;
	World world = World();
	
	if (argc < 3) {
		printf("Usage: %s -p port\n", argv[0] );
		exit(1);
	}
	
	// open socket to Game Engine
	sd = tcpopen(atoi(argv[2]));
	
	pipe_fd    = sd;
	in_stream  = fdopen(sd,"r");
	out_stream = fdopen(sd,"w");
	
	char view[5][5];
	while (1) {
		// scan 5-by-5 window around current location
		for (i = 0; i < 5; ++i) {
			for (j = 0; j < 5; ++j) {
				if ((i != 2) || (j != 2)) {
					ch = getc( in_stream );
					if (ch == -1) {
						exit(1);
					}
					view[i][j] = ch;
				} else {
					view[i][j] = '^';
				}
			}
		}
		world.updateMap(view);
		
		action = getAction(world);
		putc(action, out_stream);
		fflush(out_stream);
	}
	return 0;
}
