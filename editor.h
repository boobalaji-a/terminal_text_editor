#ifndef EDITOR_H
#define EDITOR_H

#include "row_operations.h"
#include "terminal.h"
#include <time.h>
#include <termios.h>

#define KILO_VERSION "0.0.1"

struct editor_config {   // struct containing editor variables 
	int cx, cy; // position of cursor
	int rx; // index to the render field of e_row type
	int row_off; // row of the file the user currently scrolled to
	int col_off; // col of the line the user has scrolled to 
	int screen_rows; // no of rows in the terminal
	int screen_cols; // no of cols in the terminal
	int num_rows; // no of rows of displayed
	e_row *row;
	int dirty; 
	char *file_name;
	char status_msg[80]; // buffer to display the status msg
	time_t status_msg_time;
	struct editor_syntax *syntax;
	struct termios orig_termios;
};

extern struct editor_config E;

void editor_insert_char(int);
void editor_insert_new_line();
void editor_del_char();
void init_editor();

#endif
