/*
 * ms.c:  A Minesweeper clone with a character interface.
 * Better than Windows' Minesweeper!  Uses vi keystrokes (e.g. h, j,
 * k, l) to move around the screen, so you know it's got to be wonderful.
 *
 * John R. Kerl
 * 12/23/95
 */

#include <stdlib.h>
#include <curses.h>
#include <sys/types.h> /* For time(0) */
#include <sys/time.h>  /* For time(0) */
#include <signal.h>

#define YES   1
#define NO    0

/* Limits on what dimensions the mine field may have */
#define MIN_X_SIZE 2
#define MIN_Y_SIZE 2
#define MAX_X_SIZE 255
#define MAX_Y_SIZE 255
#define DEFAULT_X_SIZE 15
#define DEFAULT_Y_SIZE 15
/* Number of bombs is either specified from the command line, or
 * calculated from xsize & ysize.
 */
int X_STRIDE; /* Number of actual terminal lines per cell */
int Y_STRIDE; /* Number of actual terminal columns per cell */
int X_OFFSET; /* How far from the left edge the grid begins */
int Y_OFFSET; /* How far from the upper edge the grid begins */
#define move_grid(x, y)  move(Y_OFFSET+(y)*Y_STRIDE, X_OFFSET+(x)*X_STRIDE)

/* Here's what these parameters mean:
 * +------------------------------SCREEN --------------------+
 * |    |                                                    |
 * |    v Y_OFFSET                                           |
 * |                                                         |
 * |      -->  <--- X_STRIDE                                 |
 * |   X X X X X X X      |                 ^                |
 * |   X X X X X X X      |                 |                |
 * |   X X GRID  X X      v                 ysize            |
 * |   X X X X X X X        Y_STRIDE        |                |
 * |   X X X X X X X      ^                 |                |
 * |   X X X X X X X      |                 v                |
 * |                                                         |
 * |-> X_OFFSET                                              |
 * |                                                         |
 * |   <---xsize--->                                         |
 * +---------------------------------------------------------+
 *
 * Note four things:
 *
 * Since the offsets & strides aren't necessarily 0 & 1,
 * respectively, grid coordinates can be different from screen
 * coordinates.  Specifically, grid position i,j will be at screen
 * position X_OFFSET + i * X_STRIDE, Y_OFFSET + j * Y_STRIDE.
 * The move_grid() macro takes care of this.
 *
 * I'm indexing from 1.  The 0 elements (and 1 more than xsize/ysize)
 * are used for padding.  The upper-left cell is at grid coordinates
 * 1,1.
 *
 * Curses uses y,x for the screen, but I'll use the x,y notation
 * for the grid.  This might get a little confusing.
 *
 * I originally wrote this program in BASIC on a PC; I wrote to
 * the screen by poking video RAM directly & so I found it convenient
 * to have both an internal & an external grid.  With curses
 * the external grid is mostly redundant.  In fact I run
 * the risk of being in a different position on the external grid
 * than I am with curses, if I'm not careful.
 */

/* The location on the screen (with lines/columns indexed from 1) where
 * we'll print the count of how many bombs the user has left to go, etc.:
 */
#define STATUS_LINE_NUMBER (ysize+2)
#define STATUS_COLUMN_NUMBER   0

/* Here are all the possible values that can occupy the grid cells.
 * On an IBM PC, 176 is a dithered block, 30 is a triangle,
 * 251 is a checkmark, and 15 is kind of a big splat.
 */
#define COVER      '.' /* 176 */
#define WHERE_AM_I '?' /* 176 */
#define FLAG       '#' /* 30 */
#define BLANK      ' '
#define ZERO       '0'
#define ONE        '1'
#define TWO        '2'
#define THREE      '3'
#define FOUR       '4'
#define FIVE       '5'
#define SIX        '6'
#define SEVEN      '7'
#define EIGHT      '8'

