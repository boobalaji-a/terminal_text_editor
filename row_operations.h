#ifndef ROW_OPERATIONS_H
#define ROW_OPERATIONS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KILO_TAB_STOP 8

typedef struct e_row{ // buffer type where all the lines from the input file are added
	int idx; // index of the row element
	int size; // size of the chars field of row element
	int r_size;  //size of render field of row element
	char *chars; // contains only printable characters of teh input file
	char *render; // contains non - printable character too
	unsigned char *hl; // colour code of the row element
	int hl_open_comment; 
} e_row;

int editor_row_cx_to_rx(e_row*, int);
int editor_row_rx_to_cx(e_row*, int);
void editor_update_row(e_row*);
void editor_insert_row(int, char*, size_t);
void editor_free_row(e_row*);
void editor_del_row(int);
void editor_row_insert(e_row*, int, int);
void editor_row_append_string(e_row*, char*, size_t);
void editor_row_del_char(e_row*, int);

#endif
