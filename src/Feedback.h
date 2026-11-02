#pragma once
#include <Arduino.h>
#include "Settings.h"

namespace Feedback {
constexpr uint8_t AUDIO_PIN=Config::AUDIO_PIN, MOTOR_PIN=Config::MOTOR_PIN;
struct Note { uint16_t hz, ms; };
enum Event { Start, Shoot, Hit, Pickup, Delivered, Car, Stun, HeatDown, Busted, Siren };
const Note start[]={{523,65},{659,65},{784,110}};
const Note shoot[]={{2100,7},{145,12},{1200,6},{120,24},{100,35},{85,45},{70,55},{60,65}};
const Note hit[]={{160,45},{95,65}};
const Note pickup[]={{880,70},{1320,90}};
const Note delivered[]={{659,70},{784,70},{1047,100},{1319,150}};
const Note car[]={{220,55},{330,55},{440,70}};
const Note stun[]={{1300,30},{650,30},{325,70}};
const Note heatDown[]={{784,70},{523,110}};
const Note busted[]={{392,140},{330,140},{262,140},{196,260}};
const Note siren[]={{650,110},{1000,110},{650,110},{1000,110}};
// Original dark synth loop; single voice, with SFX taking priority.
const Note music[]={{165,210},{330,210},{165,210},{392,210},{165,210},{330,210},{196,210},{311,210},{131,210},{262,210},{131,210},{311,210},{131,210},{262,210},{156,210},{247,210},{147,210},{294,210},{147,210},{349,210},{147,210},{294,210},{175,210},{277,210},{123,210},{247,210},{123,210},{294,210},{123,210},{247,210},{185,210},{0,210},{165,210},{392,210},{330,210},{494,210},{165,210},{392,210},{330,210},{311,210},{131,210},{311,210},{262,210},{392,210},{131,210},{311,210},{262,210},{247,210},{147,210},{349,210},{294,210},{440,210},{147,210},{349,210},{294,210},{277,210},{123,210},{294,210},{247,210},{370,210},{185,210},{247,210},{165,210},{0,210}};
constexpr uint8_t musicCount=sizeof(music)/sizeof(music[0]);
uint8_t musicIndex=0;
bool musicStepActive=false,musicTone=false;
uint32_t musicNext=0,musicOffAt=0;
const Note *sequence=nullptr;
uint8_t count=0,index=0,priority=0;
bool active=false,enabled=true,motor=false;
uint32_t nextNote=0,motorUntil=0,lastSiren=0;

void stop() {
  noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);
  digitalWrite(MOTOR_PIN,LOW);motor=false;active=false;priority=0;
  musicStepActive=false;musicTone=false;musicIndex=0;
}
void begin() {
  digitalWrite(MOTOR_PIN,LOW);pinMode(MOTOR_PIN,OUTPUT);
  digitalWrite(AUDIO_PIN,LOW);pinMode(AUDIO_PIN,OUTPUT);
}
void rumble(uint32_t now,uint16_t duration) {
  if(!enabled)return;
  motorUntil=now+(duration>180?180:duration);
  digitalWrite(MOTOR_PIN,HIGH);motor=true;
}
void play(Event event,uint32_t now) {
  if(!enabled)return;
  const Note *notes=nullptr;uint8_t n=0,p=2;
  switch(event) {
    case Start:notes=start;n=3;break;
    case Shoot:notes=shoot;n=8;p=3;break;
    case Hit:notes=hit;n=2;p=4;rumble(now,100);break;
    case Pickup:notes=pickup;n=2;break;
    case Delivered:notes=delivered;n=4;p=4;rumble(now,45);break;
    case Car:notes=car;n=3;rumble(now,45);break;
    case Stun:notes=stun;n=3;p=3;break;
    case HeatDown:notes=heatDown;n=2;break;
    case Busted:notes=busted;n=4;p=5;rumble(now,180);break;
    case Siren:notes=siren;n=4;p=1;break;
  }
  if(active && p<priority)return;
  musicStepActive=false;musicTone=false;
  noTone(AUDIO_PIN);
  sequence=notes;count=n;index=0;priority=p;active=true;
  tone(AUDIO_PIN,sequence[0].hz);nextNote=now+sequence[0].ms;
}
void tick(uint32_t now,bool policeNearby,bool musicWanted) {
  if(motor && (int32_t)(now-motorUntil)>=0){digitalWrite(MOTOR_PIN,LOW);motor=false;}
  // Keep effect duration accurate even when an OLED transfer takes longer than a note.
  bool noteChanged=false;
  while(active && (int32_t)(now-nextNote)>=0) {
    if(++index>=count) {noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);active=false;priority=0;}
    else {nextNote+=sequence[index].ms;noteChanged=true;}
  }
  if(active && noteChanged)tone(AUDIO_PIN,sequence[index].hz);
  if(!policeNearby && active && priority==1){noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);active=false;priority=0;}
  if(policeNearby && !active && now-lastSiren>=2500) {lastSiren=now;play(Siren,now);}
  if(!enabled || !musicWanted || active) {
    if(musicTone && !active){noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);}
    musicTone=false;musicStepActive=false;return;
  }
  if(musicStepActive && (int32_t)(now-musicNext)>=0) {
    musicIndex=(musicIndex+1)%musicCount;musicStepActive=false;
  }
  if(!musicStepActive) {
    const auto &note=music[musicIndex];
    musicNext=now+note.ms;musicOffAt=now+note.ms*3/5;
    musicStepActive=true;musicTone=note.hz!=0;
    if(musicTone)tone(AUDIO_PIN,note.hz);
    else {noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);}
  }
  if(musicTone && (int32_t)(now-musicOffAt)>=0) {
    noTone(AUDIO_PIN);digitalWrite(AUDIO_PIN,LOW);musicTone=false;
  }
}
}




