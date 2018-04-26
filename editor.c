#include "editor.h"
#include "editor-key.h"
#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define CTRL_KEY(k) ((k)&0x1f)

struct EditorConfig config;

void initEditor(void) {
  config.cx = 0;
  config.cy = 0;
  config.rx = 0;
  config.row_offset = 0;
  config.col_offset = 0;
  config.row_count = 0;
  config.current_row = NULL;
  config.dirty = 0;
  config.filename = NULL;

  config.status_message[0] = '\0';
  config.status_message_time = 0;

  if (editorGetWindowSize(&config.wsize) == -1) {
    die("could not get editor window size.");
  }

  config.wsize.ws_row -= 2;
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(config.status_message, sizeof(config.status_message), fmt, ap);
  va_end(ap);
  config.status_message_time = time(NULL);
}

void editorUpdateRow(EditorRow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  // render needs whatever is currently in the row, and 7 additional bytes for
  // each tab
  row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

  int idx = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      // pad tabs out to the next 8
      while (idx % KILO_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->render_size = idx;
}

void editorInsertRow(int at, char *s, size_t length) {
  if (at < 0 || at > config.row_count) {
    return;
  }

  config.current_row =
      realloc(config.current_row, sizeof(EditorRow) * (config.row_count + 1));
  memmove(&config.current_row[at + 1], &config.current_row[at],
          sizeof(EditorRow) * (config.row_count - at));

  config.current_row[at].size = length;
  config.current_row[at].chars = malloc(length + 1);
  memcpy(config.current_row[at].chars, s, length);
  config.current_row[at].chars[length] = '\0';

  config.current_row[at].render_size = 0;
  config.current_row[at].render = NULL;

  editorUpdateRow(&config.current_row[at]);

  config.dirty = 1;
  config.row_count++;
}

void editorFreeRow(EditorRow *row) {
  free(row->render);
  free(row->chars);
}

void editorDeleteRow(int at) {
  if (at < 0 || at >= config.row_count) {
    return;
  }
  editorFreeRow(&config.current_row[at]);
  memmove(&config.current_row[at], &config.current_row[at + 1],
          sizeof(EditorRow) * (config.row_count - at - 1));
  config.row_count--;
  config.dirty = 1;
}

void editorRowInsertChar(EditorRow *row, int at, int c) {
  if (at < 0 || at > row->size) {
    // TODO: it seems like a programmer error to pass an invalid row index
    at = row->size;
  }

  // realloc, (2 is for the new byte and null byte)
  row->chars = realloc(row->chars, row->size + 2);
  // dst, src, length; copy {size+1} bytes from {at} to {at + 1}
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  config.dirty = 1;
}

void editorRowAppendString(EditorRow *row, char *s, size_t length) {
  row->chars = realloc(row->chars, row->size + length + 1);
  memcpy(&row->chars[row->size], s, length);
  row->size += length;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  config.dirty = 0;
}

void editorRowDeleteChar(EditorRow *row, int at) {
  if (at < 0 || at >= row->size) {
    return;
  }

  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  config.dirty = 1;
}

void editorInsertChar(int c) {
  // insert a particular character at the current { x, y }
  if (config.cy == config.row_count) {
    editorInsertRow(config.row_count, "", 0);
  }
  editorRowInsertChar(&config.current_row[config.cy], config.cx, c);
  config.cx++;
}

void editorInsertNewline() {
  if (config.cx == 0) {
    editorInsertRow(config.cy, "", 0);
  } else {
    EditorRow *row = &config.current_row[config.cy];
    // insert a new row, using the bits to the right of the cursor
    editorInsertRow(config.cy + 1, &row->chars[config.cx],
                    row->size - config.cx);
    row = &config.current_row[config.cy];
    row->size = config.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  // update the cursor
  config.cy++;
  config.cx = 0;
}

void editorDeleteChar(void) {
  if (config.cy == config.row_count) {
    return;
  }
  if (config.cx == 0 && config.cy == 0) {
    // delete doesn't do anything at {0,0}
    return;
  }
  EditorRow *row = &config.current_row[config.cy];
  if (config.cx > 0) {
    editorRowDeleteChar(row, config.cx - 1);
    config.cx--;
  } else {
    // set the cursor position
    config.cx = config.current_row[config.cy - 1].size;
    // append the contents of the current row to the previous row
    editorRowAppendString(&config.current_row[config.cy - 1], row->chars,
                          row->size);
    // remove the current row
    editorDeleteRow(config.cy);
    config.cy--;
  }
}

char *editorRowsToString(int *buffer_length) {
  int total_length = 0;

  // calculate length of current buffer
  for (int j = 0; j < config.row_count; j++) {
    // include a byte for a newline character at the end of each line
    total_length += config.current_row[j].size + 1;
  }

  *buffer_length = total_length;

  // allocate a new buffer and copy the contents into it
  char *buf = malloc(total_length);
  char *p = buf;
  for (int j = 0; j < config.row_count; j++) {
    memcpy(p, config.current_row[j].chars, config.current_row[j].size);
    p += config.current_row[j].size;
    // append the newline we allocated size for
    *p = '\n';
    p++;
  }

  return buf;
}

void openEditor(char *filename) {
  free(config.filename);
  config.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) {
    die("could not open file.");
  }

  char *line = NULL;
  size_t linecap = 0;
  ssize_t line_length;

  while ((line_length = getline(&line, &linecap, fp)) != -1) {
    while (line_length > 0 &&
           (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')) {
      line_length--;
    }
    editorInsertRow(config.row_count, line, line_length);
  }

  free(line);
  fclose(fp);
  config.dirty = 0;
}

void editorSave() {
  // TODO: prompt user for a filename
  if (config.filename == NULL) {
    config.filename = editorPrompt("Save as: %s");
    if (config.filename == NULL) {
      editorSetStatusMessage("Save aborted.");
      return;
    }
  }

  int length;
  char *buffer = editorRowsToString(&length);

  //  0644 -> owner has read/write, others can read
  int file_descriptor = open(config.filename, O_RDWR | O_CREAT, 0644);

  if (file_descriptor != -1) {
    if (ftruncate(file_descriptor, length) != -1) {
      if (write(file_descriptor, buffer, length) == length) {
        close(file_descriptor);
        free(buffer);
        editorSetStatusMessage("Saved %d bytes to %s successfully.", length,
                               config.filename);
        config.dirty = 0;
        return;
      }
    }
    close(file_descriptor);
  }

  free(buffer);
  editorSetStatusMessage("Could not save file! I/O error: %s", strerror(errno));
}

/*
 NOTE: The intent of this function is very unclear.
 This is a very 'C' way to write this; there's nothing wrong with it, but it
 seems like the reader will have trouble following what it means or why.
 */

int editorReadKey(void) {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) {
      die("could not read from STDIN.");
    }
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
        read(STDIN_FILENO, &seq[1], 1) != 1) {
      return '\x1b';
    }

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) {
          return '\x1b';
        }
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == '0') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  }

  return c;
}