#define KABOOM     '!' /* 15 */
#define BOMB       '*'
#define BAD_GUESS  'X'
#define GOOD_GUESS FLAG /* 251 */
#define RIPPLE    '@'
/* It matters only that ripple be distinct from any other value;
 * it is temporary only (used for cascading).
 */

#define CORNER_BORDER     '+' // '%'
#define HORIZONTAL_BORDER '-' // '%'
#define VERTICAL_BORDER   '|' // '%'

/* vi equivalents for arrow keys */
#define UP           'k' /* '\001H' */
#define DOWN         'j' /* '\001P' */
#define LEFT         'h' /* '\001K' */
#define RIGHT        'l' /* '\001M' */

#define UPPER_LEFT   'q' /* '\001G' */
#define UPPER_RIGHT  'p' /* '\001I' */
#define LOWER_RIGHT  '/' /* '\001Q' */
#define LOWER_LEFT   'z' /* '\001O' */
#define ESCAPE       27
#define LEFT_BRACKET '[' /* Used in escape sequences for arrow keys, */
#define UP_ARROW     'A' /* e.g. ESC [ A for up-arrow or ESC [ K for */
#define DOWN_ARROW   'B' /* the End key on the IBM PC keypad.        */
#define RIGHT_ARROW  'C'
#define LEFT_ARROW   'D'
#define HOME         'H'
#define END          'K'

#define TOP_EDGE     'H'
#define BOTTOM_EDGE  'L'
#define LEFT_EDGE    '0'
#define RIGHT_EDGE   '$'
#define MIDDLE       'M'
#define REDRAW        014 /* ^L */
#define QUIT         'Q'
/* The two things the user can do to a cell: */
#define FLAG_AT      'f'
#define STEP_AT      's'

/* The "internal" grid:  where the bombs & neighbor counts
 * actually are (if you could see them):
 */
unsigned char igrid[MAX_X_SIZE+2][MAX_Y_SIZE+2];

/* The "external" grid: what the user sees, uncovering the external
 * grid as they go:
 */
unsigned char egrid[MAX_X_SIZE+2][MAX_Y_SIZE+2];

/* Dimensions of the minefield that the user chooses: */
int xsize=0, ysize=0;
/* I don't like globals as a rule, but these two variables are
 * used by almost every routine.
 */

int  sighandler();
int  get_grid_size_and_num_bombs();
void initialize_grids();
void get_next_move();
void move_cursor();
void reveal_all();
void cascade();
void random_pair();
void display_cell();
void display_external_grid();
void usage();

int sighandler()
{
	signal(SIGINT, SIG_IGN);
	nocrmode();
	echo();
	mvcur(0, COLS-1, LINES-2, 0);
	endwin();
	exit(0);
}

void usage(prog)
	char *prog;
{
	signal(SIGINT, SIG_IGN);
	nocrmode();
	echo();
	mvcur(0, COLS-1, LINES-2, 0);
	endwin();
	fprintf(stderr, "Usage:  %s [-s|-m|-l|-w|-f] [-x xsize] [-y ysize] [-n #mines].\n",
		prog?prog:"");
	fprintf(stderr, "  -s/m/l are for small, medium or large display; -w is wide;\n");
	fprintf(stderr, "  -f fills the window.\n");
	exit(1);
}

int bombs_found=0, number_of_flags=0, number_of_bombs=0;

