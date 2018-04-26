#ifndef editor_h
#define editor_h

#include "append-buffer.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>

typedef struct EditorRow {
  // the raw size and text (\t is always 1 char)
  int size;
  char *chars;

  // the actual rendered string and its size (\t is an impl-specific size)
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
  EditorRow *rows;
  // indicates whether the file has been modified since opening or saving
  int dirty;
  // file currently being edited.
  char *filename;
  // an optional helpful message to the user
  char status_message[80];
  // the time the status message was displayed
  time_t status_message_time;
  // the terminal settings acquired on program start
  struct termios original_termios;
};

extern struct EditorConfig config;

// Init the global state for the editor.
void editorInit(void);
// edit the file at the given path.
void editorOpen(char *filename);

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
void editorRefreshScreen(void);
int getCursorPosition(struct winsize *wsize);

#endif
