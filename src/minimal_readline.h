/*
 * Minimal readline replacement header
 * Provides compatibility with readline API for systems without readline
 */

#ifndef MINIMAL_READLINE_H
#define MINIMAL_READLINE_H

#ifdef __cplusplus
extern "C" {
#endif

// Readline variables
extern char* rl_line_buffer;
extern int rl_point;
extern int rl_end;
extern int rl_done;
extern const char* rl_prompt;

// Main readline function
char* readline(const char* prompt);

// History functions
void add_history(const char* line);
int read_history(const char* filename);
int write_history(const char* filename);

// Display functions
void rl_clear_screen(int count, int key);
void rl_on_new_line(void);
void rl_forced_update_display(void);
void rl_redisplay(void);

#ifdef __cplusplus
}
#endif

#endif /* MINIMAL_READLINE_H */