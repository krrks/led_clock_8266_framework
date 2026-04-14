#ifndef BUTTONHANDLER_H
#define BUTTONHANDLER_H

#include <Arduino.h>
#include <functional>

// 按钮事件类型枚举
enum ButtonEvent {
    BE_NONE,        // 无事件
    BE_CLICK,       // 单击
    BE_DOUBLE_CLICK,// 双击
    BE_LONG_PRESS,  // 长按
    BE_VERY_LONG_PRESS, // 超长按（5秒）
    BE_PRESS_START, // 按下开始
    BE_PRESS_END    // 按下结束
};

// 按钮状态枚举
enum ButtonState {
    BS_IDLE,        // 空闲状态
    BS_PRESSED,     // 已按下
    BS_DEBOUNCING,  // 去抖动中
    BS_WAIT_FOR_CLICK // 等待第二次点击（双击检测）
};

// 按钮配置结构体
struct ButtonConfig {
    uint8_t pin;                // 按钮引脚
    uint8_t activeLevel;        // 激活电平（通常为0，表示低电平有效）
    uint16_t debounceMs;        // 去抖动时间（毫秒）
    uint16_t clickTimeoutMs;    // 单击超时时间（毫秒）
    uint16_t doubleClickGapMs;  // 双击间隔时间（毫秒）
    uint16_t longPressMs;       // 长按时间阈值（毫秒）
    uint16_t veryLongPressMs;   // 超长按时间阈值（毫秒，5秒=5000ms）
};

// 按钮回调函数类型
typedef std::function<void(ButtonEvent event, uint32_t duration)> ButtonCallback;

class ButtonHandler {
public:
    /**
     * @brief 构造函数（使用默认配置）
     * @param pin 按钮引脚
     */
    ButtonHandler(uint8_t pin);
    
    /**
     * @brief 构造函数（使用自定义配置）
     * @param config 按钮配置
     */
    ButtonHandler(const ButtonConfig& config);
    
    /**
     * @brief 初始化按钮处理器
     */
    void begin();
    
    /**
     * @brief 更新按钮状态（必须在主循环中定期调用）
     */
    void update();
    
    /**
     * @brief 获取当前按钮事件
     * @return 按钮事件
     */
    ButtonEvent getEvent();
    
    /**
     * @brief 清除当前事件
     */
    void clearEvent();
    
    /**
     * @brief 获取当前按钮状态
     * @return 按钮状态
     */
    ButtonState getState();
    
    /**
     * @brief 获取当前按下的持续时间
     * @return 持续时间（毫秒）
     */
    uint32_t getPressDuration();
    
    /**
     * @brief 检查按钮是否处于按下状态
     * @return true如果按钮被按下，否则false
     */
    bool isPressed();
    
    /**
     * @brief 设置按钮事件回调函数
     * @param callback 回调函数
     */
    void setCallback(ButtonCallback callback);
    
    /**
     * @brief 设置按钮配置
     * @param config 新的按钮配置
     */
    void setConfig(const ButtonConfig& config);
    
    /**
     * @brief 获取当前按钮配置
     * @return 按钮配置
     */
    ButtonConfig getConfig() const;
    
    /**
     * @brief 重置按钮状态机
     */
    void reset();
    
    /**
     * @brief 启用/禁用按钮处理
     * @param enabled true启用，false禁用
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief 检查按钮处理器是否已启用
     * @return true如果启用，否则false
     */
    bool isEnabled() const;

private:
    ButtonConfig _config;           // 按钮配置
    ButtonState _state;             // 当前状态
    ButtonEvent _currentEvent;      // 当前事件
    uint32_t _lastDebounceTime;     // 上次去抖动时间
    uint32_t _pressStartTime;       // 按下开始时间
    uint32_t _lastClickTime;        // 上次点击时间
    uint8_t _clickCount;            // 点击计数（用于双击检测）
    bool _enabled;                  // 是否启用
    ButtonCallback _callback;       // 事件回调函数
    
    /**
     * @brief 读取按钮当前物理状态
     * @return 按钮电平状态
     */
    uint8_t readButton();
    
    /**
     * @brief 处理状态机
     */
    void processStateMachine();
    
    /**
     * @brief 触发事件
     * @param event 事件类型
     * @param duration 事件持续时间（毫秒）
     */
    void triggerEvent(ButtonEvent event, uint32_t duration = 0);
    
    /**
     * @brief 应用默认配置
     */
    void applyDefaultConfig();
};

#endif // BUTTONHANDLER_H