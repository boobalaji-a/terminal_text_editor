/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include "terminal.h"
#include "editor.h"
#include "fileIO.h"
#include "output.h"
#include "input.h"
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>

#define KILO_VERSION "0.0.1"

 /*** main ***/

int main(int argc, char *argv[])
{
	enable_raw_mode();
	init_editor();
	if(argc >= 2){
		editor_open(argv[1]); 
	}

	editor_set_status_message("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find ");
	
	while(1) {
		editor_refresh_screen(); // prints out the file data to the screen
		editor_process_key_press(); 
	}
	
	return 0;
}
