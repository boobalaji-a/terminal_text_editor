#include "input.h"
#include "output.h"
#include "fileIO.h"
#include "editor.h"
#include "syntax_highlighting.h"

#include <ctype.h>
#include <errno.h>

char* editor_prompt(char *prompt, void (*call_back)(char *, int)) {
	size_t buf_size = 128;
	char *buf = malloc(buf_size);

	size_t buf_len = 0;

	while(1) {
		editor_set_status_message(prompt, buf);
		editor_refresh_screen();

		int c = editor_read_key();
		if(c == DEL_KEY ||c == CTRL_KEY('h') || c == BACKSPACE) {
			if(buf_len != 0) buf[--buf_len] = '\0';
		}
		else if(c == '\x1b') {
			editor_set_status_message("");
			if(call_back) call_back(buf, c);
			free(buf);
			return NULL;
		}
		else if(c == '\r') {
			if(buf_len != 0) {
				editor_set_status_message("");
				if(call_back) call_back(buf, c);
				return buf;
			}
		}
		else if(!iscntrl(c) && c < 128) {
			if(buf_len == buf_size - 1) {
				buf_size *= 2;
				buf = realloc(buf, buf_size);
			}
			buf[buf_len++] = c;
			buf[buf_len] = '\0';
		}
		if(call_back) call_back(buf, c);
	}
}

void editor_move_cursor(int key)
{
	e_row *row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
	
	switch(key){
		case ARROW_LEFT:
			if(E.cx != 0) {
				E.cx--;
			}
			else if(E.cy > 0) { // if the curser is at the start of an empty line
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if(row && E.cx < row->size) {
				E.cx++;
			}
			else if(row && E.cx == row->size) { 
				E.cy++; // if the cursor is at the end of the line, the cursor moves to next line 
				E.cx = 0;
			}
			break;
		case ARROW_UP:
			if(E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if(E.cy < E.num_rows) {
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
	int row_len = row ? row->size : 0;
	if(E.cx > row_len) {
		E.cx = row_len;
	}
}

int  editor_read_key() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    	if (nread == -1 && errno != EAGAIN) die("read");
  	}
	if (c == '\x1b') {
    char seq[3];
    
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
    	if(seq[1] >= '0' && seq[1] <= '9'){
    		if(read(STDOUT_FILENO, &seq[2], 1) != 1) return '\x1b';
    		if(seq[2] = '~') {
    			switch(seq[1]) {
    				case '1': return HOME_KEY;
    				case '3': return DEL_KEY;
    				case '4': return END_KEY;
    				case '5': return PAGE_UP;
    				case '6': return PAGE_DOWN;
    				case '7': return HOME_KEY;
    				case '8': return END_KEY;
    			}
    		}
    	}
	else {
		switch (seq[1]) {
	    	case 'A': return ARROW_UP;
	        case 'B': return ARROW_DOWN;
	        case 'C': return ARROW_RIGHT;
	        case 'D': return ARROW_LEFT;
	        case 'H': return HOME_KEY;
	        case 'F': return END_KEY;
    	  }
	}
      
    }
    else if (seq[0] == 'O') {
    	switch(seq[1]){
    		case 'H': return HOME_KEY;
    		case 'F': return END_KEY;
    	}
    }
	return '\x1b';
  }
	else {
    	return c;
    }
}

void editor_process_key_press()
{
	static int quit_count = KILO_QUIT_COUNT;
	
	int c = editor_read_key(); // reads the character input 
	switch(c)
	{
		case '\r':
			editor_insert_new_line();
			break;
			
		case CTRL_KEY('q') : 
			if(E.dirty && quit_count > 0) {
				editor_set_status_message("WARNING!!! File has unsaved changes. ""Press Ctrl-Q %d more times to quit without changes.", quit_count);
				quit_count--;
				return;
			}
			write(STDOUT_FILENO, "\x1b[2J", 4); // clears entire screen
			write(STDOUT_FILENO, "\x1b[H", 3); // moves the cursor to top left
			exit(0);
			break;

		case CTRL_KEY('s'):
			editor_save();
			break;
			
		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			if(E.cy < E.num_rows)
				E.cx = E.row[E.cy].size;
			break;

		case CTRL_KEY('f'):
			editor_find();
			break;
			
		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			if(c == DEL_KEY) editor_move_cursor(ARROW_RIGHT);
			editor_del_char(); 
			break;
			
		case PAGE_UP:
		case PAGE_DOWN:
			{
				if(c == PAGE_UP) {
					E.cy = E.row_off;
				}
				else if(c == PAGE_DOWN) {
					E.cy = E.row_off + E.screen_rows - 1;
					if(E.cy > E.num_rows) E.cy = E.num_rows;
				}
				
				int times = E.screen_rows;
				while(times--)
					editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;
			
			case ARROW_UP:
			case ARROW_DOWN:
			case ARROW_LEFT:
			case ARROW_RIGHT:
				editor_move_cursor(c);
				break;

			case CTRL_KEY('l'):
				case '\x1b':
					break;
		default :
			editor_insert_char(c);
			break;
	}

	quit_count = KILO_QUIT_COUNT;
}

void editor_find_call_back(char *query, int key) {
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line;
	static char *saved_hl = NULL;

	if(saved_hl) {
		memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].r_size);
		free(saved_hl);
		saved_hl = NULL;
	}
	
	if(key == '\r' || key == '\x1b') {
		last_match = -1;
		direction = 1;
		return;
	}
	else if(key == ARROW_RIGHT || key == ARROW_DOWN) {
		direction = 1;
	}
	else if(key == ARROW_LEFT || key == ARROW_UP) {
		direction = -1;
	}
	else {
		last_match = -1;
		direction = 1;
	}

	if(last_match == -1) direction = 1;
	int current = last_match;
	int i;
	for(i = 0; i < E.num_rows; i++) {
		current += direction;
		if(current == -1) current = E.num_rows - 1;
		else if(current == E.num_rows) current = 0;
		
		e_row *row = &E.row[current];
		char *match = strstr(row->render, query);
		if(match) {
			last_match = current;
			E.cy = current;
			E.cx = editor_row_rx_to_cx(row, match - row->render);
			E.row_off = E.num_rows;

			saved_hl_line = current;
			saved_hl = malloc(row->r_size);
			memcpy(saved_hl, row->hl, row->r_size);
			memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
			break;
		}
	}
}

void editor_find() {
	int saved_cx = E.cx;
	int saved_cy = E.cy;
	int saved_col_off = E.col_off;
	int saved_row_off = E.row_off;
	
	char *query = editor_prompt("Search: %s (Use ESC/Arrows/Enter)", editor_find_call_back);
	if(query)
		free(query);

	else {
		E.cx = saved_cx;
		E.cy = saved_cy;
		E.col_off = saved_col_off;
		E.row_off = saved_row_off;
	}
}
