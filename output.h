#ifndef OUTPUT_H
#define OUTPUT_H

#define ABUF_INIT {NULL, 0}

struct abuf { // buffer type  where all the data gets appended before writing it to the output
	char *b; 
	int len;
};

void ab_append(struct abuf*, char*, int);
void ab_free(struct abuf*);
void editor_scroll();
void editor_draw_rows(struct abuf*);
void editor_draw_status_bar(struct abuf*);
void editor_draw_message_bar(struct abuf*);
void editor_refresh_screen();
void editor_set_status_message(const char*, ...);

#endif
