/**
 * Module Content Analyzer - Implementation
 *
 * Performs simple lexical analysis to detect module format patterns.
 * This is NOT a full parser - just enough to detect common patterns.
 */

#include "content_analyzer.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include "../util/module_debug.h"

/**
 * Lexer state for content analysis
 */
typedef struct {
  const char* content;
  size_t length;
  size_t pos;
  bool has_esm_pattern;
  bool has_cjs_pattern;
} LexerState;

/**
 * Check if character is whitespace
 */
static inline bool is_whitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Check if character is valid identifier start
 */
static inline bool is_identifier_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

/**
 * Check if character is valid identifier part
 */
static inline bool is_identifier_part(char c) {
  return is_identifier_start(c) || (c >= '0' && c <= '9');
}

/**
 * Peek at current character without advancing
 */
static inline char peek(LexerState* state) {
  if (state->pos >= state->length) {
    return '\0';
  }
  return state->content[state->pos];
}

/**
 * Peek at next character without advancing
 */
static inline char peek_next(LexerState* state) {
  if (state->pos + 1 >= state->length) {
    return '\0';
  }
  return state->content[state->pos + 1];
}

/**
 * Advance to next character
 */
static inline void advance(LexerState* state) {
  if (state->pos < state->length) {
    state->pos++;
  }
}

/**
 * Skip single-line comment (//)
 */
static void skip_line_comment(LexerState* state) {
  // Already at //, skip to end of line
  while (peek(state) != '\0' && peek(state) != '\n') {
    advance(state);
  }
}

/**
 * Skip multi-line comment (/* *\/)
 */
static void skip_block_comment(LexerState* state) {
  // Already at /*, skip to *\/
  while (peek(state) != '\0') {
    if (peek(state) == '*' && peek_next(state) == '/') {
      advance(state);  // skip *
      advance(state);  // skip /
      return;
    }
    advance(state);
  }
}

/**
 * Skip string literal (single, double, or template)
 */
static void skip_string(LexerState* state, char quote) {
  advance(state);  // skip opening quote

  while (peek(state) != '\0') {
    char c = peek(state);

    // Handle escape sequences
    if (c == '\\') {
      advance(state);  // skip backslash
      if (peek(state) != '\0') {
        advance(state);  // skip escaped char
      }
      continue;
    }

    // Check for closing quote
    if (c == quote) {
      advance(state);  // skip closing quote
      return;
    }

    // For template literals, handle nested ${}
    if (quote == '`' && c == '$' && peek_next(state) == '{') {
      advance(state);  // skip $
      advance(state);  // skip {

      // Skip until matching }
      int depth = 1;
      while (peek(state) != '\0' && depth > 0) {
        c = peek(state);
        if (c == '{') {
          depth++;
        } else if (c == '}') {
          depth--;
        } else if (c == '"' || c == '\'' || c == '`') {
          skip_string(state, c);
          continue;
        }
        advance(state);
      }
      continue;
    }

    advance(state);
  }
}

/**
 * Skip whitespace and comments
 */
static void skip_whitespace_and_comments(LexerState* state) {
  while (true) {
    char c = peek(state);

    // Skip whitespace
    if (is_whitespace(c)) {
      advance(state);
      continue;
    }

    // Skip comments
    if (c == '/' && peek_next(state) == '/') {
      skip_line_comment(state);
      continue;
    }

    if (c == '/' && peek_next(state) == '*') {
      advance(state);  // skip /
      advance(state);  // skip *
      skip_block_comment(state);
      continue;
    }

    break;
  }
}

/**
 * Check if keyword matches at current position
 * Returns true if match and advances position
 */
static bool match_keyword(LexerState* state, const char* keyword) {
  size_t kw_len = strlen(keyword);

  // Check if we have enough characters left
  if (state->pos + kw_len > state->length) {
    return false;
  }

  // Check if keyword matches
  if (strncmp(&state->content[state->pos], keyword, kw_len) != 0) {
    return false;
  }

  // Check that keyword is not part of a longer identifier
  if (state->pos + kw_len < state->length) {
    char next_char = state->content[state->pos + kw_len];
    if (is_identifier_part(next_char)) {
      return false;
    }
  }

  // Match! Advance position
  state->pos += kw_len;
  return true;
}

/**
 * Analyze content for module patterns
 */
JSRT_ModuleFormat jsrt_analyze_content_format(const char* content, size_t length) {
  if (!content || length == 0) {
    MODULE_DEBUG_DETECTOR("No content to analyze");
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  MODULE_DEBUG_DETECTOR("Analyzing content (%zu bytes)", length);

  // Initialize lexer state
  LexerState state = {
      .content = content, .length = length, .pos = 0, .has_esm_pattern = false, .has_cjs_pattern = false};

  // Scan through content
  while (state.pos < state.length) {
    skip_whitespace_and_comments(&state);

    char c = peek(&state);
    if (c == '\0') {
      break;
    }

    // Skip string literals
    if (c == '"' || c == '\'' || c == '`') {
      skip_string(&state, c);
      continue;
    }

    // Check for ESM keywords: import, export
    if (match_keyword(&state, "import")) {
      MODULE_DEBUG_DETECTOR("Found 'import' keyword");
      state.has_esm_pattern = true;
      continue;
    }

    if (match_keyword(&state, "export")) {
      MODULE_DEBUG_DETECTOR("Found 'export' keyword");
      state.has_esm_pattern = true;
      continue;
    }

    // Check for CommonJS patterns: require(, module.exports, exports.
    if (match_keyword(&state, "require")) {
      skip_whitespace_and_comments(&state);
      if (peek(&state) == '(') {
        MODULE_DEBUG_DETECTOR("Found 'require(' pattern");
        state.has_cjs_pattern = true;
      }
      continue;
    }

    if (match_keyword(&state, "module")) {
      skip_whitespace_and_comments(&state);
      if (peek(&state) == '.') {
        advance(&state);  // skip .
        if (match_keyword(&state, "exports")) {
          MODULE_DEBUG_DETECTOR("Found 'module.exports' pattern");
          state.has_cjs_pattern = true;
        }
      }
      continue;
    }

    if (match_keyword(&state, "exports")) {
      skip_whitespace_and_comments(&state);
      if (peek(&state) == '.') {
        MODULE_DEBUG_DETECTOR("Found 'exports.' pattern");
        state.has_cjs_pattern = true;
      }
      continue;
    }

    // Not a keyword we care about, skip this character
    advance(&state);
  }

  // Determine format based on patterns found
  // Prefer ESM if both patterns detected (modern code often has both)
  if (state.has_esm_pattern) {
    MODULE_DEBUG_DETECTOR("Content analysis result: ESM (import/export found)");
    return JSRT_MODULE_FORMAT_ESM;
  }

  if (state.has_cjs_pattern) {
    MODULE_DEBUG_DETECTOR("Content analysis result: CommonJS (require/module.exports found)");
    return JSRT_MODULE_FORMAT_COMMONJS;
  }

  MODULE_DEBUG_DETECTOR("Content analysis result: Unknown (no patterns found)");
  return JSRT_MODULE_FORMAT_UNKNOWN;
}