void editorDrawRows(struct append_buffer *ab) {
  for (int y = 0; y < config.wsize.ws_row; y++) {
    int filerow = y + config.row_offset;

    if (y >= config.row_count) {
      // print welcome message 1/3 of the way down the page
      if (config.row_count == 0 && y == config.wsize.ws_row / 3) {
        char welcome[80];
        int welcome_length =
            snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s",
                     KILO_VERSION);
        if (welcome_length > config.wsize.ws_col) {
          welcome_length = config.wsize.ws_col;
        }
        // center it in the window
        int padding = (config.wsize.ws_col - welcome_length) / 2;
        if (padding) {
          append_buffer_append(ab, "~", 1);
        }
        while (padding--) {
          append_buffer_append(ab, " ", 1);
        }

        append_buffer_append(ab, welcome, welcome_length);
      } else {
        append_buffer_append(ab, "~", 1);
      }
    } else {
      int length = config.current_row[filerow].render_size - config.col_offset;
      if (length < 0) {
        length = 0;
      }
      if (length > config.wsize.ws_col) {
        length = config.wsize.ws_col;
      }
      append_buffer_append(
          ab, &config.current_row[filerow].render[config.col_offset], length);
    }

    // 'ERASE IN LINE': clear each line as we redraw it
    append_buffer_append(ab, "\x1b[K", 3);
    append_buffer_append(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct append_buffer *ab) {
  // m -> select graphic rendition
  append_buffer_append(ab, "\x1b[7m", 4);

  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     config.filename ? config.filename : "[No Name]",
                     config.row_count, config.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", config.cy + 1,
                      config.row_count);

  if (len > config.wsize.ws_col) {
    len = config.wsize.ws_col;
  }
  append_buffer_append(ab, status, len);

  while (len < config.wsize.ws_col) {
    if (config.wsize.ws_col - len == rlen) {
      append_buffer_append(ab, rstatus, rlen);
      break;
    } else {
      append_buffer_append(ab, " ", 1);
      len++;
    }
  }
  append_buffer_append(ab, "\x1b[m", 3);
  append_buffer_append(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct append_buffer *ab) {
  append_buffer_append(ab, "\x1b[K", 3);
  int len = strlen(config.status_message);
  if (len > config.wsize.ws_col) {
    len = config.wsize.ws_col;
  }
  // time out messages after 5 seconds
  if (len && time(NULL) - config.status_message_time < 5) {
    append_buffer_append(ab, config.status_message, len);
  }
}

int editorRowCxToRx(EditorRow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') {
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

void editorScroll(void) {
  config.rx = 0;
  if (config.cy < config.row_count) {
    config.rx = editorRowCxToRx(&config.current_row[config.cy], config.cx);
  }

  if (config.cy < config.row_offset) {
    config.row_offset = config.cy;
  }
  if (config.cy >= config.row_offset + config.wsize.ws_row) {
    config.row_offset = config.cy - config.wsize.ws_row + 1;
  }
  if (config.rx < config.col_offset) {
    config.col_offset = config.rx;
  }
  if (config.rx >= config.col_offset + config.wsize.ws_col) {
    config.col_offset = config.rx - config.wsize.ws_col + 1;
  }
}

void append_terminal_command_to_buffer(struct append_buffer *ab,
                                       struct TerminalCommand tc) {
  append_buffer_append(ab, tc.string_representation, tc.length);
}

void editorRefreshScreen(void) {
  editorScroll();

  struct append_buffer ab = append_buffer_init;

  append_terminal_command_to_buffer(&ab, moveCursorToTopLeft());
  append_terminal_command_to_buffer(&ab, hideCursor());

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (config.cy - config.row_offset) + 1,
           (config.rx - config.col_offset) + 1);
  append_buffer_append(&ab, buf, strlen(buf));

  append_terminal_command_to_buffer(&ab, displayCursor());

  write(STDOUT_FILENO, ab.b, ab.length);
  append_buffer_free(&ab);
}

int editorGetWindowSize(struct winsize *wsize) {
  // read into a temporary var; we don't want `ioctl` to insert garbage on error
  struct winsize temp;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &temp) == -1 || temp.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      return -1;
    }

    return getCursorPosition(wsize);
  }

  wsize->ws_col = temp.ws_col;
  wsize->ws_row = temp.ws_row;

  return 0;
}

