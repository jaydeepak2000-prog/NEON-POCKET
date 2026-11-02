#pragma once
#include <Arduino.h>
namespace Config {
constexpr uint8_t OLED_SDA=8, OLED_SCL=9;
constexpr uint8_t AUDIO_PIN=12, MOTOR_PIN=13;
constexpr uint8_t CONTROL_PINS[]={4,5,6,7,10,11};
constexpr uint32_t DEBOUNCE_MS=25;
}
