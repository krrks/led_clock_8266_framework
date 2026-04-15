#ifndef LEDMATRIXLAYOUT_H
#define LEDMATRIXLAYOUT_H

#include <Arduino.h>

// Physical layout parameters
#define MATRIX_WIDTH  32
#define MATRIX_HEIGHT 8
#define NUM_LEDS      (MATRIX_WIDTH * MATRIX_HEIGHT)

/**
 * Convert 2-D display matrix (row, col) → 1-D column-snake LED buffer.
 *
 * Physical wiring starts at the bottom-right corner of the matrix.
 * Columns therefore run right-to-left from the viewer's perspective,
 * so we mirror the column index to correct the left-right flip.
 *
 * Even physical columns ascend  (LED 0 = physical bottom → display top).
 * Odd  physical columns descend (LED 0 = physical top    → display bottom).
 */
inline void convertToSnakeOrder(uint32_t displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                                 uint32_t* ledBuffer)
{
    for (uint8_t col = 0; col < MATRIX_WIDTH; col++) {
        // Mirror: physical column 0 shows display column 31, and vice-versa
        uint8_t displayCol = (MATRIX_WIDTH - 1) - col;

        for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
            // Snake parity is based on the physical column
            uint8_t physicalRow = (col % 2 == 0)
                                  ? (MATRIX_HEIGHT - 1 - row)
                                  : row;
            uint16_t index = col * MATRIX_HEIGHT + physicalRow;

            if (index < NUM_LEDS) {
                ledBuffer[index] = displayMatrix[row][displayCol];
            }
        }
    }
}

inline bool isValidPixelCoordinate(uint8_t x, uint8_t y) {
    return (x < MATRIX_WIDTH && y < MATRIX_HEIGHT);
}

#endif
