#include <termios.h>

#ifndef TERMINAL_H_INCLUDED
#define TERMINAL_H_INCLUDED

typedef struct editorLine {
	int size;
	char *content;
} editorLine;

struct editorConfig {
	struct termios originalTermios;
	int rows, cols;
	int cursorX, cursorY;
	int scroll;
	int lines;
	editorLine *line;
};

extern struct editorConfig config;

void restoreTerminal();
void setupTerminal(); 

#endif
