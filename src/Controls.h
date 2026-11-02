#pragma once
#include "Settings.h"
constexpr auto &PINS=Config::CONTROL_PINS;
struct Button {
  bool raw=false,down=false,changed=false;
  uint32_t since=0;
  void update(bool value,uint32_t now) {
    changed=false;
    if(value!=raw){raw=value;since=now;}
    if(raw!=down && now-since>=Config::DEBOUNCE_MS){down=raw;changed=true;}
  }
  void poll(uint8_t pin,uint32_t now){update(digitalRead(pin)==LOW,now);}
};
