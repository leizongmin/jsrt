#ifndef __JSRT_UTIL_COLORIZE_H__
#define __JSRT_UTIL_COLORIZE_H__

#include <stdio.h>

#define JSRT_ColorizeNoColor (-1)
#define JSRT_ColorizeBlack (0)
#define JSRT_ColorizeRed (1)
#define JSRT_ColorizeGreen (2)
#define JSRT_ColorizeYellow (3)
#define JSRT_ColorizeBlue (4)
#define JSRT_ColorizeMagenta (5)
#define JSRT_ColorizeCyan (6)
#define JSRT_ColorizeWhite (7)
#define JSRT_ColorizeBold (1)
#define JSRT_ColorizeUnderline (4)

#define JSRT_ColorizeClear "\033[0m"

#define JSRT_ColorizeFontBlack "\033[30m"
#define JSRT_ColorizeFontRed "\033[31m"
#define JSRT_ColorizeFontGreen "\033[32m"
#define JSRT_ColorizeFontYellow "\033[33m"
#define JSRT_ColorizeFontBlue "\033[34m"
#define JSRT_ColorizeFontMagenta "\033[35m"
#define JSRT_ColorizeFontCyan "\033[36m"
#define JSRT_ColorizeFontWhite "\033[37m"

#define JSRT_ColorizeFontBlackBold "\033[30;1m"
#define JSRT_ColorizeFontRedBold "\033[31;1m"
#define JSRT_ColorizeFontGreenBold "\033[32;1m"
#define JSRT_ColorizeFontYellowBold "\033[33;1m"
#define JSRT_ColorizeFontBlueBold "\033[34;1m"
#define JSRT_ColorizeFontMagentaBold "\033[35;1m"
#define JSRT_ColorizeFontCyanBold "\033[36;1m"
#define JSRT_ColorizeFontWhiteBold "\033[37;1m"

/**
 * Colorize terminal colors ANSI escape sequences.
 *
 * @param font font color (-1 to 7), see Colors enum
 * @param back background color (-1 to 7), see Colors enum
 * @param style font style (1==bold, 4==underline)
 **/
static const char* jsrt_colorize(int font, int back, int style) {
  static char code[128];

  font = (font < 0) ? 0 : font + 30;
  back = (back < 0) ? 0 : back + 40;
  style = (style < 0) ? 0 : style;

  if (back > 0 && style > 0) {
    snprintf(code, sizeof code, "\033[%d;%d;%dm", font, back, style);
  } else if (back > 0) {
    snprintf(code, sizeof code, "\033[%d;%dm", font, back);
  } else {
    snprintf(code, sizeof code, "\033[%dm", font);
  }

  return code;
}

#endif
