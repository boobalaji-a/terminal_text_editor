#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE // exposes all additional gnu APIs in the headers

/*** includes ***/

#include "terminal.h"
#include "editor.h"
#include "fileIO.h"
#include "output.h"
#include "input.h"

int main(int argc, char *argv[]) {
	enable_raw_mode();

	init_editor();
	if(argc >= 2) {
		editor_open(argv[1]); 

	}

	editor_set_status_message("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find ");
	
	while(1) {
		editor_refresh_screen(); // prints out the file data to the terminal
		editor_process_key_press(); // processes each keypress and appends it to the row buffer
	}
	
	return 0;
}