int main(argc, argv)
	int argc;
	char **argv;
{
	bool done=FALSE, won=FALSE;
	int i, j;
	int x, y;
	char action;
	char *term;

	/*if (!get_grid_size_and_num_bombs(&number_of_bombs, argc, argv))*/
	/*	usage(argv[0]);*/
	/* xsize & ysize are globals & as such are not explicitly passed,
	 * although they are set in this routine.
	 * We'll parse the command line before initializing  curses so that
	 * we can just fprintf(stderr, ...) when we give a usage message.
	 */

	initscr();
	crmode();
	noecho();
	if (!get_grid_size_and_num_bombs(&number_of_bombs, argc, argv))
		usage(argv[0]);
	if ((term=(char*)getenv("TERM")) != (char*)NULL)
		if (strcmp(term, "vt100") != 0 || strcmp(term, "xterm") != 0)
			leaveok(stdscr, TRUE); /* Has to do with my "cursor" being visible */
	signal(SIGINT, sighandler);

	initialize_grids(number_of_bombs);
	/* Randomly populate the internal grid & cover the external one. */

	display_external_grid();
	move_cursor(i=xsize/2 + 1, j=ysize/2 + 1);

	while (!done) {
		getyx(stdscr, y, x);
		move_grid(STATUS_COLUMN_NUMBER, STATUS_LINE_NUMBER);
		/* Move to screen, not grid, coordinates */
		clrtoeol();
		printw("%d/%d", number_of_bombs - number_of_flags, number_of_bombs);
		move(y, x);
		refresh();

		get_next_move(&i, &j, &action);

		if (action == FLAG_AT) {
			if (egrid[i][j] == FLAG) {
				egrid[i][j]=COVER;
				number_of_flags--;
				if (igrid[i][j] == BOMB)
					bombs_found--;
				display_cell(i, j, COVER, NO);
			}
			else if (egrid[i][j] == COVER) {
				egrid[i][j]=FLAG;
				number_of_flags++;
				if (igrid[i][j]==BOMB)
					bombs_found++;
				display_cell(i, j, FLAG, YES);
			}
		} /* end if (action == FLAG_AT) */
		else if (action == STEP_AT) {
			if (egrid[i][j] == COVER) {
				if (igrid[i][j] == BOMB) {
					egrid[i][j]=KABOOM;
					done=TRUE;
					display_cell(i, j, KABOOM, NO);
					reveal_all();
				}
				else if ((igrid[i][j] >= ONE && igrid[i][j] <= EIGHT)
						|| (igrid[i][j] == BLANK)) {
					egrid[i][j] = igrid[i][j];
					display_cell(i, j, egrid[i][j], NO);
					cascade(i, j);
				}
			}
		} /* end if (action == STEP_AT) */
		else if (action == QUIT) {
			done=TRUE;
			display_cell(i, j, 0, NO);
			reveal_all();
		}
		move_cursor(i, j);
		if (number_of_flags > number_of_bombs)  {
			done=TRUE;
			reveal_all();
		}
		if (bombs_found == number_of_bombs)  {
			won=TRUE;
			done=TRUE;
		}
	} /* end while (!done) */
	move_grid(STATUS_COLUMN_NUMBER, STATUS_LINE_NUMBER);
	if (won) {
		clrtoeol();
		printw("You won!\n");
	}
	refresh();
	{
		char line[127];
		char * term;

		/* Leave a chance to look at the screen before it goes away */
		if ((term=(char*)getenv("TERM")) != (char*)NULL)
			if (strcmp(term, "vt100") != 0)
				getstr(line);
	}

	sighandler();
	/* Just use the clean-up stuff in there to exit, since it's already coded */
}

