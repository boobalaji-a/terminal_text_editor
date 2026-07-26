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
#include <fcntl.h>

#define CTRL_KEY(k) ((k) & 0x1F)
#define ABUF_INIT {NULL, 0}

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_COUNT 3

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

enum editor_key {
  BACKSPACE = 127,
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

enum editor_highlight {
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
	HL_KEYWORD2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH
};

/*** data ***/

struct editor_syntax {
	char *file_type;
	char **file_match;
	char **keywords;
	char *single_line_comment_start;
	char *multi_line_comment_start;
	char *multi_line_comment_end;
	int flags;
};

struct abuf { // buffer type  where all the data gets appended before writing it to the output
	char *b; 
	int len;
};

typedef struct e_row{ // buffer type where all the lines from the input file are added
	int idx;
	int size;
	int r_size; 
	char *chars; // contains only printable characters of teh input file
	char *render; // contains non - printable character too
	unsigned char *hl;
	int hl_open_comment;
} e_row;

/** function prototypes **/

int get_window_size(int *rows, int *cols);
int editor_read_key();
void ab_free(struct abuf *ab);
void ab_append(struct abuf *ab, char *s, int len);
void editor_set_status_message(const char *fmt, ...);
void editor_refresh_screen();
char *editor_prompt(char *prompt, void (*call_back)(char *, int));

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

struct editor_config E;

/*** filetypes ***/

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
	"switch", "if", "while", "for", "break", "continue", "return", "else",
	"struct", "union", "typedef", "static", "enum", "class", "case",
	
	"int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
	"void|", NULL
};

struct editor_syntax HLDB[] = {
	{
		"c",
		C_HL_extensions,
		C_HL_keywords,
		"//", "/*", "*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** terminal ***/

void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4); // clear the screen
	write(STDOUT_FILENO, "\x1b[H",3); // move the cursor to upper left corner
	perror(s); 
	exit(1);
}

void disable_raw_mode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
}

void enable_raw_mode() {
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

/*** syntax highlighting ***/

int is_separator(int c) {
	return isspace(c) || c =='\0' || strchr(",.()+-/*=~%<>[];",c) != NULL;
}

void editor_update_syntax(e_row *row) {
	row->hl = realloc(row->hl, row->r_size);
	memset(row->hl, HL_NORMAL, row->r_size);

	if(E.syntax == NULL) return;

	char **keywords = E.syntax->keywords;
	
	char *scs = E.syntax->single_line_comment_start;
	char *mcs = E.syntax->multi_line_comment_start;
	char *mce = E.syntax->multi_line_comment_end;
	
	int scs_len = scs ? strlen(scs) : 0;
	int mcs_len = mcs ? strlen(mcs) : 0;
	int mce_len = mce ? strlen(mce) : 0;
	
	int prev_sep = 1;
	int in_string = 0;
	int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
	
	int i = 0;
	while(i < row->r_size) {
		char c = row->render[i];
		unsigned char prev_hl = (i < 0) ? row->hl[i - 1] : HL_NORMAL;

		if(scs_len && !in_string && !in_comment) {
			if(!strncmp(&row->render[i], scs, scs_len)) {
				memset(&row->hl[i], HL_COMMENT, row->r_size - i);
				break;
			}
		}

		if(mcs_len && mce_len && !in_string) {
			if(in_comment) {
				row->hl[i] = HL_MLCOMMENT;
				if(!strncmp(&row->render[i], mce, mce_len)) {
					memset(&row->hl[i], HL_MLCOMMENT, mce_len);
					i += mce_len;
					in_comment = 0;
					prev_sep = 1;
					continue;
				}
				else {
					i++;
					continue;
				}
			}
			else if(!strncmp(&row->render[i], mcs, mcs_len)) {
				memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
				i += mcs_len;
				in_comment = 1;
				continue;
			}
		}
		
		if(E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
			if(in_string) {
				row->hl[i] = HL_STRING;
				if(c == '\\' && i + 1 < row->r_size) {
					row->hl[i + 1] = HL_STRING;
					i += 2;
					continue;
				}
				if(c == in_string) in_string = 0;
				i++;
				prev_sep = 1;
				continue;
			}
			else {
				if(c == '"' || c == '\'') {
					in_string = c;
					row->hl[i] = HL_STRING;
					i++;
					continue;
				}
			}
		}
		
		if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
			if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {
				row->hl[i] = HL_NUMBER;
				i++;
				prev_sep = 0;
				continue;
			}
		}

		if(prev_sep) {
			int j;
			for(j = 0; keywords[j]; j++) {
				int k_len = strlen(keywords[j]);
				int kw2 = keywords[j][k_len - 1] == '|';
				if(kw2) k_len--;

				if(!strncmp(&row->render[i], keywords[j], k_len) && is_separator(row->render[i + k_len])) {
					memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, k_len);
					i += k_len;
					break;
				}
			}
			if(keywords[j] != NULL) {
				prev_sep = 0;
				continue;
			}
		}
		
		prev_sep = is_separator(c);
		i++;

		int changed = (row->hl_open_comment != in_comment);
		row->hl_open_comment = in_comment;
		if(changed && row->idx + 1 < E.num_rows)
			editor_update_syntax(&E.row[row->idx + 1]);
	}
}

