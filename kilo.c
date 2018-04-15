#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#pragma mark - Declarations

// process user input immediately on keypress, not after 'Enter' keypress
void enableRawMode(void);

// the terminal settings defined prior to us changing them; we should restore
// these at program exit.
struct termios original_termios;
void disableRawMode(void);

// print an error message and exit with a non-zero status.
void die(const char *message);

#pragma mark -

int main(void) {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
      die("could not read from STDIN.");
    }

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }

    if (c == 'q') {
      break;
    }
  }

  return 0;
}

#pragma mark - Configuration

void enableRawMode(void) {
  // schedule cleanup at exit
  if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
    die("failed to get current terminal attributes.");
  }
  atexit(disableRawMode);

  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);

  // further reading: https://linux.die.net/man/3/termios

  // disable echo: don't show user what they're typing; that's our job.
  raw.c_lflag &= ~(ECHO);
  // disable canonical mode: handle input byte-by-byte, not line-by-line
  raw.c_lflag &= ~(ICANON);
  // disable signals: don't terminate on Ctrl-C or Ctrl-Z
  raw.c_lflag &= ~(ISIG);
  // disable 'implementation-defined' input processing
  raw.c_lflag &= ~(IEXTEN);

  // disable software flow control
  raw.c_iflag &= ~(IXON);
  // disable translating \r into \n
  raw.c_iflag &= ~(ICRNL);
  // disable legacy input flags
  raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP);
  raw.c_cflag |= (CS8);

  // disable output post processing
  raw.c_oflag &= ~(OPOST);

  // set minimum number input bytes before `read` returns
  raw.c_cc[VMIN] = 0;
  // set maximum wait time before `read` returns in tenths of a second
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    die("failed to set updated terminal attributes.");
  }
}

void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1) {
    die("failed to restore original terminal attributes.");
  }
}

#pragma mark - Error Handling

void die(const char *message) {
  perror(message);
  exit(1);
}
