#ifndef LEDMATRIXLAYOUT_H
#define LEDMATRIXLAYOUT_H

#include <Arduino.h>

// 物理布局参数
#define MATRIX_WIDTH  32    // 点阵屏总宽度（列）
#define MATRIX_HEIGHT 8     // 点阵屏总高度（行）
#define NUM_LEDS      (MATRIX_WIDTH * MATRIX_HEIGHT) // LED总数

// 将二维显示矩阵转换为列蛇形顺序的一维数组
inline void convertToSnakeOrder(uint32_t displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH], uint32_t* ledBuffer) {
    for (uint8_t col = 0; col < MATRIX_WIDTH; col++) {
        for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
            // 列蛇形排列：偶数列从下到上，奇数列从上到下
            uint8_t physicalRow = (col % 2 == 0) ?  (MATRIX_HEIGHT - 1 - row):row ;
            uint16_t index = col * MATRIX_HEIGHT + physicalRow;
            
            if (index < NUM_LEDS) {
                ledBuffer[index] = displayMatrix[row][col];
            }
        }
    }
}

// 检查像素坐标是否有效
inline bool isValidPixelCoordinate(uint8_t x, uint8_t y) {
    return (x < MATRIX_WIDTH && y < MATRIX_HEIGHT);
}

#endif

