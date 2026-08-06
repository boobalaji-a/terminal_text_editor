#include "row_operations.h"
#include "editor.h"
#include "syntax_highlighting.h"

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

/*** Function to update render field once a new line inserted ***/
void editor_update_row(e_row *row) {
	int tabs = 0;
	int j;
	for(j = 0; j< row->size; j++) 
		if(row->chars[j] == '\t') tabs++;
		
	free(row->render);
	row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1); 
	int i = 0;
	for(j = 0; j < row->size; j++) {
		if(row->chars[j] == '\t') {
			row->render[i++] = ' ';
			while(i % KILO_TAB_STOP != 0) row->render[i++] = ' '; // space characters needed for each tab is 8
		}
		else {
			row->render[i++] = row->chars[j];
		}
	}
	row->render[i] = '\0';
	row->r_size = i;

	editor_update_syntax(row); 
	 
}

void editor_insert_row(int at, char *s, size_t len) {
	if(at < 0 || at > E.num_rows) return;

	E.row = realloc(E.row, sizeof(e_row) * (E.num_rows + 1)); // allocating extra space for the new row
	memmove(&E.row[at + 1], &E.row[at], sizeof(e_row) * (E.num_rows - at)); // moving the contents of the current line to new line
	for(int j = at + 1; j <= E.num_rows; j++) E.row[j].idx++; // incrementing the idx of every row below the inserted row

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
	memmove(&E.row[at], &E.row[at + 1], sizeof(e_row) * (E.num_rows - at - 1));
	for(int j = at; j < E.num_rows - 1; j++) E.row[j].idx--;
	E.num_rows--;
	E.dirty++;
}

void editor_row_insert(e_row *row, int at, int c) {
	if(at < 0 || at > row->size) at = row->size;
	row->chars = realloc(row->chars,row->size + 2);

	/* moving the charcaters to the right of the position where the
	 character is inserted */
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