/********************************************************************/
int get_grid_size_and_num_bombs(_number_of_bombs, argc, argv)
	int * _number_of_bombs;
	int argc;
	char **argv;
{
	int i;
	bool need_to_calc_bombs=YES;

	if (argc<1)
		return FALSE;
	if (!argv)
		return FALSE;
	argv++, argc--; /* Ignore program name -- we don't care in this routine */

	xsize=DEFAULT_X_SIZE;
	ysize=DEFAULT_Y_SIZE;

	while (argc) {
		if (strcmp(argv[0], "-s") == 0) { /* small */
			xsize=15;
			ysize=15;
			break;
		}
		else if (strcmp(argv[0], "-m") == 0) { /*medium */
			xsize=20;
			ysize=20;
			break;
		}
		else if (strcmp(argv[0], "-l") == 0) { /* large */
			xsize=35;
			ysize=20;
			break;
		}
		else if (strcmp(argv[0], "-w") == 0) { /* wide */
			xsize=35;
			ysize=12;
			break;
		}
		else if (strcmp(argv[0], "-f") == 0) { /* fill the window */
			/*xsize=(COLS-3)/2;*/
			xsize=(COLS/2) - 3;
			ysize=LINES-3;
			break;
		}
		if (strcmp(argv[0], "-x") == 0) {
			if (argc<2)
				return FALSE;
			if(sscanf(argv[1], "%d", &xsize) < 1)
				return FALSE;
		}
		else if (strcmp(argv[0], "-y") == 0) {
			if (argc<2)
				return FALSE;
			if(sscanf(argv[1], "%d", &ysize) < 1)
				return FALSE;
		}
		else if (strcmp(argv[0], "-n") == 0) {
			if (argc<2)
				return FALSE;
			if(sscanf(argv[1], "%d", _number_of_bombs) < 1)
				return FALSE;
			need_to_calc_bombs=FALSE;
		}
		else
			return FALSE;
		argv+=2; argc-=2;
	}

	/* Now do some sanity checks ... */
	if (xsize < MIN_X_SIZE) xsize=MIN_X_SIZE;
	if (ysize < MIN_Y_SIZE) ysize=MIN_Y_SIZE;
	if (xsize > (COLS-3)/2) xsize=(COLS-3)/2;
	if (ysize > (LINES-3))  ysize=LINES-3;
	if (*_number_of_bombs > xsize*ysize)
		*_number_of_bombs = xsize*ysize;
		/* We'll have an infinite loop if we try to place more bombs
		 * than there are cells -- since we don't allow stacking more
		 * than one bomb per cell.
		 */
	if (need_to_calc_bombs) {
		*_number_of_bombs = xsize*ysize/6; /* From experience, a good ratio */
		if (*_number_of_bombs > 25) {
			*_number_of_bombs = 5 * ((*_number_of_bombs + 4) / 5);
			/* Round up to multiple of five, unless a small number where
			 * rounding would have too big an effect.
			 */
		}
	}

	X_STRIDE=2;
	Y_STRIDE=1;
	X_OFFSET=1;
	Y_OFFSET=0;
	return TRUE;
}

/********************************************************************/
/* A subroutine that randomly populates the internal grid with bombs and
 * covers the external grid.  Does no displaying -- just sets up the
 * matrices.
 */
void initialize_grids(number_of_bombs)
	int number_of_bombs;
{
	int number_of_tries=0;
	int bombs_so_far=0, number_of_neighbors;
	int random_x, random_y;
	int i, j;
	int i1, i2, i3, j1, j2, j3; /* Used to calculate neighbor counts */

	srand(time(0)^getpid()); /* Seed the pseudo-random generator */

	/* First, initialize the internal grid to be blank. */
	for (i=0; i<=xsize+1; i++)
		for (j=0; j<=ysize+1; j++)
			igrid[i][j] = BLANK;

	/* Second, plant the bombs. */
	while (bombs_so_far < number_of_bombs) {
		if (++number_of_tries >= 10*xsize*ysize) {
			fprintf(stderr, "I'm not doing too well randomly seeding bombs.\n");
			fprintf(stderr, "bombs_so_far = %d; number_of_bombs = %d; number_of_tries = %d\n",
					bombs_so_far, number_of_bombs, number_of_tries);
			fprintf(stderr, "xsize = %d; ysize = %d\n",
					xsize, ysize);
			sleep(2);
			sighandler();
		}
		random_pair(1, xsize, 1, ysize, &random_x, &random_y);
		if (igrid[random_x][random_y] != BOMB) {
			bombs_so_far++;
			igrid[random_x][random_y] = BOMB;
		}

	}

	/* We set up the grid to be 2 wider than the maximum dimensions + 2.
	* So, since elements either of whose indices are 0 will have no
	* bombs, we can use this area for padding when looking for neighbors.
	*/

	/* Third, make the neighbor counts. */
	for (i=0; i<=xsize; i++) {
		for (j=0; j<=ysize; j++) {
			number_of_neighbors = 0;
			if (igrid[i][j] == BOMB)
				continue; /* Don't overwrite a bomb. */
			i1 = i-1; i2 = i; i3 = i+1;
			j1 = j-1; j2 = j; j3 = j+1;
			if (igrid[i1][j1] == BOMB) number_of_neighbors++;
			if (igrid[i1][j2] == BOMB) number_of_neighbors++;
			if (igrid[i1][j3] == BOMB) number_of_neighbors++;
			if (igrid[i2][j1] == BOMB) number_of_neighbors++;
			if (igrid[i2][j3] == BOMB) number_of_neighbors++;
			if (igrid[i3][j1] == BOMB) number_of_neighbors++;
			if (igrid[i3][j2] == BOMB) number_of_neighbors++;
			if (igrid[i3][j3] == BOMB) number_of_neighbors++;

			if(number_of_neighbors > 0)
				igrid[i][j]=number_of_neighbors+ZERO;
		}
	}

	/* Fourth, cover the external grid: */
	for (j=1; j<=ysize; j++)
		for (i=1; i<=xsize; i++)
			egrid[i][j]=COVER;
}

