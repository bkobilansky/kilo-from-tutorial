#include "editor.h"
#include "editor-key.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k)&0x1f)

struct EditorConfig config;

void initEditor(void) {
  config.cx = 0;
  config.cy = 0;

  if (editorGetWindowSize(&config.wsize) == -1) {
    die("could not get editor window size.");
  }
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
    // print welcome message 1/3 of the way down the page
    if (y == config.wsize.ws_row / 3) {
      char welcome[80];
      int welcome_length = snprintf(welcome, sizeof(welcome),
                                    "Kilo editor -- version %s", KILO_VERSION);
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

    // 'ERASE IN LINE': clear each line as we redraw it
    append_buffer_append(ab, "\x1b[K", 3);

    if (y < config.wsize.ws_row - 1) {
      append_buffer_append(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct append_buffer ab = append_buffer_init;
  repositionCursorToTopLeft();
  hideCursorWhileWriting();

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", config.cy + 1, config.cx + 1);
  append_buffer_append(&ab, buf, strlen(buf));

  repositionCursorToTopLeft();
  displayCursorAfterWriting();

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
  int c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    clearDisplayForStandardOut();
    repositionCursorToTopLeft();
    exit(0);
    break;

  case HOME_KEY:
    config.cx = 0;
    break;
  case END_KEY:
    config.cx = config.wsize.ws_col - 1;
    break;

  case PAGE_UP:
  case PAGE_DOWN: {
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
  }
}

void editorMoveCursor(int keypress) {
  switch (keypress) {
  case ARROW_LEFT:
    if (config.cx != 0) {
      config.cx--;
    }
    break;
  case ARROW_RIGHT:
    if (config.cx < config.wsize.ws_col - 1) {
      config.cx++;
    }
    break;
  case ARROW_UP:
    if (config.cy > 0) {
      config.cy--;
    }
    break;
  case ARROW_DOWN:
    if (config.cy < config.wsize.ws_row - 1) {
      config.cy++;
    }
    break;
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
