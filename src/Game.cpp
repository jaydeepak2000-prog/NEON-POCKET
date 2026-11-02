#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <string.h>
#include "Feedback.h"

// Original miniature city game for the existing ESP32-S3 console.
Adafruit_SSD1306 screen(128, 64, &Wire, -1);
constexpr int WORLD = 384, BLOCK = 64, ROAD = 22;
#include "Controls.h"
Button buttons[6];
struct Car { float x, y; int8_t dx, dy; bool moving; };
struct Cop { float x, y; bool active; uint32_t stunnedUntil; };
struct Bullet { float x, y, dx, dy; bool active; };
Car cars[8];
Cop cops[3];
Bullet bullets[6];
float px = 12, py = 100, faceX = 0, faceY = 1;
int riding = -1, health = 100, cash = 0, wanted = 0, jobs = 0;
bool playing = false, ended = false, carrying = false, ready = false;
uint32_t lastTrouble = 0, lastDamage = 0, lastFire = 0, lastStep = 0;
uint32_t messageUntil = 0, bootSince = 0;
const char *message = "";
float cameraX = 0, cameraY = 0;
// Courier pickup and delivery markers are on roads, never inside buildings.
const int16_t stops[][2] = {{12,140},{204,76},{332,268},{76,332},{268,204},{140,12}};
int pickup = 0, destination = 1;

