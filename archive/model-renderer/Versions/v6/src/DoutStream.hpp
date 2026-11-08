#ifndef DOUSTREAM_HPP
#define DOUSTREAM_HPP
#include <gint/gint.h>
#include <gint/display.h>
#include <string>
#include <sstream>
#include <iostream> // pour std::ostringstream


namespace casio
{
    struct DoutEnd {};

    inline DoutEnd end;

    class DoutStream {
    public:
        DoutStream(int x, int y, int color)
            : startX(x), curX(x), curY(y), color(color) {}

        template<typename T>
        DoutStream& operator<<(const T& value) {
            std::ostringstream oss;
            oss << value;
            processText(oss.str());
            return *this;
        }

        DoutStream& operator<<(DoutEnd) {
            // Force une nouvelle ligne
            newline();
            return *this;
        }

    private:
        int startX;
        int curX;
        int curY;
        int color;

        void processText(const std::string& text) {
            std::string line;
            for (char ch : text) {
                if (ch == '\n') {
                    if (!line.empty()) draw(line);
                    newline();
                    line.clear();
                } else {
                    line += ch;
                }
            }
            if (!line.empty()) draw(line);
        }

        void draw(const std::string& text) {
            dtext(curX, curY, color, text.c_str());

            int w = 0, h = 0;
            dsize(text.c_str(), dfont_default(), &w, &h);
            curX += w; // Avancer horizontalement
        }

        void newline() {
            curX = startX;
            int h = 0;
            dsize("A", dfont_default(), nullptr, &h); // Hauteur de ligne
            curY += h + 1;
        }
    };

    // Interface comme std::cout
    struct dout_creator {
        DoutStream operator()(int x, int y, int color) {
            return DoutStream(x, y, color);
        }
    };

    inline dout_creator dout;

    inline void formatFloat(float val, char* out) {
        int sign = val < 0 ? -1 : 1;
        val = val < 0 ? -val : val;
    
        int int_part = static_cast<int>(val);
        int dec_part = static_cast<int>((val - int_part) * 1000 + 0.5f); // arrondi
    
        sprintf(out, "%s%d.%03d", (sign < 0 ? "-" : ""), int_part, dec_part);
    }
    
    inline DoutStream& operator<<(casio::DoutStream& out, float val) {
        char buf[32]; // Assez large pour contenir le float formaté
        formatFloat(val, buf);
        return out << buf;
    }
    
    inline DoutStream& operator<<(casio::DoutStream& out, double val) {
        char buf[32]; // Assez large pour contenir le double formaté
        formatFloat(static_cast<float>(val), buf);
        return out << buf;
    }
}


#endif