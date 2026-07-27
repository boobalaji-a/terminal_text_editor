#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1F)
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

void die(const char*);
void disable_raw_mode();
void enable_raw_mode();
int get_cursor_position(int*, int*);
int get_window_size(int*, int*);

#endif
