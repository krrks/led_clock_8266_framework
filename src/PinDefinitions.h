// PinDefinitions.h
#ifndef PINDEFINITIONS_H
#define PINDEFINITIONS_H

#include <Arduino.h>

// 物理按钮引脚 (GPIO5) D1
#define BUTTON_PIN 5

// 板载LED状态灯引脚 (GPIO2) D4
#define STATUS_LED_PIN 2

// WS2812点阵屏数据引脚 (GPIO4) D2
#define LED_MATRIX_PIN 4

// 点阵屏参数
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT) // 从LEDMatrixLayout.h引入

#endif