int editor_syntax_to_color(int hl) {
	switch(hl) {
		case HL_COMMENT: 
		case HL_MLCOMMENT: return 36;
		case HL_KEYWORD1: return 33;
		case HL_KEYWORD2: return 32;
		case HL_STRING: return 35;
		case HL_NUMBER: return 31;
		case HL_MATCH: return 34;
		default: return 37;
	}
}

void editor_select_syntax_highlight() {
	E.syntax = NULL;
	if(E.file_name == NULL) return;

	char *ext = strrchr(E.file_name, '.');

	for(unsigned int j = 0; j < HLDB_ENTRIES; j++) {
		struct editor_syntax *s = &HLDB[j];
		unsigned int i = 0;
		while(s->file_match[i]) {
			int is_ext = (s->file_match[i][0] == '.');
			if((is_ext && ext && !strcmp(ext, s->file_match[i])) || (!is_ext && strstr(E.file_name, s->file_match[i]))) {
				E.syntax = s;

				int file_row;
				for(file_row = 0; file_row < E.num_rows; file_row++) {
					editor_update_syntax(&E.row[file_row]);
				}
				
				return;
			}
			i++;
		}
	}
}
/*** row operations ***/

int editor_row_cx_to_rx(e_row *row, int cx) {
	int rx = 0;
	int j = 0;
	for(j = 0; j < cx; j++) {
		if(row->chars[j] == '\t')
			rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
		rx++;
	}
	return rx;
}

int editor_row_rx_to_cx(e_row *row, int rx) {
	int cur_rx = 0;
	int cx;
	for(cx = 0; cx < row->size; cx++){
		if(row->chars[cx] == '\t')
			cur_rx +=(KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
		cur_rx++;

		if(cur_rx > rx) return cx;
	}
	return cx;
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

	editor_update_syntax(row);  
}

void editor_insert_row(int at, char *s, size_t len) {
	if(at < 0 || at > E.num_rows) return;

	E.row = realloc(E.row, sizeof(e_row) * (E.num_rows + 1));
	memmove(&E.row[at + 1], &E.row[at], sizeof(e_row) * (E.num_rows - at));
	for(int j = at + 1; j <= E.num_rows; j++) E.row[j].idx++;
	
	E.row[at].idx = at;
	
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].r_size = 0;
	E.row[at].render = NULL;
	E.row[at].hl = NULL;
	E.row[at].hl_open_comment = 0;
	editor_update_row(&E.row[at]);
	
	E.num_rows++;
	E.dirty++;
}

void editor_free_row(e_row *row) {
	free(row->render);
	free(row->chars);
	free(row->hl);
}

void editor_del_row(int at) {
	if(at < 0 || at >= E.num_rows) return;
	editor_free_row(&E.row[at]);
	memmove(&E.row[at], &E.row[at + 1], sizeof(e_row) * (E.num_rows - at -1));
	for(int j = at; j < E.num_rows - 1; j++) E.row[j].idx--;
	E.num_rows--;
	E.dirty++;
}

void editor_row_insert(e_row *row, int at, int c) {
	if(at < 0 || at > row->size) at = row->size;
	row->chars = realloc(row->chars,row->size + 2);

	/* moving the charcaters to the right of the position where the
	 charcater is inserted */
	memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
	row->size++;
	row->chars[at] = c;
	editor_update_row(row);
	E.dirty++;
}