/********************************************************************/
/* A subroutine that reads coordinates and the desired move.
 * The user scrolls around in this routine & we highlight the
 * cursor for them as it moves.  This routine doesn't return
 * until they flag or step (the only two possible actions).
 */
void get_next_move(_i, _j, _action)
	int *_i, *_j; char *_action;
{
	bool got_an_action=FALSE;
	int oldx, oldy, newx, newy;
	int key;
	char action;
	char cell_contents;

	action = *_action;
	oldx=*_i; oldy=*_j; newx=*_i; newy=*_j;

	while (got_an_action == FALSE) {
		/*
		{
			/. This is a little kludge to avoid a double-wide
			.. cursor on an xterm.
			./
			int tempx, tempy;
			getyx(stdscr, tempy, tempx);
			move_grid(STATUS_COLUMN_NUMBER, STATUS_LINE_NUMBER);
			/. Move to screen, not grid, coordinates ./
			clrtoeol();
			printw("%d:%d", number_of_bombs - number_of_flags,
					number_of_bombs);
			move(tempy, tempx);
			refresh();
		}
		*/

		oldx=newx;
		oldy=newy;
		key = getch();

		switch(key) {
			case WHERE_AM_I:
				if (egrid[oldx][oldy] != WHERE_AM_I) {
					cell_contents = egrid[oldx][oldy];
					egrid[oldx][oldy] = WHERE_AM_I;
					display_cell(oldx, oldy, egrid[oldx][oldy], NO);
				}
				else {
					egrid[oldx][oldy] = cell_contents;
					display_cell(oldx, oldy, egrid[oldx][oldy], NO);
				}
				break;
			case STEP_AT:
			case '*':
				action=STEP_AT;
				got_an_action=TRUE;
				break;
			case FLAG_AT:
			case '+':
				action=FLAG_AT;
				got_an_action=TRUE;
				break;
			case REDRAW:
				touchwin(stdscr);
				refresh();
				break;
			case QUIT:
				action=QUIT;
				got_an_action=TRUE;
			case DOWN:
				newy++;
				break;
			case UP: /* We'll do a bounds check below -- not here */
				newy--;
				break;
			case LEFT:
				newx--;
				break;
			case RIGHT:
				newx++;
				break;
			case ESCAPE:
				/* Arrows are: up, ESC [ A; down, ESC [ B; right, ESC [ C;
				** left, ESC [ D; Home, ESC [ H; End, ESC [ K.  PgUp and
				** PgDn are moot because ProComm eats them.
				** All these are three-character sequences, so we need to
				** read in the next two chars here (politely ignoring if
				** the user actually pressed the escape key).
				*/
				{
					int nextchar;
					if ((nextchar=getch()) != LEFT_BRACKET)
						break;
					switch(nextchar=getch()) {
						case UP_ARROW:
							newy--;
							break;
						case DOWN_ARROW:
							newy++;
							break;
						case LEFT_ARROW:
							newx--;
							break;
						case RIGHT_ARROW:
							newx++;
							break;
						case END:
							action=STEP_AT;
							got_an_action=TRUE;
						default:
							break; /* Politely ignore */
					}
				}
				break;
			case UPPER_LEFT:
				newx=1;
				newy=1;
				break;
			case LOWER_LEFT:
				newx=1;
				newy=ysize;
				break;
			case UPPER_RIGHT:
				newx=xsize;
				newy=1;
				break;
			case LOWER_RIGHT:
				newx=xsize;
				newy=ysize;
				break;
			case TOP_EDGE:
				newy=1;
				break;
			case BOTTOM_EDGE:
				newy=ysize;
				break;
			case LEFT_EDGE:
				newx=1;
				break;
			case RIGHT_EDGE:
				newx=xsize;
				break;
			case MIDDLE:
				newx=xsize/2+1;
				newy=ysize/2+1;
				break;
		}

		if (newx < 1)     newx = 1; /* Don't let them run off the edge */
		if (newy < 1)     newy = 1;
		if (newx > xsize) newx = xsize;
		if (newy > ysize) newy = ysize;

		display_cell(oldx, oldy, 0, FALSE);
		display_cell(newx, newy, 0, TRUE);
	}
	*_i = newx;
	*_j = newy;
	*_action = action;
}

