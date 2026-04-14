// FontData.h - 字模数据管理
#ifndef FONTDATA_H
#define FONTDATA_H

#include <Arduino.h>

// 字模高度定义
#define DIGIT_HEIGHT 7

// 点亮方向枚举
enum LightDirection {
  d_up,    // 从底部向上点亮
  d_down   // 从顶部向下点亮
};

// 数字字模 (5x7像素)
extern const uint8_t DIGIT_FONT[10][5];

// 大写字母字模 (5x7像素)
extern const uint8_t LETTER_FONT[26][5];

// 小写字母字模 (5x7像素)
extern const uint8_t LOWERCASE_FONT[26][5];

// 标点符号字模 (统一为1列宽度，组合成二维数组)
extern const uint8_t PUNCTUATION_FONT[6][1];  // 所有标点符号组合

// 标点符号索引定义
#define PUNCT_COLON_INDEX 0
#define PUNCT_EXCLAMATION_INDEX 1
#define PUNCT_SINGLE_QUOTE_INDEX 2
#define PUNCT_PERIOD_INDEX 3
#define PUNCT_DOT_INDEX 4
#define PUNCT_SPACE_INDEX 5

// 标点符号映射结构
struct PunctuationMap {
    char character;
    uint8_t fontIndex;
    uint8_t width;
};

extern const PunctuationMap PUNCTUATION_MAP[];
extern const uint8_t PUNCTUATION_MAP_COUNT;

// 字模工具函数
uint8_t getCharWidth(char c);
const uint8_t* getCharFontData(char c);

#endif