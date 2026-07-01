/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define CTRL_KEY(k) ((k) & 0x1F)
#define ABUF_INIT {NULL, 0}

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

enum editor_key {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

struct abuf {
	char *b;
	int len;
};

typedef struct e_row{
	int size;
	int r_size;
	char *chars;
	char *render;
} e_row;

int get_window_size(int *rows, int *cols);
int editor_read_key();
void ab_free(struct abuf *ab);
void ab_append(struct abuf *ab, char *s, int len);


struct editor_config {
	int cx, cy;
	int rx;
	int row_off;
	int col_off;
	int screen_rows;
	int screen_cols;
	int num_rows;
	e_row *row;
	char *file_name;
	char status_msg[80];
	time_t status_msg_time;
	struct termios orig_termios;
};

struct editor_config E;

/*** terminal ***/

void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4); // clear the screen
	write(STDOUT_FILENO, "\x1b[H",3); // move the cursor to upper left corner
	perror(s); 
	exit(1);
}

void disable_raw_mode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
}

void enable_raw_mode()
{	
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disable_raw_mode); // disabling the custom attributes before the program exits to preserve the default state of the terminal
	
	struct termios raw = E.orig_termios;
	
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
 	raw.c_oflag &= ~(OPOST);
 	raw.c_cflag |= (CS8);
 	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
 	raw.c_cc[VMIN] = 0;
 	raw.c_cc[VTIME] = 1;
 	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int get_cursor_position(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;
	
	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; // \x1b[6n is the escape sequence which query about the cursor position in terminal

	while(i < sizeof(buf) - 1) { // the cursor position is read into the character buffer named 'buf'
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break; 
		if(buf[i] == 'R') break; // the response will be escape character followed by [rows:colsR
		i++;
		}
	buf[i] = '\0';

	if(buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int get_window_size(int *rows, int *cols){
	struct winsize ws;
	
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
		return get_cursor_position(rows, cols);
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** row operations ***/

int editor_row_cx_to_rx(e_row *row, int cx) {
	int rx = 0;
	int j = 0;
	for(j = 0; j < cx; j++){
		if(row->chars[j] == '\t')
			rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
		rx++;
	}
	return rx;
}

void editor_update_row(e_row *row) {
	int tabs = 0;
	int j;
	for(j = 0; j< row->size; j++) 
		if(row->chars[j] == '\t') tabs++;
		
	free(row->render);
	row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

	int idx = 0;
	for(j = 0; j < row->size; j++) {
		if(row->chars[j] == '\t') {
			row->render[idx++] = ' ';
			while(idx % KILO_TAB_STOP != 0) row->render[idx++] = ' '; // max characters needed for each tab is 8
		}
		else {
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx] = '\0';
	row->r_size = idx;
}

void editor_append_row(char *s, size_t len) {
	E.row = realloc(E.row, sizeof(e_row) * (E.num_rows + 1));

	int at = E.num_rows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].r_size = 0;
	E.row[at].render = NULL;
	editor_update_row(&E.row[at]);
	
	E.num_rows++;
}

/*** file I/O ***/

void editor_open(char *file_name) {
	free(E.file_name);
	E.file_name = strdup(file_name);
	
	FILE *fp = fopen(file_name, "r");
	if(!fp) die("fopen");

	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;
	
	//line_len = getline(&line, &line_cap, fp); 
	
	while((line_len = getline(&line, &line_cap, fp)) != -1) {
		while(line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
			line_len--;

		editor_append_row(line, line_len);
	}

	free(line);
	fclose(fp);
}

/*** append buffer ***/

void ab_append(struct abuf *ab, char *s, int len)
{
	char *new = realloc(ab->b, len + ab->len);

	if(new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

 void ab_free(struct abuf *ab){
 	free(ab->b);
 }

/*** output ***/

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
		int file_row = i + E.row_off;
		if(file_row >= E.num_rows){
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
			ab_append(ab, &E.row[file_row].render[E.col_off], len);
		}

		ab_append(ab, "\x1b[K", 3);

			ab_append(ab, "\r\n", 2);
		}
}

void editor_draw_status_bar(struct abuf *ab) {
	ab_append(ab, "\x1b[7m", 4);
	char status[80], r_status[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.file_name ? E.file_name : "[No Name]", E.num_rows);
	int r_len = snprintf(r_status, sizeof(r_status), "%d%d", E.cy +1, E.num_rows);
	
	if(len > E.screen_cols) len = E.screen_cols;
	ab_append(ab, status, len);
	while(len < E.screen_cols) {
		if(E.screen_cols - len == r_len) {
			ab_append(ab, r_status, r_len);
			break;
		}
		else {
			ab_append(ab, " ", 1);
			len++;
		}
	}

	ab_append(ab, "\x1b[m", 3);
	ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(struct abuf *ab) {
	ab_append(ab, "\x1b[K", 3);
	int msg_len = strlen(E.status_msg);
	if (msg_len > E.screen_cols) msg_len = E.screen_cols;
	if (msg_len && time(NULL) - E.status_msg_time < 5)
		ab_append(ab, E.status_msg, msg_len);
}

void editor_refresh_screen()
{
	editor_scroll();
	
	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3);
	
	editor_draw_rows(&ab);
	editor_draw_status_bar(&ab);
	editor_draw_message_bar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_off) + 1, (E.rx - E.col_off)  + 1);
	ab_append(&ab, buf, strlen(buf));

	ab_append(&ab, "\x1b[?25h", 6);
	
	write(STDOUT_FILENO, ab.b, ab.len);

	ab_free(&ab);
}

void editor_set_status_message(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
	va_end(ap);
	 E.status_msg_time = time(NULL);
}

/*** input ***/

void editor_move_cursor(int key)
{
	e_row *row = (E.cy >= E.num_rows) ? NULL : &E.row[E.cy];
	
	switch(key){
		case ARROW_LEFT:
			if(E.cx != 0) {
				E.cx--;
			}
			else if(E.cy > 0) {
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if(row && E.cx < row->size) {
				E.cx++;
			}
			else if(row && E.cx == row->size) {
				E.cy++;
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

int  editor_read_key()
{
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
    		if(seq[2] = '~'){
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
	int c = editor_read_key();
	switch(c)
	{
		case CTRL_KEY('q') : 
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			if(E.cy < E.num_rows)
				E.cx = E.row[E.cy].size;
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
		default :
		printf("%c",c);
	}
}

 /*** init ***/

 void init_editor() {
 	E.cx = 0;
 	E.cy = 0;
 	E.rx = 0;
 	E.row_off = 0;
 	E.col_off = 0;
 	E.num_rows = 0;
 	E.row = NULL;
 	E.file_name = NULL;
 	E.status_msg[0] = '\0';
 	E.status_msg_time = 0;
 	
 	if(get_window_size(&E.screen_rows, &E.screen_cols) == -1) die("get_window_size");
	E.screen_rows -= 2;
 }
 
 
/*** main ***/

int main(int argc, char *argv[])
{
	enable_raw_mode();
	init_editor();
	if(argc >= 2){
		editor_open(argv[1]); 
	}

	editor_set_status_message("HELP: Ctrl-Q = quit");
	
	while(1) {
		editor_refresh_screen();
		editor_process_key_press();
	}
	
	return 0;
}
