#include "ButtonHandler.h"

// 默认配置
const ButtonConfig DEFAULT_BUTTON_CONFIG = {
    .pin = 5,                       // GPIO5 (D1)
    .activeLevel = 0,               // 低电平有效
    .debounceMs = 50,               // 50ms去抖动
    .clickTimeoutMs = 300,          // 300ms单击超时
    .doubleClickGapMs = 500,        // 500ms双击间隔
    .longPressMs = 1000,            // 1000ms长按阈值
    .veryLongPressMs = 5000         // 5000ms超长按阈值（5秒）
};

ButtonHandler::ButtonHandler(uint8_t pin) {
    _config = DEFAULT_BUTTON_CONFIG;
    _config.pin = pin;
    _state = BS_IDLE;
    _currentEvent = BE_NONE;
    _enabled = true;
    _clickCount = 0;
}

ButtonHandler::ButtonHandler(const ButtonConfig& config) {
    _config = config;
    _state = BS_IDLE;
    _currentEvent = BE_NONE;
    _enabled = true;
    _clickCount = 0;
}

void ButtonHandler::begin() {
    pinMode(_config.pin, INPUT_PULLUP);
    reset();
}

void ButtonHandler::update() {
    if (!_enabled) {
        return;
    }
    
    processStateMachine();
}

ButtonEvent ButtonHandler::getEvent() {
    ButtonEvent event = _currentEvent;
    _currentEvent = BE_NONE; // 读取后清除
    return event;
}

void ButtonHandler::clearEvent() {
    _currentEvent = BE_NONE;
}

ButtonState ButtonHandler::getState() {
    return _state;
}

uint32_t ButtonHandler::getPressDuration() {
    if (_state == BS_PRESSED) {
        return millis() - _pressStartTime;
    }
    return 0;
}

bool ButtonHandler::isPressed() {
    return _state == BS_PRESSED;
}

void ButtonHandler::setCallback(ButtonCallback callback) {
    _callback = callback;
}

void ButtonHandler::setConfig(const ButtonConfig& config) {
    _config = config;
    reset();
}

ButtonConfig ButtonHandler::getConfig() const {
    return _config;
}

void ButtonHandler::reset() {
    _state = BS_IDLE;
    _currentEvent = BE_NONE;
    _clickCount = 0;
    _lastDebounceTime = 0;
    _pressStartTime = 0;
    _lastClickTime = 0;
}

void ButtonHandler::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        reset();
    }
}

bool ButtonHandler::isEnabled() const {
    return _enabled;
}

uint8_t ButtonHandler::readButton() {
    return digitalRead(_config.pin);
}

void ButtonHandler::processStateMachine() {
    uint8_t buttonState = readButton();
    uint32_t currentTime = millis();
    uint32_t pressDuration = 0;
    
    switch (_state) {
        case BS_IDLE:
            // 检查按钮是否被按下
            if (buttonState == _config.activeLevel) {
                _state = BS_DEBOUNCING;
                _lastDebounceTime = currentTime;
            }
            break;
            
        case BS_DEBOUNCING:
            // 等待去抖动时间
            if (currentTime - _lastDebounceTime >= _config.debounceMs) {
                // 再次检查按钮状态
                if (buttonState == _config.activeLevel) {
                    // 确实按下了
                    _state = BS_PRESSED;
                    _pressStartTime = currentTime;
                    triggerEvent(BE_PRESS_START, 0);
                } else {
                    // 误触发，返回空闲状态
                    _state = BS_IDLE;
                }
            }
            break;
            
        case BS_PRESSED:
            // 检查按钮是否释放
            if (buttonState != _config.activeLevel) {
                // 按钮释放
                _state = BS_DEBOUNCING;
                _lastDebounceTime = currentTime;
                pressDuration = currentTime - _pressStartTime;
                triggerEvent(BE_PRESS_END, pressDuration);
                
                // 判断是单击还是长按
                if (pressDuration < _config.longPressMs) {
                    // 可能是单击，需要等待判断是否为双击
                    _state = BS_WAIT_FOR_CLICK;
                    _lastClickTime = currentTime;
                    _clickCount = 1;
                } else if (pressDuration >= _config.longPressMs && pressDuration < _config.veryLongPressMs) {
                    // 长按事件
                    triggerEvent(BE_LONG_PRESS, pressDuration);
                } else if (pressDuration >= _config.veryLongPressMs) {
                    // 超长按事件（5秒）
                    triggerEvent(BE_VERY_LONG_PRESS, pressDuration);
                }
            } else {
                // 按钮仍然被按着，检查是否达到长按或超长按阈值
                pressDuration = currentTime - _pressStartTime;
                
                // 检查长按阈值（只触发一次）
                static bool longPressTriggered = false;
                if (!longPressTriggered && pressDuration >= _config.longPressMs) {
                    if (pressDuration < _config.veryLongPressMs) {
                        triggerEvent(BE_LONG_PRESS, pressDuration);
                    }
                    longPressTriggered = true;
                }
                
                // 检查超长按阈值（5秒）
                static bool veryLongPressTriggered = false;
                if (!veryLongPressTriggered && pressDuration >= _config.veryLongPressMs) {
                    triggerEvent(BE_VERY_LONG_PRESS, pressDuration);
                    veryLongPressTriggered = true;
                }
                
                // 如果按钮释放，重置标记
                if (buttonState != _config.activeLevel) {
                    longPressTriggered = false;
                    veryLongPressTriggered = false;
                }
            }
            break;
            
        case BS_WAIT_FOR_CLICK:
            // 等待可能的第二次点击（双击）
            if (currentTime - _lastClickTime > _config.doubleClickGapMs) {
                // 超时，确定为单击
                if (_clickCount == 1) {
                    triggerEvent(BE_CLICK, 0);
                }
                _state = BS_IDLE;
                _clickCount = 0;
            } else if (buttonState == _config.activeLevel) {
                // 检测到第二次按下
                _state = BS_DEBOUNCING;
                _lastDebounceTime = currentTime;
                _clickCount++;
                
                // 如果是第二次点击，触发双击事件
                if (_clickCount == 2) {
                    _state = BS_IDLE;
                    triggerEvent(BE_DOUBLE_CLICK, 0);
                    _clickCount = 0;
                }
            }
            break;
    }
}

void ButtonHandler::triggerEvent(ButtonEvent event, uint32_t duration) {
    _currentEvent = event;
    
    // 调用回调函数（如果设置了）
    if (_callback) {
        _callback(event, duration);
    }
    
    // 打印调试信息
    #ifdef DEBUG_BUTTON
    const char* eventNames[] = {
        "NONE", "CLICK", "DOUBLE_CLICK", "LONG_PRESS", 
        "VERY_LONG_PRESS", "PRESS_START", "PRESS_END"
    };
    
    Serial.print("Button Event: ");
    Serial.print(eventNames[event]);
    if (duration > 0) {
        Serial.print(" (");
        Serial.print(duration);
        Serial.print("ms)");
    }
    Serial.println();
    #endif
}

void ButtonHandler::applyDefaultConfig() {
    _config = DEFAULT_BUTTON_CONFIG;
}