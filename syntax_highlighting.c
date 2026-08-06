#include "syntax_highlighting.h"
#include "editor.h"

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *PYTHON_HL_extensions[] = {".py", NULL};
char *JAVA_HL_extensions[] = {".java", NULL};

char *C_HL_keywords[] = {
	"switch", "if", "while", "for", "break", "continue", "return", "else",
	"struct", "union", "typedef", "static", "enum", "class", "case",
	
	"int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
	"void|", NULL
};

char *PYTHON_HL_keywords[] = {
 	"False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield", NULL
};

char *JAVA_HL_keywords[] = {
 	"abstract", "assert", "break", "case", "catch", "class", "const", "continue",
    "default", "do", "else", "enum", "extends", "final", "finally", "for","goto",
    "if", "implements", "import", "instanceof","interface","native", "new", "package",
    "private", "protected", "public", "return", "static", "strictfp", "super", "switch",
    "synchronized", "this", "throw", "throws", "transient", "try", "volatile","while",

    "boolean|", "byte|", "char|", "double|", "float|",   "int|", 
     "long|", "short|","void|", NULL
};

struct editor_syntax HLDB[] = {
	{
		"c",
		C_HL_extensions,
		C_HL_keywords,
		"//", "/*", "*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
	
	{
		"py",
		PYTHON_HL_extensions,
		PYTHON_HL_keywords,
		"#", NULL, NULL,
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},

	{
		"java",
		JAVA_HL_extensions,
		JAVA_HL_keywords,
		"//", "/*", "*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	}
};

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
		unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

		if(is_separator(c) && !in_comment && !in_string) {
			row->hl[i] = HL_SEPARATOR;
		}
		
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
			else if(!strncmp(&row->render[i], mcs, mcs_len)) { //check for  what case it is
				memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
				i += mcs_len;
				in_comment = 1;
				continue;
			}
		}
		
		if(E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
			if(in_string) {
				row->hl[i] = HL_STRING;
				if(c == '\\' && i + 1 < row->r_size) { // coloring for the escape charater
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
		case HL_SEPARATOR:
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

	char *extension = strrchr(E.file_name, '.');

	for(unsigned int j = 0; j < HLDB_ENTRIES; j++) {
		struct editor_syntax *s = &HLDB[j];
		unsigned int i = 0;
		while(s->file_match[i]) {
			int is_extension = (s->file_match[i][0] == '.');
			if((is_extension && extension && !strcmp(extension, s->file_match[i])) || (!is_extension && strstr(E.file_name, s->file_match[i]))) {
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