void editor_row_append_string(e_row *row, char *s, size_t len) {
	row->chars = realloc(row->chars, row->size + len + 1);
	memcpy(&row->chars[row->size], s, len);
	row->size +=len;
	row->chars[row->size] = '\0';
	editor_update_row(row);
	E.dirty++;
}

void editor_row_del_char(e_row *row, int at) {
	if(at < 0 || at >= row->size) return;
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editor_update_row(row);
	E.dirty++;  
}

/*** editor operations ***/

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
		row = &E.row[E.cy]; // reduntant ??
		row->size = E.cx;
		row->chars[row->size] = '\0';
		editor_update_row(row);
	}
	E.cy++;
	E.cx = 0;
}

void editor_del_char() {
	if(E.cy == E.num_rows) return;
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

/*** file I/O ***/

char *editor_rows_to_string(int *buf_len) {
	int total_len = 0;
	int j;
	for(j = 0; j < E.num_rows; j++)
		total_len += E.row[j].size + 1;
	*buf_len = total_len;

	char *buf = malloc(total_len);
	char *p = buf;
	for(j = 0; j < E.num_rows; j++) {
		memcpy(p,E.row[j].chars, E.row[j].size);
		p += E.row[j].size;
		*p = '\n';
		p++;
	}
	return buf;
}

void editor_open(char *file_name) {
	free(E.file_name);
	E.file_name = strdup(file_name);

	editor_select_syntax_highlight();
	
	FILE *fp = fopen(file_name, "r");
	if(!fp) die("fopen");

	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len; 
	
	while((line_len = getline(&line, &line_cap, fp)) != -1) {
		while(line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))  // neglecting new line characters???
			line_len--;                                                                  

		editor_insert_row(E.num_rows, line, line_len);
	}

	free(line);
	fclose(fp);
	E.dirty = 0;
}

void editor_save() {
	if(E.file_name == NULL) {
		E.file_name = editor_prompt("Save as: %s (ESC to cancel)",NULL);
		if(E.file_name == NULL) {
			editor_set_status_message("Save aborted");
			return;
		}
		editor_select_syntax_highlight();
	}

	int len;
	char *buf = editor_rows_to_string(&len);

	/* opening the file in R/W mode, 0644 is code for 
	standard permissions required for the text file*/
	
	int fd = open(E.file_name, O_RDWR | O_CREAT, 0644);
	if(fd !=-1) {
		if(ftruncate(fd, len) != -1) { // trucates or adds zero the file to a specific size
			if(write(fd, buf, len) == len) {
				close(fd);
				free(buf);
				E.dirty = 0;
				editor_set_status_message("%d bytes written to disk", len);
				return;
			}
		}
	close(fd);
	}

	free(buf);
	editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}

/*** find **/

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
/*** append buffer ***/

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
		int file_row = i + E.row_off; // file_row variable acts as index e_row row
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

		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
		}
}

void editor_draw_status_bar(struct abuf *ab) {
	ab_append(ab, "\x1b[7m", 4); // inverts background (makes text look highlighted)
	char status[80]; // buffer to display the file name and no of line the file contains
	char  r_status[80]; // buffer to display the cursor position
	int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.file_name ? E.file_name : "[No Name]", E.num_rows, E.dirty ? "(modified)" : "");
	int r_len = snprintf(r_status, sizeof(r_status), "%s | %d %d", E.syntax ? E.syntax->file_type : "no ft", E.cy +1, E.num_rows);
	
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
	ab_append(ab, "\x1b[K", 3); // clears line from cursor right
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
	ab_append(&ab, "\x1b[H", 3); // move cursor to top left
	
	editor_draw_rows(&ab);
	editor_draw_status_bar(&ab);
	editor_draw_message_bar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.row_off) + 1, (E.rx - E.col_off)  + 1);
	ab_append(&ab, buf, strlen(buf));

	ab_append(&ab, "\x1b[?25h", 6);
	
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

/*** input ***/

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
 
 /*** init ***/

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

	editor_set_status_message("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find ");
	
	while(1) {
		editor_refresh_screen(); // prints out the file data to the screen
		editor_process_key_press(); 
	}
	
	return 0;
}
