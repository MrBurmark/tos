
#include <kernel.h>

#define MAZE_WIDTH  19
#define MAZE_HEIGHT 16
#define GHOST_CHAR  0x02

typedef struct {
    int x;
    int y;
} GHOST;


WINDOW* pacman_wnd;



char *maze[] = {
    "r--------T--------i",
    "|        |        |",
    "| ri r-i | r-i ri |",
    "| Ll L-l | L-l Ll |",
    "|                 |",
    "| -- | --T-- | -- |",
    "|    |   |   |    |",
    "E--- L--   --l ---e",
    "|        |        |",
    "| -i --- | --- r- |",
    "|  |           |  |",
    "E- | | --T-- | | -e",
    "|    |   |   |    |",
    "| ---t-- | --t--- |",
    "|                 |",
    "L-----------------l",
    NULL
};


void draw_maze_char(char maze_char)
{
    char ch = ' ';
    
    // For details of PC-ASCII characters see:
    // http://www.jimprice.com/jim-asc.shtml
    switch (maze_char) {
    case '|':
	ch = 0xB3;
	break;
    case '-':
	ch = 0xC4;
	break;
    case 'r':
	ch = 0xDA;
	break;
    case 'i':
	ch = 0xBF;
	break;
    case 'L':
	ch = 0xC0;
	break;
    case 'l':
	ch = 0xD9;
	break;
    case 'T':
	ch = 0xC2;
	break;
    case 't':
	ch = 0xC1;
	break;
    case 'E':
	ch = 0xC3;
	break;
    case 'e':
	ch = 0xB4;
	break;
    }
    output_char(pacman_wnd, ch);
}



void draw_maze()
{
    int x, y;
    
    clear_window(pacman_wnd);
    y = 0;
    while (maze[y] != NULL) {
	char* row = maze[y];
	x = 0;
	while (row[x] != '\0') {
	    char ch = row[x];
	    draw_maze_char(ch);
	    x++;
	}
	y++;
    }
    wprintf(pacman_wnd, "PacMan ");
}



// Pseudo random number generator
// http://cnx.org/content/m13572/latest/
int seed = 17489;
int last_random_number = 0;

int random()
{
    last_random_number = (25173 * last_random_number + 13849) % 65536;
    return last_random_number;
}

void init_ghost(GHOST* ghost)
{
    while (1)
    {
    	int x = random() % MAZE_WIDTH;
    	int y = random() % MAZE_HEIGHT;
    	if (maze[y][x] != ' ') continue;
    	ghost->x = x;
    	ghost->y = y;
    	break;
    }
}


void wait(unsigned int n)
{
    unsigned int i;
    n *= 10;
    for(i=0; i < n; i++) ;
}


void create_new_ghost()
{
    int x, y;
    GHOST ghost;
    init_ghost(&ghost);
    while(1)
    {
        x = random() % 4 - 1;
        y = 0;
        if (x % 2 == 0)
        {
            y = x - 1;
            x = 0;
        }

        if (maze[ghost.y + y][ghost.x + x] != ' ') continue;
        ghost.x += x;
        ghost.y += y;
        move_cursor(pacman_wnd, ghost.x, ghost.y);
        wait(250);
    }
}

    
void init_pacman(WINDOW* wnd, int num_ghosts)
{
    pacman_wnd = wnd;
    pacman_wnd->width = MAZE_WIDTH;
    pacman_wnd->height = MAZE_HEIGHT + 1;
    pacman_wnd->cursor_char = GHOST_CHAR;

    draw_maze();

    int j;
    for (j = 0; j < num_ghosts; j++)
        create_new_ghost();
}

