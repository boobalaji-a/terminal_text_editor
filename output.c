#include "output.h"
#include "editor.h"
#include "syntax_highlighting.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

void ab_append(struct abuf *ab, char *s, int len) {
	char *new = realloc(ab->b, len + ab->len);

	if(new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void ab_free(struct abuf *ab){
 	free(ab->b);
 }

/*** Function to adjust cursor position ***/
void editor_scroll() { 
	E.rx = E.cx; 
	
	if(E.cy < E.num_rows) {
		E.rx = editor_row_cx_to_rx(&E.row[E.cy], E.cx);
	}
	
	if(E.cy < E.row_off) {
		E.row_off = E.cy;
	}
	
	if(E.cy >= E.row_off + E.screen_rows) {
		E.row_off = E.cy - E.screen_rows + 1;
	}
	
	if(E.rx < E.col_off) {
		E.col_off = E.rx;
	}
	
	if(E.rx >= E.col_off + E.screen_cols) {
		E.col_off = E.rx - E.screen_cols + 1;
	}
}

void editor_draw_rows(struct abuf *ab)
{
	for(int i = 0; i < E.screen_rows; i++)
	{
		int file_row = i + E.row_off; // file_row variable acts as index to e_row row elements
		if(file_row >= E.num_rows) {
			if(E.num_rows == 0 && i == E.screen_rows / 3) {
				char welcome_msg[80];
				int welcome_len = snprintf(welcome_msg,sizeof(welcome_msg),"balG's kilo -- version %s", KILO_VERSION);
				if(welcome_len > E.screen_cols) welcome_len = E.screen_cols;
				int padding = (E.screen_cols - welcome_len)/2;
				if(padding){
					ab_append(ab, "~", 1);
					padding--;
				}
				while(padding--) ab_append(ab, " ", 1); 
				ab_append(ab, welcome_msg, welcome_len);
			}
			else {
				ab_append(ab, "~", 1);
			}
		}
		else {
			int len = E.row[file_row].r_size - E.col_off;
			if(len < 0) len = 0;
			if(len > E.screen_cols) len = E.screen_cols;
			char *c = &E.row[file_row].render[E.col_off];
			unsigned char *hl = &E.row[file_row].hl[E.col_off];
			int curr_color = -1;
			int j;
			for(j = 0; j < len; j++) {
				if(iscntrl(c[j])) {
					char sym = (c[j] <= 26) ? '@' + c[j] : '?';
					ab_append(ab, "\x1b[7m", 4); // invert colour
					ab_append(ab, &sym, 1);
					ab_append(ab, "\x1b[m", 3); // turn-off colour inversion
					if(curr_color != -1) {
						char buf[16];
						int c_len = snprintf(buf, sizeof(buf), "\x1b[%dm", curr_color);
						ab_append(ab, buf, c_len);
					}
				}
				else if(hl[j] == HL_NORMAL) {
					if(curr_color != -1) {
						ab_append(ab, "\x1b[39m", 5); // change the colour to terminal's default
						curr_color = -1;
					}
					ab_append(ab, &c[j], 1);
				}
				else {
					int color = editor_syntax_to_color(hl[j]);
					if(color != curr_color) {
						curr_color = color;
						char buf[16];
						int c_len = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						ab_append(ab, buf, c_len);
					}
					ab_append(ab, &c[j], 1);
				}
			}
			ab_append(ab, "\x1b[39m", 5);
		}

		ab_append(ab, "\x1b[K", 3); // clear line from cursor right
		ab_append(ab, "\r\n", 2);
		}
}

void editor_draw_status_bar(struct abuf *ab) {
	ab_append(ab, "\x1b[7m", 4); // inverts background (makes text look highlighted)
	char status[80]; // buffer to display the file name and no of line the file contains
	char  r_status[80]; // buffer to display file type and cursor line
	int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.file_name ? E.file_name : "[No Name]", E.num_rows, E.dirty ? "(modified)" : "");
	int r_len = snprintf(r_status, sizeof(r_status), "%s | %d %d", E.syntax ? E.syntax->file_type : "no ft", E.cy +1, E.num_rows);
	
	if(len > E.screen_cols) len = E.screen_cols;
	ab_append(ab, status, len);
	while(len < E.screen_cols) { // moving thefile type and cursor line number to the far right
		if(E.screen_cols - len == r_len) {
			ab_append(ab, r_status, r_len);
			break;
		}
		else {
			ab_append(ab, " ", 1);
			len++;
		}
	}

	ab_append(ab, "\x1b[m", 3); // turnoff character attributes
	ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(struct abuf *ab) {
	ab_append(ab, "\x1b[K", 3); // clears line from cursor right
	int msg_len = strlen(E.status_msg);
	if (msg_len > E.screen_cols) msg_len = E.screen_cols;
	if (msg_len && (time(NULL) - E.status_msg_time < 5))
		ab_append(ab, E.status_msg, msg_len);
}

void editor_refresh_screen()
{
	editor_scroll();
	
	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3); // move cursor to top left
	
	editor_draw_rows(&ab);
	editor_draw_status_bar(&ab);
	editor_draw_message_bar(&ab);

	char buf[32];
	
	/*** Move the cursor to the location where editing occurs ***/
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_off) + 1, (E.rx - E.col_off)  + 1);
	ab_append(&ab, buf, strlen(buf));

	ab_append(&ab, "\x1b[?25h", 6); // show cursor
	
	write(STDOUT_FILENO, ab.b, ab.len); // printing out everything that has been appended to the abuf buffer

	ab_free(&ab); // free the memory occupied by abuf buffer
}

void editor_set_status_message(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.status_msg, sizeof(E.status_msg), fmt,  ap);
	va_end(ap);
	E.status_msg_time = time(NULL);
}
