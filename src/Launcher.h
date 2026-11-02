#pragma once

namespace Launcher {
enum Page { Home, Library, Controls, System, Game };
Page page=Home;
uint8_t selected=0;
uint32_t lastFrame=0;
const char *entries[]={"CITY CHASE","CONTROLS","SYSTEM INFO"};

void text(int x,int y,const char *label,uint8_t size=1) {
  screen.setTextSize(size);screen.setCursor(x,y);screen.print(label);
}
void home(uint32_t now) {
  // Deterministic moving stars and a perspective grid behind the title panel.
  for(int i=0;i<19;++i)
    screen.drawPixel((i*37+now/90)%128,(i*13)%37,SSD1306_WHITE);
  for(int x=-64;x<=192;x+=32)screen.drawLine(64,33,x,63,SSD1306_WHITE);
  for(int i=0;i<4;++i) {
    int y=38+(i*7+now/130)%26;
    screen.drawFastHLine(0,y,128,SSD1306_WHITE);
  }
  screen.fillRoundRect(20,5,88,34,4,SSD1306_BLACK);
  screen.drawRoundRect(20,5,88,34,4,SSD1306_WHITE);
  text(40,9,"NEON",2);text(29,25,"P O C K E T");
  screen.fillRect(16,46,96,12,SSD1306_BLACK);
  if((now/650)%2==0)text(34,48,"A TO ENTER");
}
void render(uint32_t now) {
  screen.clearDisplay();screen.setTextColor(SSD1306_WHITE);
  if(page==Home)home(now);
  else if(page==Library) {
    text(0,1,"NEON POCKET");text(101,1,"1/1");
    screen.drawFastHLine(0,11,128,SSD1306_WHITE);
    for(int i=0;i<3;++i) {
      int y=15+i*12;
      if(selected==i)screen.fillRoundRect(0,y-1,128,11,2,SSD1306_WHITE);
      screen.setTextColor(selected==i?SSD1306_BLACK:SSD1306_WHITE);
      text(4,y,selected==i?">":" ");text(16,y,entries[i]);
    }
    screen.setTextColor(SSD1306_WHITE);text(1,55,"UP/DN SELECT  A OPEN");
  } else if(page==Controls) {
    text(0,0,"CONTROLS");screen.drawFastHLine(0,10,128,SSD1306_WHITE);
    text(0,14,"D-PAD: MOVE / SELECT");text(0,25,"A: OPEN / ENTER CAR");
    text(0,36,"B TOGGLE: FIRE");text(0,47,"HOLD BOOT: LIBRARY");text(0,57,"A: BACK");
  } else if(page==System) {
    text(0,0,"SYSTEM INFO");screen.drawFastHLine(0,10,128,SSD1306_WHITE);
    text(0,14,"ESP32-S3 / OLED");screen.setCursor(0,25);
    screen.printf("FLASH: %u MB",unsigned(ESP.getFlashChipSize()/1048576));
    screen.setCursor(0,36);screen.printf("PSRAM: %u MB",unsigned(ESP.getPsramSize()/1048576));
    text(0,47,"1 GAME: CITY CHASE");text(0,57,"A: BACK");
  }
  screen.display();
}
void library() {
  page=Library;playing=false;ended=false;Feedback::stop();lastFrame=0;
}
// One transition per debounced press; holding A cannot skip home into the game.
void pressA(uint32_t now) {
  if(page==Home){page=Library;Feedback::play(Feedback::Pickup,now);}
  else if(page==Library) {
    if(selected==0){page=Game;resetGame(now);}
    else page=selected==1?Controls:System;
  } else if(page==Controls || page==System)page=Library;
  lastFrame=0;
}
bool handle(uint32_t now) {
  if(page==Game)return false;
  if(page==Library) {
    int direction=(buttons[1].changed && buttons[1].down)-(buttons[0].changed && buttons[0].down);
    if(direction){selected=(selected+3+direction)%3;Feedback::play(Feedback::HeatDown,now);}
  }
  if(buttons[4].changed && buttons[4].down)pressA(now);
  if(page!=Game && (lastFrame==0 || now-lastFrame>=50)){lastFrame=now;render(now);}
  return true;
}
}
