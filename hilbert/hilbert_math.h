#pragma once
#include <cstdint>

namespace Hilbert {

    // Размер стороны квадрата для маски /24 (2^12 = 4096)
    const uint32_t MAP_SIZE = 4096;

    // Перевод из одномерного индекса (ID подсети) в 2D координаты (X, Y)
    void indexToXY(uint32_t index, uint32_t& x, uint32_t& y);

    // Обратный перевод: из координат (X, Y) в одномерный индекс
    uint32_t xyToIndex(uint32_t x, uint32_t y);

}