/********************************************************************/
void move_cursor(x, y) /* Move to grid coordinates */
	int x, y;
{
	move_grid(x, y);
	standout();
	addch(inch()); /* Re-add the character that's already there -- but bold */
	standend();
	move_grid(x, y);
	refresh();
}

/********************************************************************/
/* The subroutine that reveals where bombs actually were and shows
 * erroneous guesses.
 *
 * Internal cells can be blanks, numbers, or bombs.
 * External cells can be blanks, numbers, covers, or flags.
 *
 * Blanks & numbers will be left as is.
 * Covers will be revealed.
 * Flags that flag a bomb will show as GOOD_GUESS.
 * Flags that do not flag bombs will show as BAD_GUESS.
 */
void reveal_all()
{
	int i, j;
	int c;

	for (j = 1; j <= ysize; j++) {
		for (i = 1; i <= xsize; i++) {
			if (egrid[i][j] == COVER) {
				if (igrid[i][j] == BOMB)
					c=BOMB;
				else
					/*c=egrid[i][j]=BLANK;*/
					c=COVER;
			}
			else {
				if (egrid[i][j] == FLAG) {
					if (igrid[i][j] == BOMB)
						c=GOOD_GUESS;
					else
						c=BAD_GUESS;
				}
				else
					c=egrid[i][j];
			}
			move_grid(i, j);
			addch(c);
		}
	}
	refresh();
}

/********************************************************************/
/* The cascade subroutine:  When the user clicks on an empty
 * square, reveal all adjacent empty squares as well, where
 * "adjacent" means directly above, below, right, or left
 * (diagonals do not count).
 */

