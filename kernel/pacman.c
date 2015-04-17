
#include <kernel.h>

unsigned int ghost_sleep = 0x38;

typedef struct {
    int x;
    int y;
} GHOST;

static WINDOW pacman_window_def = {WINDOW_TOTAL_WIDTH - MAZE_WIDTH, WINDOW_TOTAL_HEIGHT - MAZE_HEIGHT - 1, MAZE_WIDTH, MAZE_HEIGHT + 1, 0, 0, GHOST_CHAR};
WINDOW* pacman_wnd = &pacman_window_def;


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
    int x, y;
    while (1)
    {
    	x = random() % MAZE_WIDTH;
    	y = random() % MAZE_HEIGHT;
    	if (maze[y][x] != ' ') continue;
    	ghost->x = x;
    	ghost->y = y;
    	break;
    }
}

void move_ghost(GHOST *ghost)
{
    int dx, dy, num;
    dx = -ghost->x;
    dy = -ghost->y;

    do
    {
        dx = dy = 0;
        num = random() % 4;
        switch (num)
        {
            case 0:
                dx = 1;
                break;
            case 1:
                dy = -1;
                break;
            case 2:
                dx = -1;
                break;
            case 3:
                dy = 1;
                break;
        }
    }
    while (maze[ghost->y + dy][ghost->x + dx] != ' ');

    /* move ghost in pacman_wnd */

    move_cursor(pacman_wnd, ghost->x, ghost->y);
    remove_cursor(pacman_wnd);
    ghost->x += dx;
    ghost->y += dy;
    move_cursor(pacman_wnd, ghost->x, ghost->y);
    show_cursor(pacman_wnd);
}

void create_new_ghost()
{
    GHOST ghost;
    init_ghost(&ghost);
    while(1)
    {
        sleep(ghost_sleep);
        move_ghost(&ghost);
    }
}

void ghost_proc(PROCESS self, PARAM param)
{
    create_new_ghost();
}

void init_pacman(WINDOW* wnd, int num_ghosts)
{
    if (wnd->height < MAZE_HEIGHT + 1 || wnd->width < MAZE_WIDTH)
    {
        wprintf(wnd, "Window too small\n");
        return;
    }

    pacman_wnd = wnd;
    pacman_wnd->width = MAZE_WIDTH;
    pacman_wnd->height = MAZE_HEIGHT + 1;
    pacman_wnd->cursor_char = GHOST_CHAR;
    move_cursor(pacman_wnd, 0, 0);

    // clear_window(pacman_wnd);

    draw_maze();

    int j;
    for (j = 0; j < num_ghosts; j++)
        create_process(ghost_proc, 3, 0, "Ghost");
}

