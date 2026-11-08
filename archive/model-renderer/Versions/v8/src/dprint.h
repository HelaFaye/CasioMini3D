#ifndef DPRINT_H
#define DPRINT_H
#include <gint/gint.h>
#include <gint/display.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

inline void formatFloat(float val, char* out, int precision)
{
    int sign = val < 0 ? -1 : 1;
    val = val < 0 ? -val : val;

    int int_part = (int)val;
    int dec_part = (int)((val - int_part) * pow(10,precision) + 0.5f); // arrondi

    sprintf(out, "%s%d.%03d", (sign < 0 ? "-" : ""), int_part, dec_part);
}

void dprint(int x, int y, int color, const char* fmt, ...) {
    char buffer[256] = {0};
    int cx = x;
    int cy = y;

    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;

            int precision = -1;
            if (*fmt == '.') {
                fmt++;
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }

            char temp[64] = {0};

            switch (*fmt) {
                case 'd':
                case 'i':
                    sprintf(temp, "%d", va_arg(args, int));
                    break;
                case 'u':
                    sprintf(temp, "%u", va_arg(args, unsigned int));
                    break;
                case 'x':
                    sprintf(temp, "%x", va_arg(args, unsigned int));
                    break;
                case 'l':
                    fmt++;
                    if (*fmt == 'd')
                        sprintf(temp, "%ld", va_arg(args, long));
                    else if (*fmt == 'u')
                        sprintf(temp, "%lu", va_arg(args, unsigned long));
                    break;
                case 'z':
                    fmt++;
                    if (*fmt == 'u')
                        sprintf(temp, "%zu", va_arg(args, size_t));
                    break;
                case 'f': {
                    double f = va_arg(args, double);
                    if (precision == -1)
                        precision = 3;
                    formatFloat(f, temp, precision);
                    break;
                }
                case 's':
                    snprintf(temp, sizeof(temp), "%s", va_arg(args, char*));
                    break;
                case 'p':
                    sprintf(temp, "%p", va_arg(args, void*));
                    break;
                case 'b':
                    snprintf(temp, sizeof(temp), "%s", va_arg(args, int) ? "true" : "false");
                    break;
                case 'c':
                    snprintf(temp, sizeof(temp), "%c", va_arg(args, int));
                    break;
                case '%':
                    strcpy(temp, "%");
                    break;
                default:
                    snprintf(temp, sizeof(temp), "%%%c", *fmt); // Unknown format
                    break;
            }

            dtext(cx, cy, color, temp);
            int w = 0, h = 0;
            dsize(temp, dfont_default(), &w, &h);
            cx += w;
        } else if (*fmt == '\n') {
            int h;
            dsize("A", dfont_default(), NULL, &h);
            cx = x;
            cy += h;
        } else {
            buffer[0] = *fmt;
            buffer[1] = '\0';
            dtext(cx, cy, color, buffer);
            int w = 0, h = 0;
            dsize(buffer, dfont_default(), &w, &h);
            cx += w;
        }
        fmt++;
    }

    va_end(args);
}

 #endif // DPRINT_H