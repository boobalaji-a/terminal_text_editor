#ifndef SYNTAX_HIHLIGHTING_H
#define SYNTAX_HIGHLIGHTING_H

#include "terminal.h"
#include "row_operations.h"
#include <ctype.h>
#include <string.h>

struct editor_syntax {
	char *file_type;
	char **file_match;
	char **keywords;
	char *single_line_comment_start;
	char *multi_line_comment_start;
	char *multi_line_comment_end;
	int flags;
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

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

int is_separator(int);
void editor_update_syntax(e_row*);
int editor_syntax_to_color(int);
void editor_select_syntax_highlight();

#endif