void cascade(cascx, cascy)
{
	bool steady_state;
	int i, j;
	int i1, i2, i3, j1, j2, j3;


	if (igrid[cascx][cascy] != BLANK)  /* Just for safety's sake */
		return;

	igrid[cascx][cascy]=RIPPLE;
	steady_state = FALSE;
	while (steady_state == FALSE) {
		steady_state=TRUE;
		for (i=1; i<=xsize; i++) {
			for (j=1; j<=ysize; j++) {
				if (  (igrid[i][j] != BLANK) ||
						(egrid[i][j] == FLAG))
					;
				else if ((igrid[i-1][ j ] == RIPPLE) ||
							(igrid[i+1][ j ] == RIPPLE) ||
							(igrid[ i ][j-1] == RIPPLE) ||
							(igrid[ i ][j+1] == RIPPLE))
				{
				/*	... found a new element of the cascade path. */
					steady_state=FALSE;
					igrid[i][j]=RIPPLE;
					display_cell(i,j,BLANK, NO);
				}
			}
		}
	}

	/* Now turn ripples back into blanks: */
	for (i=1; i<=xsize; i++)
		for (j=1; j<=ysize; j++)
			if (igrid[i][j]==RIPPLE)
				igrid[i][j] = egrid[i][j] = BLANK;

	/* If a cell with a non-zero neighbor count borders an exposed blank,
	 * uncover it.
	 */
	for (i=1; i<=xsize; i++) {
		for (j=1; j<=ysize; j++) {
			if (igrid[i][j] >= ONE && igrid[i][j] <= EIGHT) {
				i1 = i-1; i2 = i; i3 = i+1;
				j1 = j-1; j2 = j; j3 = j+1;
				if ((egrid[i1][j1] == BLANK) ||
						(egrid[i1][j2] == BLANK) ||
						(egrid[i1][j3] == BLANK) ||
						(egrid[i2][j1] == BLANK) ||
						(egrid[i2][j3] == BLANK) ||
						(egrid[i3][j1] == BLANK) ||
						(egrid[i3][j2] == BLANK) ||
						(egrid[i3][j3] == BLANK))
				{
					egrid[i][j] = igrid[i][j];
					display_cell(i, j, egrid[i][j], NO);
				}
			}
		}
	}

	/* Position & highlight the cursor */
	move_cursor(cascx, cascy);
	display_cell(cascx, cascy, 0, YES);
}

/********************************************************************/
/* A subroutine that returns a pair of pseudo-random integers
 * within specified low and high ranges for each.
 */
void random_pair(xlo, xhi, ylo, yhi, outx, outy)
	int xlo, xhi, ylo, yhi, *outx, *outy;
{
	/* Shift off the lower 12 bits from rand(); manpage for
	 * random() says the lower 12 bits are cyclic.
	 */
	*outx = xlo + ((rand()>>12) % (xhi-xlo+1));
	*outy = ylo + ((rand()>>12) % (yhi-ylo+1));
}

/********************************************************************/
void display_cell(x, y, c, hilite)
	int x, y;
	char c;
	bool hilite;
{
	if (x < 0 || x > xsize || y < 0 || y > ysize)
		return;

	move_grid(x, y);
	if (!c)
		c=inch(); /* Peek the character at the current position on the screen */

	if (hilite)
		standout();
	addch(c);
	move_grid(x, y);
	/* An insch() shifts the rest of the line to the right, but leaves
	 * the cursor where it was before the insert.  An addch() overstrikes
	 * -- it doesn't shift the rest of the line -- but it leaves the
	 * cursor one position to the right.  I want to overstrike AND
	 * stay put -- hence the two move() calls here.
	 */

	if (hilite)
		standend();

	refresh();
}

/********************************************************************/
/* A subroutine that displays the entire external grid */
void display_external_grid()
{
	int i, j;
	int left, right, top, bottom;

	left=0; right=xsize+1; top=0; bottom=ysize+1;

	clear();

	move_grid(left,  top);    addch(CORNER_BORDER);
	move_grid(right, top);    addch(CORNER_BORDER);
	move_grid(left,  bottom); addch(CORNER_BORDER);
	move_grid(right, bottom); addch(CORNER_BORDER);

	for (j=top+1; j<=bottom-1; j++) {
		move_grid(left, j); addch(VERTICAL_BORDER);
	}
	for (j=top+1; j<=bottom-1; j++) {
		move_grid(right, j); addch(VERTICAL_BORDER);
	}
	for (i=left+1; i<=right-1; i++) {
		move_grid(i, top); addch(HORIZONTAL_BORDER);
	}
	for (i=left+1; i<=right-1; i++) {
		move_grid(i, bottom); addch(HORIZONTAL_BORDER);
	}

	for (j=1; j<=ysize; j++)
		for (i=1; i<=xsize; i++) {
			move_grid(i, j);
			addch(egrid[i][j]);
		}

	refresh();
}
