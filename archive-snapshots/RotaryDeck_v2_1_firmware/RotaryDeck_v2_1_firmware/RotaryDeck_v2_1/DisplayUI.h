#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "DeckCore.h"

// UI layout revision R2. Bluetooth, state and command handling remain in DeckCore.
class DisplayUI {
public:
 DisplayUI():lcd_(U8G2_R0,DeckConfig::CLOCK,DeckConfig::DATA,DeckConfig::CS,U8X8_PIN_NONE){}
 void begin(){lcd_.begin();lcd_.setPowerSave(0);}
 void render(const DeckState& s,uint32_t now){
  if(s.blank){if(!blanked_){clear();blanked_=true;}return;}
  if(!blanked_&&now-lastFrame_<50)return;
  blanked_=false;lastFrame_=now;v_=s;now_=now;
  lcd_.clearBuffer();lcd_.setDrawColor(1);draw();
  if(v_.options.invert){uint8_t* p=lcd_.getBufferPtr();for(size_t i=0;i<1024;i++)p[i]^=0xff;}
  lcd_.sendBuffer();
 }
 void blankForSleep(){clear();lcd_.setPowerSave(1);blanked_=true;}
private:
 U8G2_ST7920_128X64_F_SW_SPI lcd_;
 DeckState v_;
 uint32_t now_=0,lastFrame_=0;
 bool blanked_=false;
 const char* apps_[6]={"CHROME","SPOTIFY","DISCORD","FILES","OBS","CODE"};
 const char* actions_[6]={"SHOT","MUTE","LOCK","SEARCH","DESKTOP","TASKS"};
 void clear(){lcd_.clearBuffer();lcd_.sendBuffer();}
 void normal(){lcd_.setFont(u8g2_font_5x7_tr);lcd_.setFontPosBaseline();}
 void small(){lcd_.setFont(u8g2_font_4x6_tr);lcd_.setFontPosBaseline();}
 void menuFont(){lcd_.setFont(u8g2_font_6x10_tr);lcd_.setFontPosBaseline();}
 void text(int x,int y,const char* s){lcd_.drawStr(x,y,s);}
 void center(int y,const char* s){text((128-lcd_.getStrWidth(s))/2,y,s);}
 bool feedback()const{return v_.feedback&&now_-v_.feedbackStart<DeckConfig::FEEDBACK;}
 bool pcFresh()const{return v_.connected&&v_.pcSeen&&now_-v_.lastPcAt<12000;}
 bool mediaFresh()const{return pcFresh()&&now_-v_.lastMediaAt<12000;}
 void bt(){
  if(!v_.connected&&v_.advertising&&(now_/500)%2)return;
  lcd_.drawVLine(120,1,11);lcd_.drawLine(120,1,124,4);
  lcd_.drawLine(124,4,116,9);lcd_.drawLine(116,3,124,8);
  lcd_.drawLine(124,8,120,11);
  if(!v_.connected&&!v_.advertising)lcd_.drawLine(115,11,125,1);
 }
 void header(const char* label){normal();lcd_.setDrawColor(1);text(3,9,label);bt();lcd_.drawHLine(2,12,124);}
 void footer(const char* hint){normal();lcd_.setDrawColor(1);lcd_.drawHLine(2,55,124);text(3,63,feedback()?v_.feedback:hint);}
 void arrow(int x,int y,bool next){
  if(next){lcd_.drawTriangle(x+2,y,x+2,y+10,x+10,y+5);lcd_.drawBox(x+12,y,2,11);}
  else{lcd_.drawBox(x+1,y,2,11);lcd_.drawTriangle(x+13,y,x+13,y+10,x+5,y+5);}
 }
 void playCommand(int x,int y,bool useState){
  if(useState&&mediaFresh()&&v_.playback==1){lcd_.drawBox(x+3,y,3,11);lcd_.drawBox(x+10,y,3,11);}
  else if(useState&&mediaFresh()&&(v_.playback==2||v_.playback==3)){lcd_.drawTriangle(x+3,y,x+3,y+10,x+12,y+5);}
  else{lcd_.drawTriangle(x,y,x,y+10,x+7,y+5);lcd_.drawBox(x+9,y,2,11);lcd_.drawBox(x+13,y,2,11);}
 }
 void dashboard(){
  header("ROTARY DECK");small();text(92,9,pcFresh()?"PC":"--");
  if(v_.timeValid){
   time_t local=time_t(int64_t(v_.utc)+uint32_t(now_-v_.timeAt)/1000+int32_t(v_.utcOffset)*60);
   struct tm t;gmtime_r(&local,&t);char hm[6],sec[4];
   snprintf(hm,sizeof(hm),"%02d:%02d",t.tm_hour,t.tm_min);snprintf(sec,sizeof(sec),":%02d",t.tm_sec);
   lcd_.setFont(u8g2_font_logisoso18_tn);text(13,33,hm);menuFont();text(91,31,sec);
   if(!pcFresh()){small();text(115,32,"*");}
  }else{menuFont();center(30,"--:--:--");}
  const char* title=nullptr;
  if(!pcFresh())title="PC COMPANION OFFLINE";
  else if(!mediaFresh())title="MEDIA DATA STALE";
  else if(v_.mediaSession==0)title="NO MEDIA SESSION";
  else if(v_.mediaSession==2||!v_.title[0])title="TITLE UNAVAILABLE";
  else title=v_.title;
  char window[25];size_t len=strlen(title),start=0;
  if(title==v_.title&&len>24){uint32_t elapsed=now_-v_.titleAt;size_t span=len-24;size_t phase=(elapsed/350)%(span+9);start=phase<4?0:min(span,phase-4);}
  size_t count=min(size_t(24),len-start);memcpy(window,title+start,count);window[count]=0;
  normal();center(44,feedback()?v_.feedback:window);
  for(int i=0;i<4;i++){
   int x=2+i*32;lcd_.setDrawColor(1);
   if(i==v_.dashboard)lcd_.drawRBox(x,47,28,16,2);else lcd_.drawRFrame(x,47,28,16,2);
   lcd_.setDrawColor(i==v_.dashboard?0:1);
   if(i==0)arrow(x+6,49,false);if(i==1)playCommand(x+6,49,true);if(i==2)arrow(x+6,49,true);
   if(i==3){for(int r=0;r<2;r++)for(int c=0;c<2;c++)lcd_.drawBox(x+8+c*7,50+r*6,4,4);}
  }lcd_.setDrawColor(1);
 }
 void category(int x,int y,int which,bool selected,int radius){
  lcd_.setDrawColor(1);
  if(selected)lcd_.drawDisc(x,y,radius);else lcd_.drawCircle(x,y,radius);
  lcd_.setDrawColor(selected?0:1);
  if(which==0){for(int r=0;r<2;r++)for(int c=0;c<2;c++)lcd_.drawBox(x-4+c*5,y-4+r*5,3,3);}
  if(which==1){lcd_.drawVLine(x+2,y-5,9);lcd_.drawLine(x+2,y-5,x+6,y-3);lcd_.drawDisc(x,y+4,2);}
  if(which==2){
   lcd_.drawCircle(x,y,4);lcd_.drawCircle(x,y,2);
   lcd_.drawVLine(x,y-7,3);lcd_.drawVLine(x,y+5,3);
   lcd_.drawHLine(x-7,y,3);lcd_.drawHLine(x+5,y,3);
   lcd_.drawLine(x-5,y-5,x-3,y-3);lcd_.drawLine(x+3,y-3,x+5,y-5);
   lcd_.drawLine(x-5,y+5,x-3,y+3);lcd_.drawLine(x+3,y+3,x+5,y+5);
  }
  if(which==3){lcd_.drawFrame(x-5,y-4,10,9);lcd_.drawHLine(x-3,y-1,6);lcd_.drawHLine(x-3,y+2,6);}
  lcd_.setDrawColor(1);
 }
 void home(){
  header("MENU / R2");
  const char* names[]={"APPS","MEDIA","SETTINGS","ACTIONS"};
  const char* hints[]={"APPS: PRESS OPEN","MEDIA: PRESS OPEN","SETTINGS: PRESS OPEN","ACTIONS: PRESS OPEN"};
  int selected=constrain(int(v_.home),0,3);
  for(int i=0;i<4;i++){
   int cx=16+i*32;
   category(cx,27,i,i==selected,10);
   if(i==selected)lcd_.drawCircle(cx,27,12);
   small();text(cx-lcd_.getStrWidth(names[i])/2,49,names[i]);
   if(i==selected)lcd_.drawHLine(cx-13,52,26);
  }
  footer(hints[selected]);
 }
 void grid(bool apps){
  header(apps?"APPS":"ACTIONS");int selected=apps?v_.app:v_.action;
  const int x[]={22,64,106},y[]={27,44};
  for(int i=0;i<6;i++){
   int cx=x[i%3],cy=y[i/3];lcd_.setDrawColor(1);
   if(i==selected)lcd_.drawBox(cx-19,cy-9,38,16);else lcd_.drawFrame(cx-19,cy-9,38,16);
   lcd_.setDrawColor(i==selected?0:1);normal();const char* name=apps?apps_[i]:actions_[i];text(cx-lcd_.getStrWidth(name)/2,cy+2,name);
  }footer(v_.options.backDouble?"PRESS RUN DBL BACK":"PRESS RUN HOLD BACK");
 }
 void speaker(int x,int y,bool muted){
  lcd_.drawBox(x,y+4,3,5);lcd_.drawTriangle(x+3,y+4,x+8,y,x+8,y+12);
  if(muted){lcd_.drawLine(x+10,y+3,x+14,y+9);lcd_.drawLine(x+14,y+3,x+10,y+9);}
  else{lcd_.drawLine(x+11,y+2,x+13,y+4);lcd_.drawVLine(x+13,y+4,5);lcd_.drawLine(x+13,y+9,x+11,y+11);}
 }
 void media(){
  header("MEDIA");normal();text(3,22,v_.estimate<0?"VOL":"EST");char level[9];
  if(v_.estimate<0)snprintf(level,sizeof(level),"--%%");else snprintf(level,sizeof(level),"~%d%%",v_.estimate);
  text(125-lcd_.getStrWidth(level),22,level);lcd_.drawFrame(26,15,70,8);
  int fill=v_.estimate>0?v_.estimate*66/100:0;if(fill)lcd_.drawBox(28,17,fill,4);
  for(int i=0;i<5;i++){
   int x=2+i*25;lcd_.setDrawColor(1);
   if(i==v_.media)lcd_.drawRBox(x,27,23,18,2);else lcd_.drawRFrame(x,27,23,18,2);
   lcd_.setDrawColor(i==v_.media?0:1);
   if(i==0)speaker(x+4,30,false);if(i==1)arrow(x+4,30,false);if(i==2)playCommand(x+4,30,false);
   if(i==3)arrow(x+4,30,true);if(i==4)speaker(x+3,30,true);
  }
  lcd_.setDrawColor(1);normal();const char* names[]={"VOLUME","PREVIOUS TRACK","PLAY / PAUSE","NEXT TRACK","MUTE"};
  center(51,v_.volumeEdit?"ADJUST VOLUME":names[v_.media]);
  footer(v_.volumeEdit?"TURN ADJUST PRESS DONE":v_.options.backDouble?"PRESS SELECT DBL BACK":"PRESS SELECT HOLD BACK");
 }
 void menu(const char* title,const char* const* rows,int n,int selected){
  header(title);menuFont();lcd_.setFontRefHeightAll();
  const int ascent=lcd_.getAscent(),descent=lcd_.getDescent();
  const int glyphHeight=ascent-descent;
  const int rowHeight=max(12,glyphHeight+2);
  const int top=15,available=38;
  const int visible=max(1,min(3,available/rowHeight));
  if(n<=0){footer("NO OPTIONS");return;}
  selected=constrain(selected,0,n-1);
  const int maxStart=max(0,n-visible);
  const int start=constrain(selected-visible/2,0,maxStart);
  for(int i=0;i<visible&&start+i<n;i++){
   const int index=start+i,rowTop=top+i*rowHeight;
   const int baseline=rowTop+(rowHeight-glyphHeight)/2+ascent;
   const bool active=index==selected;
   lcd_.setDrawColor(1);if(active)lcd_.drawBox(3,rowTop,119,rowHeight);
   lcd_.setDrawColor(active?0:1);text(7,baseline,rows[index]);
   // Draw the chevron geometrically, independent of font metrics.
   const int cy=rowTop+rowHeight/2;
   lcd_.drawLine(115,cy-2,117,cy);lcd_.drawLine(117,cy,115,cy+2);
  }
  lcd_.setDrawColor(1);
  if(n>visible){
   const int trackTop=15,trackHeight=37;
   const int thumbHeight=max(5,trackHeight*visible/n);
   const int thumbTop=trackTop+(trackHeight-thumbHeight)*start/(n-visible);
   lcd_.drawVLine(125,trackTop,trackHeight);lcd_.drawBox(124,thumbTop,3,thumbHeight);
  }
  footer(v_.options.backDouble?"PRESS OPEN DBL BACK":"PRESS OPEN HOLD BACK");
 }
 void edit(){
  header("EDIT");normal();char val[24];const char* label="";
  switch(v_.editField){
   case EditField::Step:label="COMMANDS PER DETENT";snprintf(val,sizeof(val),"%d",v_.editValue);break;
   case EditField::Reverse:label="ENCODER DIRECTION";snprintf(val,sizeof(val),"%s",v_.editValue?"REVERSED":"NORMAL");break;
   case EditField::Edges:label="EDGES PER DETENT";snprintf(val,sizeof(val),"%d",v_.editValue?4:2);break;
   case EditField::Sound:label="BUTTON SOUND";snprintf(val,sizeof(val),"%s",v_.editValue?"ON":"OFF");break;
   case EditField::Timeout:{label="SCREEN TIMEOUT";const char* values[]={"NEVER","30 SECONDS","60 SECONDS","120 SECONDS"};snprintf(val,sizeof(val),"%s",values[v_.editValue]);break;}
   case EditField::Estimate:label="VOLUME ESTIMATE";snprintf(val,sizeof(val),"~%d%%",v_.editValue);break;
   case EditField::BackGesture:label="BACK GESTURE";snprintf(val,sizeof(val),"%s",v_.editValue?"HOLD":"DOUBLE PRESS");break;
  }
  center(26,label);menuFont();center(42,val);footer(v_.options.backDouble?"PRESS SAVE DBL CANCEL":"PRESS SAVE HOLD CANCEL");
 }
 void confirm(){
  bool power=v_.confirmation==Confirmation::Power;header(power?"POWER":"CONNECTION");normal();
  center(26,power?"TURN OFF THE DECK?":"FORGET ALL PAIRINGS?");
  for(int i=0;i<2;i++){int x=28+i*47;lcd_.setDrawColor(1);if(i==v_.choice)lcd_.drawBox(x,34,28,14);lcd_.setDrawColor(i==v_.choice?0:1);text(x+6,44,i?"YES":"NO");}
  footer(v_.options.backDouble?"PRESS OK DBL CANCEL":"PRESS OK HOLD CANCEL");
 }
 void animation(){
  bool boot=v_.screen==Screen::Boot;int elapsed=now_-v_.animationStart;
  int r=boot?min(17,elapsed/35):max(0,17-elapsed/50);if(r)lcd_.drawCircle(64,25,r);
  if(boot){lcd_.drawBox(59,20,4,4);lcd_.drawBox(65,20,4,4);lcd_.drawBox(59,26,4,4);lcd_.drawBox(65,26,4,4);}
  else{lcd_.drawCircle(64,25,6);lcd_.drawVLine(64,17,9);}
  normal();center(51,boot?"ROTARY DECK V2.1":"TURNING OFF...");
 }
 void draw(){
  switch(v_.screen){
   case Screen::Boot:case Screen::Shutdown:animation();break;
   case Screen::Dashboard:dashboard();break;
   case Screen::Home:home();break;
   case Screen::Apps:grid(true);break;
   case Screen::Actions:grid(false);break;
   case Screen::Media:media();break;
   case Screen::Settings:{const char* rows[]={"CONNECTION","DISPLAY","CONTROLS","ABOUT","TURN OFF"};menu("SETTINGS",rows,5,v_.setting);break;}
   case Screen::Connection:{const char* rows[]={v_.connected?"CONNECTED":"DISCONNECTED",v_.advertising?"DISCOVERABLE: ON":"DISCOVERABLE: OFF","FORGET PAIRING"};menu("CONNECTION",rows,3,v_.connection);break;}
   case Screen::DisplayPage:{const char* rows[]={v_.options.invert?"INVERT: ON":"INVERT: OFF","SCREEN TIMEOUT","VOLUME ESTIMATE"};menu("DISPLAY",rows,3,v_.display);break;}
   case Screen::Controls:{const char* rows[]={"VOLUME STEP","DIRECTION","DETENT EDGES","BUTTON SOUND","BACK GESTURE"};menu("CONTROLS",rows,5,v_.control);break;}
   case Screen::About:header("ABOUT");normal();text(4,25,"ROTARY DECK V2.1");text(4,35,v_.syncReady?"PC SYNC SERVICE READY":"PC SYNC INIT FAILED");text(4,45,"XIAO ESP32-S3 / BLE");footer(v_.options.backDouble?"DOUBLE PRESS: BACK":"HOLD: BACK");break;
   case Screen::Edit:edit();break;
   case Screen::Confirm:confirm();break;
  }lcd_.setDrawColor(1);
 }
};
