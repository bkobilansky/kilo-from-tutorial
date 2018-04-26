#include "editor.h"
#include "util.h"

#include <termios.h>
#include <unistd.h>

#pragma mark - Configuration

void disableRawMode(void) {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.original_termios) == -1) {
    die("failed to restore original terminal attributes.");
  }
}

// process user input immediately on keypress, not after 'Enter' keypress
void enableRawMode(void) {
  // schedule cleanup at exit
  if (tcgetattr(STDIN_FILENO, &config.original_termios) == -1) {
    die("failed to get current terminal attributes.");
  }
  atexit(disableRawMode);

  struct termios raw = config.original_termios;
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

#pragma mark -

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  // if we were passed an arg, assume it's a filename to open
  if (argc >= 2) {
    openEditor(argv[1]);
  }

  // the default status message indicates control keys
  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  // main loop
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
