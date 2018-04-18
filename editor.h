#ifndef editor_h
#define editor_h

#include "append-buffer.h"
#include <sys/ioctl.h>
#include <termios.h>

struct EditorConfig {
  // current cursor position
  int cx, cy;
  // current window size
  struct winsize wsize;
  // the terminal settings acquired on program start
  struct termios original_termios;
};

extern struct EditorConfig config;

void initEditor(void);

// Read the next character from STDIN.
int editorReadKey(void);

int editorGetWindowSize(struct winsize *wsize);

// Read the next character from STDIN and process it immediately.
void editorProcessKeypress(void);
void editorMoveCursor(int keypress);
void editorDrawRows(struct append_buffer *ab);
void editorRefreshScreen();
int getCursorPosition(struct winsize *wsize);

#endif
