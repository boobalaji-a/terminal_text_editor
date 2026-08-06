#ifndef INPUT_H
#define INPUT_H

#include "editor.h"

char *editor_prompt(char *, void (*callback)(char *, int));
void editor_move_cursor(int);
int  editor_read_key();
void editor_process_key_press();
void editor_find_call_back(char*, int);
void editor_find();

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

#endif
