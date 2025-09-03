/*
 * Minimal readline replacement for systems without readline
 * Provides basic line editing and history functionality
 */

#include "minimal_readline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <conio.h>
#define STDIN_FILENO 0
#define isatty _isatty
#else
#include <termios.h>
#include <unistd.h>
#endif

// Simple history implementation
#define MAX_HISTORY 1000
static char* history[MAX_HISTORY];
static int history_count = 0;
static int history_pos = 0;

// Readline state
char* rl_line_buffer = NULL;
int rl_point = 0;
int rl_end = 0;
int rl_done = 0;
const char* rl_prompt = NULL;

// Terminal settings
#ifndef _WIN32
static struct termios orig_termios;
#else
static DWORD orig_console_mode;
static HANDLE console_handle;
#endif
static int terminal_initialized = 0;

// Initialize terminal for raw mode
static void init_terminal(void) {
  if (terminal_initialized)
    return;

#ifdef _WIN32
  console_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (console_handle == INVALID_HANDLE_VALUE) {
    return;
  }
  
  if (!GetConsoleMode(console_handle, &orig_console_mode)) {
    return;
  }
  
  DWORD new_mode = orig_console_mode;
  new_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
  
  if (!SetConsoleMode(console_handle, new_mode)) {
    return;
  }
#else
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    return;
  }

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
    return;
  }
#endif

  terminal_initialized = 1;
}

// Restore terminal settings
static void restore_terminal(void) {
  if (!terminal_initialized)
    return;
    
#ifdef _WIN32
  SetConsoleMode(console_handle, orig_console_mode);
#else
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
#endif
  terminal_initialized = 0;
}

// Add line to history
void add_history(const char* line) {
  if (!line || strlen(line) == 0)
    return;

  // Don't add duplicate consecutive entries
  if (history_count > 0 && strcmp(history[history_count - 1], line) == 0) {
    return;
  }

  // Remove oldest entry if at capacity
  if (history_count >= MAX_HISTORY) {
    free(history[0]);
    memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(char*));
    history_count--;
  }

  history[history_count] = strdup(line);
  history_count++;
  history_pos = history_count;
}

// Read history from file (stub implementation)
int read_history(const char* filename) {
  (void)filename;  // Unused
  return 0;
}

// Write history to file (stub implementation)
int write_history(const char* filename) {
  (void)filename;  // Unused
  return 0;
}

// Clear screen
void rl_clear_screen(int count, int key) {
  (void)count;
  (void)key;
  printf("\033[2J\033[H");
  fflush(stdout);
}

// Prepare for new line
void rl_on_new_line(void) {
  // Reset state for new line
  rl_point = 0;
  rl_end = 0;
  rl_done = 0;
}

// Force redisplay (stub)
void rl_forced_update_display(void) {
  // In minimal implementation, we don't redraw
}

// Redisplay current line (stub)
void rl_redisplay(void) {
  // In minimal implementation, we don't redraw
}

// Simple readline implementation
char* readline(const char* prompt) {
  if (prompt) {
    printf("%s", prompt);
    fflush(stdout);
    rl_prompt = prompt;
  }

  // For CI environments or non-interactive terminals, use simple fgets
  if (!isatty(STDIN_FILENO)) {
    char* line = malloc(1024);
    if (!line)
      return NULL;

    if (!fgets(line, 1024, stdin)) {
      free(line);
      return NULL;
    }

    // Remove trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    return line;
  }

  // For interactive terminals, use minimal line editing
  init_terminal();

  char* line = malloc(1024);
  if (!line)
    return NULL;

  int pos = 0;
  int len = 0;

  while (1) {
    char c;
#ifdef _WIN32
    DWORD chars_read;
    if (!ReadConsole(console_handle, &c, 1, &chars_read, NULL) || chars_read != 1) {
      break;
    }
#else
    if (read(STDIN_FILENO, &c, 1) != 1) {
      break;
    }
#endif

    if (c == '\n' || c == '\r') {
      // Enter pressed
      printf("\n");
      break;
    } else if (c == 127 || c == 8) {  // Backspace
      if (pos > 0) {
        pos--;
        len--;
        printf("\b \b");
        fflush(stdout);
      }
    } else if (c == 4) {  // Ctrl-D (EOF)
      if (len == 0) {
        free(line);
        restore_terminal();
        return NULL;
      }
    } else if (c >= 32 && c < 127) {  // Printable characters
      if (len < 1023) {
        line[pos] = c;
        pos++;
        len++;
        printf("%c", c);
        fflush(stdout);
      }
    }
    // Ignore other control characters for simplicity
  }

  line[len] = '\0';
  restore_terminal();

  // Update readline state
  if (rl_line_buffer)
    free(rl_line_buffer);
  rl_line_buffer = strdup(line);
  rl_point = len;
  rl_end = len;

  return line;
}