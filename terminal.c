#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "terminal.h"
#include "error.h"

struct editorConfig config;

void getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) err("reading cursor position");

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}

	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') err("reading cursor position (2)");
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) err("reading cursor position (3)");
}

// Determines the terminal size in rows and columns
void getTerminalSize(int *rows, int *cols) {
	struct winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1 || size.ws_col == 0) {
		err("reading terminal size");
	} else {
		*rows = size.ws_row;
		*cols = size.ws_col;
	}
}

// Restores the user's terminal to it's original settings
void restoreTerminal() {
	// Set orignalTermios back as the terminal attributes
	int code = tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.originalTermios);

	// Ensure the terminal attributes write succeeded
	if (code == -1) err("restoring terminal");
}

// Sets up raw mode in the users terminal, as needed
void setupTerminal() {

	config.cursorX = 0;
	config.cursorY = 0;
	config.lines = 0;
	config.line = NULL;
	config.scroll = 0;

	// Setup the restoreTerminal function to be called on program exit
	atexit(restoreTerminal);

	// Determine the terminal size (rows and columns)
	getTerminalSize(&config.rows, &config.cols);

	// Get the terminal attributes and save them to originalTermios, so that they can
	// be restored on exit by the restoreTerminal() function
	int code = tcgetattr(STDIN_FILENO, &config.originalTermios);

	// Ensure the terminal attributes read succeded
	if (code == -1) err("reading terminal attributes");

	struct termios rawTermios = config.originalTermios;

	// Disable ECHO in the local flags, preventing keystrokes from automatically being
	// drawn in the terminal
	rawTermios.c_lflag &= ~(ECHO);

	// Disable ISIG in the local flags, disabling SIGINT and SIGSTP signals to the
	// terminal, preventing Ctrl+C, Ctrl+Z, and Ctrl+Y from affecting the process
	rawTermios.c_lflag &= ~(ISIG);

	// Disable IEXTEN in the local flags, disabling literal character input via Ctrl+V
	rawTermios.c_lflag &= ~(IEXTEN);

	// Disable ICANON in the local flags, disabling canonical mode in the terminal. 
	// When enabled, canonical mode will only send input to the program when the user
	// presses enter. Disabling this allows us to read keystrokes, not lines
	rawTermios.c_lflag &= ~(ICANON);

	// Disable IXON in the input flags, preventing Ctrl+S and Ctrl+Q from sending XON
	// and XOFF signals--pausing and resuming data flow to the terminal
	rawTermios.c_iflag &= ~(IXON);

	// Disable ICRNL in the input flags, preventing the terminal from interpreting
	// Ctrl+M and Enter (carriage returns) as newline
	rawTermios.c_iflag &= ~(ICRNL);

	// Disable OPOST in the output flags, disabling post-processing output, notably
	// disabling the conversion of \n to \r\n.
	rawTermios.c_oflag &= ~(OPOST);

	// Set input read timeouts
	rawTermios.c_cc[VMIN] = 0;
	rawTermios.c_cc[VTIME] = 1;

	// Set raw back as the terminal attributes
	int code2 = tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios);

	// Ensure the terminal attributes write succeded
	if (code == -1) err("writing terminal attributes");
}