float distance2(float ax, float ay, float bx, float by) {
  float dx = ax-bx, dy = ay-by; return dx*dx + dy*dy;
}
bool solid(float x, float y, float radius) {
  if (x-radius < 0 || y-radius < 0 || x+radius >= WORLD || y+radius >= WORLD) return true;
  for (int cy=0; cy<6; ++cy) for (int cx=0; cx<6; ++cx) {
    int left=cx*BLOCK+ROAD+2, top=cy*BLOCK+ROAD+2;
    int right=(cx+1)*BLOCK-3, bottom=(cy+1)*BLOCK-3;
    if (x+radius>left && x-radius<right && y+radius>top && y-radius<bottom) return true;
  }
  return false;
}
bool moveBody(float &x, float &y, float dx, float dy, float radius) {
  bool moved = false;
  if (!solid(x+dx,y,radius)) { x+=dx; moved = dx!=0; }
  if (!solid(x,y+dy,radius)) { y+=dy; moved = moved || dy!=0; }
  return moved;
}
void tell(const char *text, uint32_t now) {
  message=text;messageUntil=now+1200;
  if(!strcmp(text,"A:CAR  B:FIRE")) Feedback::play(Feedback::Start,now);
  else if(!strcmp(text,"CAR TAKEN!") || !strcmp(text,"ON FOOT")) Feedback::play(Feedback::Car,now);
  else if(!strcmp(text,"POLICE STUNNED")) Feedback::play(Feedback::Stun,now);
  else if(!strcmp(text,"DELIVER TO X")) Feedback::play(Feedback::Pickup,now);
  else if(!strcmp(text,"DELIVERED +100")) Feedback::play(Feedback::Delivered,now);
  else if(!strcmp(text,"HEAT DOWN")) Feedback::play(Feedback::HeatDown,now);
}
void trouble(uint32_t now) { wanted=min(3,wanted+1); lastTrouble=now; }
void resetGame(uint32_t now) {
  Feedback::stop();
  px=12; py=100; faceX=0; faceY=1; riding=-1;
  health=100; cash=0; wanted=0; jobs=0; carrying=false;
  pickup=0; destination=1; ended=false; playing=true;
  lastDamage=now; lastTrouble=now; lastFire=now-300;
  for (auto &b:bullets) b.active=false;
  for (auto &c:cops) c.active=false;
  cars[0]={12,114,0,1,false};
  cars[1]={76,72,0,1,false};
  cars[2]={204,152,0,-1,false};
  cars[3]={300,268,1,0,false};
  cars[4]={45,7,1,0,true};
  cars[5]={260,71,-1,0,true};
  cars[6]={135,200,0,1,true};
  cars[7]={327,300,0,-1,true};
  tell("A:CAR  B:FIRE",now);
  Serial.println("CITY STARTED");
}
void enterExit(uint32_t now) {
  if (riding >= 0) {
    const int8_t offset[][2]={{9,0},{-9,0},{0,10},{0,-10}};
    for (auto &o:offset) {
      if (!solid(px+o[0],py+o[1],2)) {
        cars[riding].moving=false;
        px+=o[0]; py+=o[1]; riding=-1;
        tell("ON FOOT",now); return;
      }
    }
    tell("NO EXIT SPACE",now); return;
  }
  float nearest=20*20; int selected=-1;
  for (int i=0;i<8;++i) {
    float d=distance2(px,py,cars[i].x,cars[i].y);
    if (d<nearest) { nearest=d; selected=i; }
  }
  if (selected<0) { tell("GET NEAR A CAR",now); return; }
  riding=selected; cars[selected].moving=false;
  px=cars[selected].x; py=cars[selected].y;
  faceX=cars[selected].dx; faceY=cars[selected].dy;
  trouble(now); tell("CAR TAKEN!",now);
}
void fire(uint32_t now) {
  if (now-lastFire<220) return;
  for (auto &b:bullets) if (!b.active) {
    b={px+faceX*7,py+faceY*7,faceX*4,faceY*4,true};
    lastFire=now; trouble(now); Feedback::play(Feedback::Shoot,now); return;
  }
}
void updateTraffic() {
  for(int i=0;i<8;++i) {
    if(i==riding || !cars[i].moving) continue;
    auto &c=cars[i]; c.x+=c.dx*0.65f; c.y+=c.dy*0.65f;
    if(c.x<5) c.x=WORLD-6; if(c.x>WORLD-6) c.x=5;
    if(c.y<5) c.y=WORLD-6; if(c.y>WORLD-6) c.y=5;
  }
}
void spawnCop(Cop &cop) {
  for(int attempt=0;attempt<50;++attempt) {
    float x=px+random(-75,76),y=py+random(-75,76);
    float d=distance2(px,py,x,y);
    if(d>40*40 && !solid(x,y,3)) {cop={x,y,true,0};return;}
  }
}
void damage(int amount, uint32_t now) {
  if(now-lastDamage<650) return;
  health=max(0,health-amount); lastDamage=now; Feedback::play(health==0?Feedback::Busted:Feedback::Hit,now);
  if(health==0) { ended=true; Serial.println("CITY GAME OVER"); }
}
void updateCops(uint32_t now) {
  bool near=false;
  for(int i=0;i<3;++i) {
    auto &c=cops[i];
    if(i>=wanted) {c.active=false;continue;}
    if(!c.active || distance2(px,py,c.x,c.y)>140*140) spawnCop(c);
    if(!c.active) continue;
    float dx=px-c.x,dy=py-c.y,d=sqrtf(dx*dx+dy*dy);
    if(d<45) near=true;
    if((int32_t)(now-c.stunnedUntil)>=0 && d>1) {
      float speed=0.8f+0.12f*wanted;
      bool moved=moveBody(c.x,c.y,dx/d*speed,dy/d*speed,3);
      // Follow road centres around corners if a building blocks the direct route.
      if(!moved) {
        float tx=floorf(c.x/BLOCK)*BLOCK+11;
        float ty=floorf(c.y/BLOCK)*BLOCK+11;
        if(fmodf(c.x,BLOCK)<ROAD+2) moveBody(c.x,c.y,0,ty>c.y?speed:-speed,3);
        else moveBody(c.x,c.y,tx>c.x?speed:-speed,0,3);
      }
    }
    if(d<(riding>=0?11:7)) damage(riding>=0?8:12,now);
  }
  if(wanted && !near && now-lastTrouble>9000) {
    --wanted; lastTrouble=now; tell("HEAT DOWN",now);
  }
}
void updateBullets(uint32_t now) {
  for(auto &b:bullets) {
    if(!b.active) continue;
    // Substeps prevent fast shots from passing through thin targets.
    for(int step=0;step<2 && b.active;++step) {
      b.x+=b.dx/2; b.y+=b.dy/2;
      if(solid(b.x,b.y,1) || distance2(px,py,b.x,b.y)>85*85) {b.active=false;break;}
      for(auto &c:cops) if(c.active && distance2(b.x,b.y,c.x,c.y)<6*6) {
        c.stunnedUntil=now+1800; b.active=false; tell("POLICE STUNNED",now); break;
      }
    }
  }
}
void updateJob(uint32_t now) {
  int index=carrying?destination:pickup;
  if(distance2(px,py,stops[index][0],stops[index][1])>10*10) return;
  if(!carrying) {carrying=true;tell("DELIVER TO X",now);}
  else {
    carrying=false;cash+=100;++jobs; health=min(100,health+15);
    pickup=destination; destination=(destination+1)%6;
    tell("DELIVERED +100",now);
    // Move the next pickup away from the completed delivery location.
    pickup=(pickup+2)%6;
    if(pickup==destination) pickup=(pickup+1)%6;
  }
}
void tick(uint32_t now) {
  int dx=(buttons[3].down?1:0)-(buttons[2].down?1:0);
  int dy=(buttons[1].down?1:0)-(buttons[0].down?1:0);
  if(dx || dy) {
    float len=sqrtf(float(dx*dx+dy*dy)); faceX=dx/len;faceY=dy/len;
    float speed=riding>=0?2.3f:1.05f;
    bool moved=moveBody(px,py,faceX*speed,faceY*speed,riding>=0?5:2);
    if(!moved && riding>=0) damage(3,now);
  }
  if(riding>=0) {
    cars[riding].x=px;cars[riding].y=py;
    if(dx || dy) {cars[riding].dx=abs(dx)>=abs(dy)?dx:0;cars[riding].dy=abs(dy)>abs(dx)?dy:0;}
  }
  updateTraffic(); updateBullets(now); updateCops(now); updateJob(now);
}
int sx(float x) {return (int)(x-cameraX);}
int sy(float y) {return (int)(y-cameraY)+10;}
void drawCar(float x,float y,bool vertical,bool police,bool player,uint32_t now) {
  int xx=sx(x),yy=sy(y),w=vertical?7:11,h=vertical?11:7;
  if(xx<-12 || xx>140 || yy<0 || yy>76) return;
  screen.fillRoundRect(xx-w/2,yy-h/2,w,h,1,SSD1306_WHITE);
  screen.fillRect(xx-2,yy-2,4,4,SSD1306_BLACK);
  if(player) screen.drawPixel(xx,yy,SSD1306_WHITE);
  if(police) {
    screen.drawFastHLine(xx-2,yy,5,SSD1306_WHITE);
    if((now/180)%2) screen.drawPixel(xx,yy-1,SSD1306_WHITE);
  }
}
void drawWorld(uint32_t now) {
  cameraX=constrain(px-64,0.0f,float(WORLD-128));
  cameraY=constrain(py-27,0.0f,float(WORLD-54));
  screen.clearDisplay();
  // Draw only the visible city blocks and dashed road centre lines.
  for(int by=max(0,(int)cameraY/BLOCK);by<=min(5,((int)cameraY+54)/BLOCK);++by)
    for(int bx=max(0,(int)cameraX/BLOCK);bx<=min(5,((int)cameraX+128)/BLOCK);++bx) {
      int x=sx(bx*BLOCK+24),y=sy(by*BLOCK+24);
      screen.drawRect(x,y,37,37,SSD1306_WHITE);
      screen.drawRect(x+3,y+3,31,31,SSD1306_WHITE);
      for(int wy=8;wy<30;wy+=10) for(int wx=8;wx<30;wx+=10)
        screen.drawRect(x+wx,y+wy,3,3,SSD1306_WHITE);
      for(int d=26;d<64;d+=12) {
        screen.drawFastHLine(sx(bx*BLOCK+d),sy(by*BLOCK+11),5,SSD1306_WHITE);
        screen.drawFastVLine(sx(bx*BLOCK+11),sy(by*BLOCK+d),5,SSD1306_WHITE);
      }
    }
  int target=carrying?destination:pickup;
  int tx=sx(stops[target][0]),ty=sy(stops[target][1]);
  if((now/250)%2) {
    screen.drawRect(tx-5,ty-5,11,11,SSD1306_WHITE);
    screen.drawLine(tx-3,ty-3,tx+3,ty+3,SSD1306_WHITE);
    screen.drawLine(tx-3,ty+3,tx+3,ty-3,SSD1306_WHITE);
  }
  // Small edge pointer indicates an off-screen courier target.
  if(tx<3 || tx>124 || ty<14 || ty>60) {
    int ex=constrain(tx,3,124),ey=constrain(ty,14,60);
    screen.drawCircle(ex,ey,2,SSD1306_WHITE);
  }
  for(int i=0;i<8;++i) drawCar(cars[i].x,cars[i].y,cars[i].dy!=0,false,i==riding,now);
  for(auto &c:cops) if(c.active) {
    bool stunned=(int32_t)(now-c.stunnedUntil)<0;
    if(!stunned || (now/120)%2) drawCar(c.x,c.y,true,true,false,now);
  }
  for(auto &b:bullets) if(b.active) screen.fillCircle(sx(b.x),sy(b.y),1,SSD1306_WHITE);
  if(riding<0 && (now-lastDamage>250 || (now/80)%2)) {
    int x=sx(px),y=sy(py);
    screen.fillCircle(x,y-2,2,SSD1306_WHITE);
    screen.drawLine(x,y,x,y+3,SSD1306_WHITE);
    screen.drawLine(x-2,y+4,x,y+2,SSD1306_WHITE);
    screen.drawLine(x+2,y+4,x,y+2,SSD1306_WHITE);
    screen.drawLine(x,y,x+(int)(faceX*5),y+(int)(faceY*5),SSD1306_WHITE);
  }
  // HUD is painted last so off-screen geometry cannot overwrite it.
  screen.fillRect(0,0,128,10,SSD1306_BLACK);
  screen.setTextSize(1);screen.setTextColor(SSD1306_WHITE);screen.setCursor(0,1);
  screen.printf("H%d $%d",health,cash);
  screen.setCursor(104,1);
  for(int i=0;i<wanted;++i) screen.print('*');
  screen.drawFastHLine(0,9,128,SSD1306_WHITE);
  if((int32_t)(messageUntil-now)>0) {
    screen.fillRect(0,55,128,9,SSD1306_BLACK);
    screen.setCursor(1,56);screen.print(message);
  }
  screen.display();
}
#include "Launcher.h"
void title() { Launcher::render(millis()); }
// Optional serial diagnostic: T runs core gameplay checks and returns to title.
void runChecks() {
  Feedback::enabled=false;Feedback::stop();
  int passed=0, total=0;
  auto check=[&](bool ok,const char *name) {
    ++total;if(ok)++passed;
    Serial.printf("%s %s\n",ok?"PASS":"FAIL",name);
  };
  Button test;
  test.update(true,1);test.update(true,30);
  check(test.changed && test.down,"momentary press debounced");
  test.update(false,31);test.update(false,60);
  check(test.changed && !test.down,"momentary release ignored by action gate");
  test.update(true,61);test.update(true,90);bool first=test.changed;
  test.update(false,91);test.update(false,120);
  check(first && test.changed,"toggle fires on both stable transitions");
  resetGame(2000);
  check(!solid(px,py,2) && solid(40,40,2),"roads and buildings");
  float x=20,y=35;
  moveBody(x,y,7,0,2);
  check(x==20,"wall collision");
  moveBody(x,y,0,5,2);
  check(y==40,"movement along wall");
  enterExit(2100);
  check(riding==0 && wanted==1,"take nearby car and wanted star");
  enterExit(2200);
  check(riding==-1 && !solid(px,py,2),"safe car exit");
  resetGame(3000);faceX=0;faceY=1;
  cops[0]={px,py+10,true,0};fire(3300);updateBullets(3333);
  check(cops[0].stunnedUntil==5133,"projectile hits police");
  int shots=0;for(auto &b:bullets)if(b.active)++shots;
  fire(3400);
  int after=0;for(auto &b:bullets)if(b.active)++after;
  check(after==shots,"shot cooldown");
  px=stops[pickup][0];py=stops[pickup][1];updateJob(4000);
  check(carrying,"courier pickup");
  px=stops[destination][0];py=stops[destination][1];health=80;updateJob(4100);
  check(!carrying && cash==100 && jobs==1 && health==95,"delivery reward and repair");
  lastDamage=0;damage(10,5000);damage(10,5100);
  check(health==85,"damage cooldown");
  resetGame(6000);wanted=3;lastTrouble=0;
  for(auto &c:cops)c={12,160,true,20000};
  updateCops(10000);
  check(wanted==2,"wanted level decays away from police");
  Launcher::page=Launcher::Home;Launcher::selected=0;
  Launcher::pressA(11000);
  check(Launcher::page==Launcher::Library,"home A enters library");
  Launcher::selected=2;Launcher::pressA(11100);
  check(Launcher::page==Launcher::System,"system info opens");
  Launcher::pressA(11200);
  check(Launcher::page==Launcher::Library,"info A returns to library");
  Launcher::selected=1;Launcher::pressA(11300);
  check(Launcher::page==Launcher::Controls,"controls opens");
  Launcher::pressA(11400);Launcher::selected=0;Launcher::pressA(11500);
  check(Launcher::page==Launcher::Game && playing && !ended,"library launches city");
  Launcher::library();
  check(!playing && !Feedback::motor && Launcher::page==Launcher::Library,"return stops game and motor");
  resetGame(millis());playing=false;messageUntil=0;Launcher::library();title();
  Feedback::stop();Feedback::enabled=true;Serial.printf("CHECKS %d/%d\n",passed,total);
}
void gameSetup() {
  Feedback::begin();
  Serial.begin(115200);
  for(int i=0;i<6;++i) {
    pinMode(PINS[i],INPUT_PULLUP);
    buttons[i].raw=buttons[i].down=digitalRead(PINS[i])==LOW;
  }
  pinMode(0,INPUT_PULLUP);
  Wire.begin(Config::OLED_SDA,Config::OLED_SCL);Wire.setClock(400000);
  uint8_t address=0;
  for(uint8_t a:{uint8_t(0x3C),uint8_t(0x3D)}) {
    Wire.beginTransmission(a);if(Wire.endTransmission()==0){address=a;break;}
  }
  ready=address && screen.begin(SSD1306_SWITCHCAPVCC,address,true,false);
  if(!ready){Serial.println("OLED NOT FOUND");return;}
  Wire.setClock(400000);randomSeed(micros());title();
  Serial.printf("CITY READY; free heap=%u bytes\n",ESP.getFreeHeap());
}
void gameLoop() {
  if(!ready){delay(10);return;}
  if(Serial.available() && Serial.read()=='T') {runChecks();return;}
  uint32_t now=millis();
  bool policeNearby=false;
  if(playing && !ended && wanted) for(const auto &c:cops)
    if(c.active && distance2(px,py,c.x,c.y)<65*65) policeNearby=true;
  Feedback::tick(now,policeNearby,playing && !ended);
  for(int i=0;i<6;++i) buttons[i].poll(PINS[i],now);
  if(digitalRead(0)==LOW) {
    if(!bootSince) bootSince=now;
    if(now-bootSince>=1000) {Launcher::library();title();}
  } else bootSince=0;
  if(Launcher::handle(now)){delay(1);return;}
  // A is momentary (press only); B is latching (either stable edge).
  if(buttons[4].changed && buttons[4].down) {
    if(!playing || ended) resetGame(now); else enterExit(now);
  }
  if(playing && !ended && buttons[5].changed) fire(now);
  if(!playing){delay(1);return;}
  if(ended) {
    screen.clearDisplay();screen.setTextSize(2);screen.setCursor(16,8);screen.print("BUSTED!");
    screen.setTextSize(1);screen.setCursor(8,32);screen.printf("CASH:%d JOBS:%d",cash,jobs);
    screen.setCursor(8,48);screen.print("A:TRY AGAIN");screen.display();delay(20);return;
  }
  if(now-lastStep>=33){lastStep=now;tick(now);drawWorld(now);}
  else delay(1);
}




