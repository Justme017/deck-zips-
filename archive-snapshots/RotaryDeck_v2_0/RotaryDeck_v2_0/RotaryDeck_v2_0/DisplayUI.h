#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "DeckCore.h"

// V2.0-compatible UI: larger menu/tile fonts and revised Media presentation.
class DisplayUI {
public:
 DisplayUI():lcd_(U8G2_R0,DeckConfig::CLOCK,DeckConfig::DATA,DeckConfig::CS,U8X8_PIN_NONE){}
 void begin(){lcd_.begin();lcd_.setPowerSave(0);}
 void render(const DeckState& state,unsigned long now){
  if(state.blank){if(!blanked_){clear();blanked_=true;}return;}
  if(!blanked_ && now-lastFrame_<50)return;
  blanked_=false;lastFrame_=now;view_=state;now_=now;
  lcd_.clearBuffer();lcd_.setDrawColor(1);draw();
  if(view_.options.invert){uint8_t* p=lcd_.getBufferPtr();for(size_t i=0;i<1024;i++)p[i]^=0xff;}
  lcd_.sendBuffer();
 }
 void blankForSleep(){clear();lcd_.setPowerSave(1);blanked_=true;}
private:
 U8G2_ST7920_128X64_F_SW_SPI lcd_;
 DeckState view_;unsigned long now_=0,lastFrame_=0;bool blanked_=false;
 const char* apps_[6]={"CHROME","SPOTIFY","DISCORD","FILES","OBS","CODE"};
 const char* actions_[6]={"SHOT","MUTE","LOCK","SEARCH","DESKTOP","TASKS"};
 void clear(){lcd_.clearBuffer();lcd_.sendBuffer();}
 void small(){lcd_.setFont(u8g2_font_4x6_tr);}
 void normal(){lcd_.setFont(u8g2_font_5x7_tr);}
 void menuFont(){lcd_.setFont(u8g2_font_6x10_tr);}
 void text(int x,int y,const char* s){lcd_.drawStr(x,y,s);}
 void center(int y,const char* s){text((128-lcd_.getStrWidth(s))/2,y,s);}
 bool feedback()const{return view_.feedback && now_-view_.feedbackStart<DeckConfig::FEEDBACK;}
 void bluetooth(){
  if(!view_.connected && view_.advertising && (now_/500)%2)return;
  lcd_.drawVLine(120,1,11);lcd_.drawLine(120,1,124,4);lcd_.drawLine(124,4,116,9);
  lcd_.drawLine(116,3,124,8);lcd_.drawLine(124,8,120,11);
  if(!view_.connected&&!view_.advertising)lcd_.drawLine(115,11,125,1);
 }
 void header(const char* s){normal();lcd_.setDrawColor(1);text(3,9,s);bluetooth();lcd_.drawHLine(2,12,124);}
 void footer(const char* s){normal();lcd_.setDrawColor(1);lcd_.drawHLine(2,53,124);text(3,62,feedback()?view_.feedback:s);}
 void icon(int cx,int cy,int which,bool selected,int radius){
  lcd_.setDrawColor(1);if(selected)lcd_.drawDisc(cx,cy,radius);else lcd_.drawCircle(cx,cy,radius);
  lcd_.setDrawColor(selected?0:1);
  if(which==0){for(int r=0;r<2;r++)for(int c=0;c<2;c++)lcd_.drawBox(cx-4+c*5,cy-4+r*5,3,3);}
  if(which==1){lcd_.drawBox(cx+1,cy-5,2,8);lcd_.drawLine(cx+2,cy-5,cx+6,cy-3);lcd_.drawDisc(cx-1,cy+4,2);}
  if(which==2){lcd_.drawCircle(cx,cy,4);lcd_.drawCircle(cx,cy,1);lcd_.drawVLine(cx,cy-6,3);lcd_.drawVLine(cx,cy+4,3);lcd_.drawHLine(cx-6,cy,3);lcd_.drawHLine(cx+4,cy,3);}
  if(which==3){lcd_.drawFrame(cx-5,cy-4,10,9);lcd_.drawHLine(cx-3,cy-1,6);lcd_.drawHLine(cx-3,cy+2,6);}
  lcd_.setDrawColor(1);
 }
 void home(){
  header("ROTARY DECK");const int x[]={64,99,64,29},y[]={22,33,44,33},r[]={8,9,8,9};
  for(int i=0;i<4;i++)icon(x[i],y[i],i,view_.home==i,r[i]);
  const char* hints[]={"APPS: PRESS OPEN","MEDIA: PRESS OPEN","SETTINGS: PRESS OPEN","ACTIONS: PRESS OPEN"};footer(hints[view_.home]);
 }
 void grid(bool appPage){
  header(appPage?"APPS":"ACTIONS");int selected=appPage?view_.app:view_.action;const int x[]={22,64,106},y[]={27,44};
  for(int i=0;i<6;i++){
   int cx=x[i%3],cy=y[i/3];bool active=i==selected;lcd_.setDrawColor(1);
   if(active)lcd_.drawBox(cx-19,cy-9,38,16);else lcd_.drawFrame(cx-19,cy-9,38,16);
   lcd_.setDrawColor(active?0:1);normal();const char* label=appPage?apps_[i]:actions_[i];text(cx-lcd_.getStrWidth(label)/2,cy+2,label);
  }footer("TURN SELECT PRESS RUN");
 }
 void mediaSpeaker(int x,int y,bool muted){
  lcd_.drawBox(x,y+4,3,5);lcd_.drawTriangle(x+3,y+4,x+8,y,x+8,y+12);
  if(muted){lcd_.drawLine(x+10,y+3,x+14,y+9);lcd_.drawLine(x+14,y+3,x+10,y+9);}
  else {lcd_.drawLine(x+11,y+2,x+13,y+4);lcd_.drawVLine(x+13,y+4,5);lcd_.drawLine(x+13,y+9,x+11,y+11);}
 }
 void mediaGlyph(int x,int which){
  // Symbols occupy y=30..42, inside the 18-pixel selection tile.
  if(which==0){mediaSpeaker(x+4,30,false);}
  else if(which==1){lcd_.drawBox(x+5,30,2,13);lcd_.drawTriangle(x+16,30,x+16,42,x+8,36);}
  else if(which==2){
   // Combined command symbol, not an assertion of current playback state.
   lcd_.drawTriangle(x+3,30,x+3,42,x+11,36);lcd_.drawBox(x+14,30,2,13);lcd_.drawBox(x+18,30,2,13);
  }else if(which==3){lcd_.drawTriangle(x+6,30,x+6,42,x+14,36);lcd_.drawBox(x+16,30,2,13);}
  else {mediaSpeaker(x+3,30,true);}
 }
 void media(){
  header("MEDIA");normal();
  text(3,22,view_.estimate<0?"VOL":"EST");
  char value[9];if(view_.estimate<0)snprintf(value,sizeof(value),"--%%");else snprintf(value,sizeof(value),"~%d%%",view_.estimate);
  text(125-lcd_.getStrWidth(value),22,value);
  lcd_.drawFrame(26,15,70,8);int fill=view_.estimate>0?view_.estimate*66/100:0;
  if(fill>0)lcd_.drawBox(28,17,fill,4);
  for(int i=0;i<5;i++){
   int x=2+i*25;bool selected=i==view_.media;lcd_.setDrawColor(1);
   if(selected)lcd_.drawRBox(x,27,23,18,2);
   else lcd_.drawRFrame(x,27,23,18,2);
   lcd_.setDrawColor(selected?0:1);mediaGlyph(x,i);
  }
  lcd_.setDrawColor(1);normal();
  const char* names[]={"VOLUME","PREVIOUS TRACK","PLAY / PAUSE","NEXT TRACK","MUTE"};
  center(51,view_.volumeEdit?"ADJUST VOLUME":names[view_.media]);
  // Feedback uses only the footer; the bar, controls and label remain visible.
  footer(view_.volumeEdit?"TURN ADJUST PRESS DONE":view_.media==0?"PRESS ADJUST DBL HOME":"PRESS SEND  DBL HOME");
 }
 void menu(const char* title,const char* const* labels,int count,int selected){
  header(title);menuFont();const int start=selected>3?selected-3:0;
  for(int i=0;i<4 && start+i<count;i++){
   int index=start+i,baseline=20+i*10;bool active=index==selected;
   lcd_.setDrawColor(1);if(active)lcd_.drawBox(3,baseline-7,119,10);
   lcd_.setDrawColor(active?0:1);text(7,baseline,labels[index]);text(115,baseline,">");
  }
  lcd_.setDrawColor(1);if(count>4){int h=35*4/count,top=16+(35-h)*start/(count-4);lcd_.drawVLine(125,16,35);lcd_.drawBox(124,top,3,h);}
  footer("PRESS OPEN HOLD BACK");
 }
 void edit(){
  header("EDIT");normal();const char* label="";char value[24];
  switch(view_.editField){
   case EditField::Step:label="COMMANDS PER DETENT";snprintf(value,sizeof(value),"%d",view_.editValue);break;
   case EditField::Reverse:label="ENCODER DIRECTION";snprintf(value,sizeof(value),"%s",view_.editValue?"REVERSED":"NORMAL");break;
   case EditField::Edges:label="EDGES PER DETENT";snprintf(value,sizeof(value),"%d",view_.editValue?4:2);break;
   case EditField::Sound:label="BUTTON SOUND";snprintf(value,sizeof(value),"%s",view_.editValue?"ON":"OFF");break;
   case EditField::Timeout:{label="SCREEN TIMEOUT";const char* options[]={"NEVER","30 SECONDS","60 SECONDS","120 SECONDS"};snprintf(value,sizeof(value),"%s",options[view_.editValue]);break;}
   case EditField::Estimate:label="VOLUME ESTIMATE";snprintf(value,sizeof(value),"~%d%%",view_.editValue);break;
  }center(26,label);menuFont();center(42,value);footer("PRESS SAVE HOLD CANCEL");
 }
 void confirm(){
  bool power=view_.confirmation==Confirmation::Power;header(power?"POWER":"CONNECTION");normal();center(26,power?"TURN OFF THE DECK?":"FORGET ALL PAIRINGS?");
  for(int i=0;i<2;i++){int x=28+i*47;lcd_.setDrawColor(1);if(i==view_.choice)lcd_.drawBox(x,34,28,14);lcd_.setDrawColor(i==view_.choice?0:1);text(x+6,44,i?"YES":"NO");}
  footer("PRESS OK HOLD CANCEL");
 }
 void animation(){
  bool boot=view_.screen==Screen::Boot;unsigned long t=now_-view_.animationStart;int r=boot?min(17,int(t/35)):max(0,17-int(t/50));
  if(r)lcd_.drawCircle(64,25,r);
  if(boot){lcd_.drawBox(59,20,4,4);lcd_.drawBox(65,20,4,4);lcd_.drawBox(59,26,4,4);lcd_.drawBox(65,26,4,4);}
  else {lcd_.drawCircle(64,25,6);lcd_.drawVLine(64,17,9);}
  normal();center(51,boot?"ROTARY DECK V2.0":"TURNING OFF...");
 }
 void draw(){
  switch(view_.screen){
   case Screen::Boot:case Screen::Shutdown:animation();break;
   case Screen::Home:home();break;case Screen::Apps:grid(true);break;case Screen::Actions:grid(false);break;case Screen::Media:media();break;
   case Screen::Settings:{const char* rows[]={"CONNECTION","DISPLAY","CONTROLS","ABOUT","TURN OFF"};menu("SETTINGS",rows,5,view_.setting);break;}
   case Screen::Connection:{const char* rows[]={view_.connected?"CONNECTED":"DISCONNECTED",view_.advertising?"DISCOVERABLE: ON":"DISCOVERABLE: OFF","FORGET PAIRING"};menu("CONNECTION",rows,3,view_.connection);break;}
   case Screen::DisplayPage:{const char* rows[]={view_.options.invert?"INVERT: ON":"INVERT: OFF","SCREEN TIMEOUT","VOLUME ESTIMATE"};menu("DISPLAY",rows,3,view_.display);break;}
   case Screen::Controls:{const char* rows[]={"VOLUME STEP","DIRECTION","DETENT EDGES","BUTTON SOUND"};menu("CONTROLS",rows,4,view_.control);break;}
   case Screen::About:header("ABOUT");normal();text(4,25,"ROTARY DECK V2.0");text(4,35,"XIAO ESP32-S3 / BLE");text(4,45,"128x64 MONO DISPLAY");footer("DOUBLE PRESS: HOME");break;
   case Screen::Edit:edit();break;case Screen::Confirm:confirm();break;
  }lcd_.setDrawColor(1);
 }
};
