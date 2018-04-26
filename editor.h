#ifndef editor_h
#define editor_h

#include "append-buffer.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>

typedef struct EditorRow {
  // the raw size and text
  int size;
  char *chars;

  // the actual rendered string and its size
  int render_size;
  char *render;
} EditorRow;

struct EditorConfig {
  // current cursor position
  int cx, cy;
  // current rendered cursor position
  int rx;
  // current window size
  struct winsize wsize;
  // number of rows in the current document
  int row_count;
  // current row offset
  int row_offset;
  // current col offset
  int col_offset;
  // the lines in the current file
  // TODO: rename
  EditorRow *current_row;
  // indicates whether the file has been modified since opening or saving
  int dirty;
  // file currently being edited.
  char *filename;

  char status_message[80];
  time_t status_message_time;
  // the terminal settings acquired on program start
  struct termios original_termios;
};

extern struct EditorConfig config;

void initEditor(void);
void openEditor(char *filename);

// Read the next character from STDIN.
int editorReadKey(void);

int editorGetWindowSize(struct winsize *wsize);

void editorSetStatusMessage(const char *fmt, ...);
char *editorPrompt(char *prompt);

// Read the next character from STDIN and process it immediately.
void editorProcessKeypress(void);
void editorMoveCursor(int keypress);
void editorDrawRows(struct append_buffer *ab);
void editorScroll(void);
void editorRefreshScreen();
int getCursorPosition(struct winsize *wsize);

#endif
