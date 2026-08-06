#include "editor.h"
#include "row_operations.h"
#include "syntax_highlighting.h"

struct editor_config E;

void init_editor() {
 	E.cx = 0;
 	E.cy = 0;
 	E.rx = 0;
 	E.row_off = 0;
 	E.col_off = 0;
 	E.num_rows = 0;
 	E.row = NULL;
 	E.dirty = 0;
 	E.file_name = NULL;
 	E.status_msg[0] = '\0';
 	E.status_msg_time = 0;
 	E.syntax = NULL;
 	
 	if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) die("get_window_size");
	E.screen_rows -= 2; // decrementing screen_rows by 2 because last two lines ar reserved for status bar
 }
 
void editor_insert_char(int c) {
	if(E.cy == E.num_rows) {
		editor_insert_row(E.num_rows, "", 0);
	}
	editor_row_insert(&E.row[E.cy], E.cx, c);
	E.cx++;
}

void editor_insert_new_line() {
	if(E.cx == 0) {
		editor_insert_row(E.cy, "", 0);
	}
	else {
		e_row *row = &E.row[E.cy];
		editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
		row = &E.row[E.cy]; 
		row->size = E.cx;
		row->chars[row->size] = '\0';
		editor_update_row(row);
	}
	E.cy++;
	E.cx = 0;
}

void editor_del_char() {
	if(E.cy == E.num_rows) {
		E.cy--;
		E.cx = E.row[E.cy].r_size;
		return;
	}
	if(E.cx == 0 && E.cy == 0) return;
	
	e_row *row = &E.row[E.cy];

	/* if there is a character to the left of the cursor
	we initiate deletion */
	if(E.cx > 0) { 
		editor_row_del_char(row, E.cx - 1);
		E.cx--;
	}
	else {
		E.cx = E.row[E.cy - 1].size;
		editor_row_append_string(&E.row[E.cy - 1], row->chars, row->size);
		editor_del_row(E.cy);
		E.cy--;
	}
}
