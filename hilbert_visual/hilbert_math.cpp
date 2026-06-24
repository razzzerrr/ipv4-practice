#include "hilbert_math.h"
#include <algorithm>

namespace Hilbert {

    // Вспомогательная функция для поворота и отражения квадрантов кривой
    void rotate(uint32_t n, uint32_t& x, uint32_t& y, uint32_t rx, uint32_t ry) {
        if (ry == 0) {
            if (rx == 1) {
                x = n - 1 - x;
                y = n - 1 - y;
            }
            // Меняем местами X и Y
            std::swap(x, y);
        }
    }

    // Реализация перевода: Индекс -> (X, Y)
    void indexToXY(uint32_t index, uint32_t& x, uint32_t& y) {
        uint32_t rx, ry, s;
        uint32_t t = index;
        x = 0;
        y = 0;

        // Идем по всем уровням фрактала (от 1 до MAP_SIZE)
        for (s = 1; s < MAP_SIZE; s *= 2) {
            rx = 1 & (t / 2);
            ry = 1 & (t ^ rx);
            rotate(s, x, y, rx, ry);
            x += s * rx;
            y += s * ry;
            t /= 4;
        }
    }

    // Реализация обратного перевода: (X, Y) -> Индекс
    uint32_t xyToIndex(uint32_t x, uint32_t y) {
        uint32_t rx, ry, s;
        uint32_t index = 0;

        for (s = MAP_SIZE / 2; s > 0; s /= 2) {
            rx = (x & s) > 0;
            ry = (y & s) > 0;
            index += s * s * ((3 * rx) ^ ry);
            rotate(s, x, y, rx, ry);
        }
        return index;
    }

}