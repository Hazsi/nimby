#include <stdio.h>
#include <stdlib.h>

#include "error.h"

void err(const char* string) {
	perror(string);
	exit(1);
}