void editorProcessKeypress(void) {
  static int quit_times = KILO_QUIT_TIMES;
  int c = editorReadKey();

  switch (c) {
  case '\r':
    editorInsertNewline();
    break;

  case CTRL_KEY('q'):
    if (config.dirty && quit_times > 0) {
      editorSetStatusMessage("WARNING! File has unsaved changes. Press Ctrl-Q "
                             "%d more time%s to quit.",
                             quit_times, quit_times == 1 ? "" : "s");
      quit_times--;
      return;
    }
    clearDisplayForStandardOut();
    repositionCursorToTopLeft();
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    config.cx = 0;
    break;

  case END_KEY:
    if (config.cy < config.row_count) {
      config.cx = config.current_row[config.cy].size;
    }
    break;

  case BACKSPACE:
  case CTRL_KEY('h'):
  case DEL_KEY:
    if (c == DEL_KEY) {
      editorMoveCursor(ARROW_RIGHT);
    }
    editorDeleteChar();
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
    if (c == PAGE_UP) {
      config.cy = config.row_offset;
    } else if (c == PAGE_DOWN) {
      config.cy = config.row_offset + config.wsize.ws_row - 1;
      if (config.cy > config.row_count) {
        config.cy = config.row_count;
      }
    }

    int times = config.wsize.ws_row;
    int key = c == PAGE_UP ? ARROW_UP : ARROW_DOWN;
    while (times--) {
      editorMoveCursor(key);
    }
  } break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b':
    break;

  default:
    editorInsertChar(c);
    break;
  }

  quit_times = KILO_QUIT_TIMES;
}

void editorMoveCursor(int keypress) {
  EditorRow *row =
      (config.cy >= config.row_count) ? NULL : &config.current_row[config.cy];
  switch (keypress) {
  case ARROW_LEFT:
    if (config.cx != 0) {
      config.cx--;
    } else if (config.cy > 0) {
      config.cy--;
      config.cx = config.current_row[config.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && config.cx < row->size) {
      config.cx++;
    } else if (row && config.cx == row->size) {
      config.cy++;
      config.cx = 0;
    }
    break;
  case ARROW_UP:
    if (config.cy > 0) {
      config.cy--;
    }
    break;
  case ARROW_DOWN:
    if (config.cy < config.row_count) {
      config.cy++;
    }
    break;
  }

  row = (config.cy > config.row_count) ? NULL : &config.current_row[config.cy];
  int row_length = row ? row->size : 0;
  if (config.cx > row_length) {
    config.cx = row_length;
  }
}

int getCursorPosition(struct winsize *wsize) {
  char buf[32] = {0};
  unsigned int i = 0;

  // request the cursor position
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }

  // the reply is an escape sequence; read the characters into a buffer
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R') {
      break;
    }
    i++;
  }
  buf[i] = '\0';
  // validate message format
  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }

  if (sscanf(&buf[2], "%hu;%hu", &(wsize->ws_row), &(wsize->ws_col)) != 2) {
    return -1;
  }

  return 0;
}

char *editorPrompt(char *prompt) {
  size_t buffer_size = 128;
  char *buffer = malloc(buffer_size);

  size_t buffer_length = 0;
  buffer[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buffer);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buffer_length != 0) {
        buffer[--buffer_length] = '\0';
      }
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      free(buffer);
      return NULL;
    } else if (c == '\r') {
      if (buffer_length != 0) {
        editorSetStatusMessage("");
        return buffer;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buffer_length == buffer_size - 1) {
        // if we hit the end of the buffer, double the size
        buffer_size *= 2;
        buffer = realloc(buffer, buffer_size);
      }
      // append 'c' to the end of this buffer
      buffer[buffer_length++] = c;
      buffer[buffer_length] = '\0';
    }
  }
}
