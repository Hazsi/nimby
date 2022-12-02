#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "terminal.h"

#define NIMBY_VERSION		"1.0.0"
#define NIMBY_WELCOME		"nimby - " NIMBY_VERSION
#define NIMBY_ABOUT		"the lightweight, easy command line editor"

#define MOD_CTRL(k) 		((k) & 0x1F)

#define CONTROL_CLEAR_SCREEN 	"\x1b[2J"	, 4
#define CONTROL_CLEAR_LINE	"\x1b[K"	, 3
#define CONTROL_RESTORE_CURSOR	"\x1b[H"	, 3
#define CONTROL_HIDE_CURSOR	"\x1b[?25l"	, 6
#define CONTROL_SHOW_CURSOR	"\x1b[?25h"	, 6

#define ARROW_UP		1000
#define ARROW_DOWN		1001
#define ARROW_LEFT		1002
#define ARROW_RIGHT		1003
#define PAGE_UP			1100
#define PAGE_DOWN		1101
#define HOME			1102
#define END			1103
#define DELETE			1104

#define ABUF_INIT {NULL, 0}

struct appendBuffer {
	char* content;
	int length;
};

void abufAppend(struct appendBuffer *buffer, const char *content, int length) {
	char *new = realloc(buffer->content, buffer->length + length);

	if (new == NULL) return;
	memcpy(&new[buffer->length], content, length);
	buffer->content = new;
	buffer->length += length;
}

void abufFree(struct appendBuffer *buffer) {
	free(buffer->content);
}

void editorScroll() {
	if (config.cursorY < config.scroll) {
		config.scroll = config.cursorY;
	}
	if (config.cursorY >= config.scroll + config.rows) {
		config.scroll = config.cursorY - config.rows + 1;
	}
}

void editorAppendRow(char *content, size_t length) {
	config.line = realloc(config.line, sizeof(editorLine) * (config.lines + 1));

	int at = config.lines;
	config.line[at].size = length;
	config.line[at].content = malloc(length + 1);
	memcpy(config.line[at].content, content, length);
	config.line[at].content[length] = '\0';
	config.lines++;
}

void editorOpen(char *fileName) {
	FILE *filePointer = fopen(fileName, "r");
	if (!filePointer) err("reading file");

	char *line = NULL;
	size_t lineCapacity = 0;
	ssize_t lineLength;

	while ((lineLength = getline(&line, &lineCapacity, filePointer)) != -1) {
		while (lineLength > 0 && (line[lineLength - 1] == '\n' || 
			line[lineLength - 1] == '\r')) lineLength--;

		editorAppendRow(line, lineLength);
	}

	free(line);
	fclose(filePointer);
}

void editorMoveCursor(int key) {
	editorLine *line = (config.cursorY >= config.lines) ? NULL : 
		&config.line[config.cursorY];

	switch (key) {
		case ARROW_UP:
			if (config.cursorY > 0) config.cursorY--;
			break;
		case ARROW_LEFT:
			if (config.cursorX > 0) config.cursorX--;
			else if (config.cursorY > 0) {
				config.cursorY--;
				config.cursorX = config.line[config.cursorY].size;
			}
			break;
		case ARROW_DOWN:
			if (config.cursorY < config.lines) config.cursorY++;
			break;
		case ARROW_RIGHT:
			if (line && config.cursorX < line->size) config.cursorX++;
			else if (line && config.cursorX == line->size) {
				config.cursorY++;
				config.cursorX = 0;
			}
			break;
	}

	line = (config.cursorY >= config.lines) ? NULL : &config.line[config.cursorY];
	int lineLength = line ? line->size : 0;

	if (config.cursorX > lineLength) {
		config.cursorX = lineLength;
	}
}

int editorReadKey() {
	int nread;
	char key;
	while ((nread = read(STDIN_FILENO, &key, 1)) != 1);

	if (key == '\x1b') {
		char sequence[3];

		if (read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';

		if (sequence[0] == '[') {
			if (sequence[1] >= '0' && sequence[1] < '9') {
				if (read(STDIN_FILENO, &sequence[2],1) != 1) return '\x1b';
				if (sequence[2] == '*') {
					switch(sequence[1]) {
						case '1': return HOME;
						case '3': return DELETE;
						case '4': return END;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME;
						case '8': return END;
					}
				}
			} else {
				switch (sequence[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME;
					case 'F': return END;
				}
			}
		} else if (sequence[0] == '0') {
			switch(sequence[1]) {
				case 'H': return HOME;
				case 'F': return END;
			}
		}

		return '\x1b';
	} else return key;
}

void editorProcessKey() {
	int key = editorReadKey();

	switch (key) {
		case MOD_CTRL('q'):

			// Clear screen and move cursor to top-left corner
			write(STDOUT_FILENO, CONTROL_CLEAR_SCREEN);
			write(STDOUT_FILENO, CONTROL_RESTORE_CURSOR);

			exit(0);
			break;
		
		case HOME:
			config.cursorX = 0;
			break;
		case END:
			config.cursorX = config.cols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN: {
			int times = config.rows;
			while (times-- > 0) {
				editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}}

			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(key);
			break;
	}
}

void editorDrawRows(struct appendBuffer *buffer) {
	for (int row = 0; row < config.rows; row++) {
	
		int scrollrow = row + config.scroll;	
		if (scrollrow >= config.lines) {
			if (config.lines == 0 && row == config.rows / 2) {
				char welcome[80];
				int welcomelength = snprintf(welcome, 
					sizeof(welcome), NIMBY_WELCOME);

				if (welcomelength > config.cols) 
					welcomelength = config.cols;

				int padding = (config.cols - welcomelength) / 2;
				if (padding > 0) {
					abufAppend(buffer, "*", 1);
					padding--;
				}

				while (padding-- > 0) abufAppend(buffer, " ", 1);

				abufAppend(buffer, welcome, welcomelength);
			} else {
				abufAppend(buffer, "*", 1);
			}
		} else {
			int length = config.line[scrollrow].size;
			if (length > config.cols) length = config.cols;
			abufAppend(buffer, config.line[scrollrow].content, length);
		}

		abufAppend(buffer, CONTROL_CLEAR_LINE);

		// We don't want a newline on the last line of our terminal
		if(row < (config.rows - 1)) {
			abufAppend(buffer, "\r\n", 2);
		}
	}
}

void editorRefreshScreen() {

	editorScroll();

	struct appendBuffer buffer = ABUF_INIT;

	abufAppend(&buffer, CONTROL_HIDE_CURSOR);
	abufAppend(&buffer, CONTROL_RESTORE_CURSOR);

	editorDrawRows(&buffer);
	
	// Move terminal cursor to nimby cursor location
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (config.cursorY - config.scroll) + 1, 
		config.cursorX + 1);
	abufAppend(&buffer, buf, strlen(buf));

	abufAppend(&buffer, CONTROL_SHOW_CURSOR);

	write(STDOUT_FILENO, buffer.content, buffer.length);
	abufFree(&buffer);
}

int main(int argc, char *argv[]) {
	setupTerminal();
	
	if (argc >= 2) {
		editorOpen(argv[1]);
	}

	while (1) {
		editorRefreshScreen();
		editorProcessKey();
	}

	return 0;
}
