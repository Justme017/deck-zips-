#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "DeckCore.h"

// Rendering only: no keyboard writes, Preferences writes, or navigation mutations.
class DisplayUI {
public:
 DisplayUI():lcd_(U8G2_R0,DeckConfig::LCD_CLOCK,DeckConfig::LCD_DATA,
                  DeckConfig::LCD_CS,U8X8_PIN_NONE){}
 void begin(){lcd_.begin();lcd_.setPowerSave(0);}
 void render(const DeckState& current,unsigned long now){
  if(current.displayBlank){
   if(!blanked_){clearPixels();blanked_=true;}return;
  }
  if(now-lastFrame_<DeckConfig::FRAME_MS && !blanked_)return;
  blanked_=false;lastFrame_=now;
  // Freeze state and blink/animation time for the complete U8g2 page loop.
  frame_=current;frameTime_=now;
  lcd_.firstPage();do{drawFrame();}while(lcd_.nextPage());
 }
 void blankForSleep(){clearPixels();lcd_.setPowerSave(1);blanked_=true;}
private:
 U8G2_ST7920_128X64_1_SW_SPI lcd_;
 DeckState frame_;
 unsigned long lastFrame_=0,frameTime_=0;
 bool blanked_=false;
 const char* appLabels_[6]={"Code","Chrome","Spotify","Discord","Files","OBS"};
 const char* actionLabels_[6]={"Shot","Mute","Lock","Search","Desktop","Tasks"};
 void clearPixels(){lcd_.firstPage();do{}while(lcd_.nextPage());}
 void small(){lcd_.setFont(u8g2_font_5x7_tr);}
 void normal(){lcd_.setFont(u8g2_font_6x10_tr);}
 void text(int x,int y,const char* value){lcd_.drawStr(x,y,value);}
 void center(int y,const char* value){text((128-lcd_.getStrWidth(value))/2,y,value);}
 bool feedbackVisible()const{
  return frame_.feedback && frameTime_-frame_.feedbackStart<DeckConfig::FEEDBACK_MS;
 }
 void bluetooth(){
  if(!frame_.connected && frame_.advertising && ((frameTime_/500)%2))return;
  lcd_.drawVLine(120,0,11);lcd_.drawLine(120,0,124,3);lcd_.drawLine(124,3,116,8);
  lcd_.drawLine(116,2,124,7);lcd_.drawLine(124,7,120,10);
  if(!frame_.connected && !frame_.advertising)lcd_.drawLine(115,10,125,0);
 }
 void header(const char* title){
  small();lcd_.setDrawColor(1);text(1,8,title);bluetooth();lcd_.drawHLine(0,11,128);
 }
 void footer(const char* hint,bool allowFeedback=true){
  small();lcd_.setDrawColor(1);lcd_.drawHLine(0,55,128);
  text(1,63,allowFeedback && feedbackVisible()?frame_.feedback:hint);
 }
 void powerIcon(int x,int y){
  lcd_.drawCircle(x+5,y+5,4);lcd_.drawVLine(x+5,y-1,7);
 }
 void categoryIcon(int x,int y,int id){
  if(id==0){for(int r=0;r<2;r++)for(int c=0;c<2;c++)lcd_.drawBox(x+c*6,y+r*6,4,4);}
  else if(id==1){lcd_.drawVLine(x+5,y,9);lcd_.drawLine(x+5,y,x+10,y+2);lcd_.drawDisc(x+3,y+9,2);}
  else if(id==2){lcd_.drawLine(x+8,y,x+2,y+5);lcd_.drawHLine(x+2,y+5,6);lcd_.drawLine(x+7,y+5,x+3,y+11);}
  else {
   lcd_.drawCircle(x+5,y+5,4);lcd_.drawDisc(x+5,y+5,1);
   lcd_.drawVLine(x+5,y-1,13);lcd_.drawHLine(x-1,y+5,13);
   lcd_.drawLine(x+1,y+1,x+9,y+9);lcd_.drawLine(x+1,y+9,x+9,y+1);
  }
 }
 void tileIcon(int x,int y,int id){
  static const uint8_t icons[12][18]={
   {0x84,0x00,0x86,0x01,0x03,0x03,0x86,0x01,0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
   {0x78,0x00,0x84,0x00,0x32,0x01,0x31,0x02,0x31,0x02,0x01,0x02,0x02,0x01,0x84,0x00,0x78,0x00},
   {0xfc,0x00,0x02,0x01,0xfd,0x02,0x01,0x02,0x79,0x02,0x01,0x02,0x32,0x01,0x84,0x00,0x78,0x00},
   {0xfc,0x00,0x02,0x01,0x01,0x02,0x49,0x02,0x01,0x02,0x02,0x01,0xfc,0x00,0x84,0x00,0x00,0x00},
   {0x1e,0x00,0x12,0x00,0xff,0x03,0x01,0x02,0x01,0x02,0x01,0x02,0x01,0x02,0x01,0x02,0xff,0x03},
   {0x78,0x00,0x84,0x00,0x1a,0x01,0x25,0x02,0xd9,0x02,0x21,0x03,0x22,0x01,0x84,0x00,0x78,0x00},
   {0x78,0x00,0xff,0x03,0x01,0x02,0x31,0x02,0x49,0x02,0x31,0x02,0x01,0x02,0xff,0x03,0x00,0x00},
   {0x20,0x00,0xb8,0x02,0x3e,0x01,0xbe,0x02,0x38,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
   {0x78,0x00,0x84,0x00,0x84,0x00,0xff,0x03,0x01,0x02,0x31,0x02,0x31,0x02,0x01,0x02,0xff,0x03},
   {0x3c,0x00,0x42,0x00,0x81,0x00,0x81,0x00,0x42,0x00,0x3c,0x00,0x40,0x00,0x80,0x00,0x00,0x01},
   {0xff,0x03,0x01,0x02,0x01,0x02,0x01,0x02,0x01,0x02,0xff,0x03,0x30,0x00,0x30,0x00,0xfc,0x00},
   {0x3f,0x00,0x21,0x00,0xf9,0x01,0x09,0x01,0x0f,0x01,0x08,0x01,0xf8,0x01,0x00,0x00,0x00,0x00}
  };
  lcd_.drawXBMP(x,y,10,9,icons[id]);
 }
 void home(){
  header("ROTARY DECK");
  lcd_.drawCircle(64,29,15);lcd_.drawHLine(39,29,10);lcd_.drawHLine(80,29,14);
  const int x[]={59,96,59,22},y[]={13,24,35,24};
  for(int i=0;i<4;i++){
   lcd_.setDrawColor(i==frame_.home?1:0);lcd_.drawBox(x[i]-3,y[i]-2,17,15);
   lcd_.setDrawColor(i==frame_.home?0:1);categoryIcon(x[i],y[i],i);
  }
  lcd_.setDrawColor(1);lcd_.drawDisc(64,29,2);
  const char* labels[]={"APPS","MEDIA","ACTIONS","SETTINGS"};
  small();center(53,labels[frame_.home]);footer("Turn: browse Click: open");
 }
 void grid(bool apps){
  header(apps?"APPS":"ACTIONS");
  const int selected=apps?frame_.app:frame_.action;
  for(int i=0;i<6;i++){
   const int x=(i%3)*43,y=13+(i/3)*21;
   lcd_.setDrawColor(1);
   if(i==selected)lcd_.drawRBox(x+1,y+1,39,19,2);
   else lcd_.drawRFrame(x+1,y+1,39,19,2);
   lcd_.setDrawColor(i==selected?0:1);tileIcon(x+15,y+2,apps?i:i+6);
   small();const char* label=apps?appLabels_[i]:actionLabels_[i];
   text(x+(41-lcd_.getStrWidth(label))/2,y+18,label);
  }
  footer("Click: run  Hold: back");
 }
 void speaker(int x,int y,bool muted){
  lcd_.drawBox(x,y+4,3,5);lcd_.drawTriangle(x+3,y+4,x+8,y,x+8,y+12);
  if(muted){lcd_.drawLine(x+10,y+3,x+14,y+9);lcd_.drawLine(x+14,y+3,x+10,y+9);}
  else {lcd_.drawLine(x+11,y+2,x+13,y+4);lcd_.drawVLine(x+13,y+4,5);lcd_.drawLine(x+13,y+9,x+11,y+11);}
 }
 void media(){
  header("MEDIA");normal();
  if(frame_.volumeEditing){
   center(23,"VOLUME");
   text(31,40,"-");speaker(57,29,false);text(91,40,"+");
   small();if(feedbackVisible())center(53,frame_.feedback);
   footer("Turn: adjust Click: done",false);return;
  }
  const char* labels[]={"Volume","Previous","Play / Pause","Next","Mute"};
  center(23,labels[frame_.media]);
  for(int i=0;i<5;i++){
   int x=i*25+1;lcd_.setDrawColor(1);
   if(i==frame_.media)lcd_.drawRBox(x,28,24,17,2);
   lcd_.setDrawColor(i==frame_.media?0:1);
   if(i==0)speaker(x+4,30,false);
   else if(i==1 || i==3){
    lcd_.drawTriangle(x+(i==1?16:7),30,x+(i==1?16:7),42,x+(i==1?7:16),36);
    lcd_.drawVLine(x+(i==1?5:18),30,13);
   }else if(i==2){
    lcd_.drawTriangle(x+3,30,x+3,42,x+11,36);
    lcd_.drawBox(x+14,30,2,13);lcd_.drawBox(x+18,30,2,13);
   }else speaker(x+3,30,true);
  }
  lcd_.setDrawColor(1);small();if(feedbackVisible())center(53,frame_.feedback);
  footer(frame_.media==0?"Click: adjust Hold: back":"Click: send  Hold: back",false);
 }
 void list(const char* title,const char* const* rows,int count,int selected,bool powerLast=false){
  header(title);normal();const int start=selected>=4?selected-3:0;
  for(int i=0;i<4 && start+i<count;i++){
   int index=start+i,y=13+i*10;bool active=index==selected;
   lcd_.setDrawColor(1);if(active)lcd_.drawBox(0,y,122,10);
   lcd_.setDrawColor(active?0:1);
   if(powerLast && index==count-1){
    lcd_.drawCircle(6,y+5,3);lcd_.drawVLine(6,y+1,4);text(13,y+7,rows[index]);
   }else text(3,y+7,rows[index]);
   small();text(115,y+8,">");normal();
  }
  lcd_.setDrawColor(1);
  if(count>4){
   lcd_.drawVLine(125,14,38);int height=max(5,38*4/count);
   int top=14+(38-height)*start/max(1,count-4);lcd_.drawBox(124,top,3,height);
  }
  footer("Click: open Hold: back");
 }
 void edit(){
  header("EDIT");small();const char* label="";char value[24];
  switch(frame_.editField){
   case DeckEdit::VolumeStep:label="Commands per detent";snprintf(value,sizeof(value),"%d",frame_.editValue);break;
   case DeckEdit::Direction:label="Encoder direction";snprintf(value,sizeof(value),"%s",frame_.editValue?"Reversed":"Normal");break;
   case DeckEdit::Detents:label="Transitions / detent";snprintf(value,sizeof(value),"%d",frame_.editValue?4:2);break;
   case DeckEdit::Sound:label="Button sound";snprintf(value,sizeof(value),"%s",frame_.editValue?"On":"Off");break;
   case DeckEdit::Timeout:{
    label="Screen timeout";const char* values[]={"Never","30 seconds","60 seconds","120 seconds"};
    snprintf(value,sizeof(value),"%s",values[frame_.editValue]);break;
   }
  }
  center(26,label);normal();center(44,value);footer("Click: save Hold: cancel");
 }
 void confirmation(){
  const bool power=frame_.confirmation==DeckConfirm::PowerOff;
  header(power?"POWER":"CONNECTION");small();center(27,power?"Turn off the deck?":"Forget all pairings?");
  for(int i=0;i<2;i++){
   const int x=28+i*47;lcd_.setDrawColor(1);if(i==frame_.confirmationChoice)lcd_.drawRBox(x,34,28,14,2);
   lcd_.setDrawColor(i==frame_.confirmationChoice?0:1);normal();text(x+5,44,i?"YES":"NO");
  }
  footer("Click: OK  Hold: cancel");
 }
 void animation(){
  const bool boot=frame_.screen==DeckScreen::Boot;
  const unsigned long elapsed=frameTime_-frame_.animationStart;
  int radius=boot?min(18,int(elapsed/35)):max(0,18-int(elapsed/50));
  if(radius>0)lcd_.drawCircle(64,24,radius);
  if(boot)categoryIcon(59,19,0);else powerIcon(59,19);
  normal();center(51,boot?"ROTARY DECK":"Turning off...");
 }
 void drawFrame(){
  lcd_.setDrawColor(1);small();
  switch(frame_.screen){
   case DeckScreen::Boot:case DeckScreen::Shutdown:animation();break;
   case DeckScreen::Home:home();break;
   case DeckScreen::Apps:grid(true);break;
   case DeckScreen::Actions:grid(false);break;
   case DeckScreen::Media:media();break;
   case DeckScreen::Settings:{
    const char* rows[]={"Connection","Display","Controls","About","Turn off"};
    list("SETTINGS",rows,5,frame_.setting,true);break;
   }
   case DeckScreen::Connection:{
    const char* rows[]={frame_.connected?"Connected":"Disconnected",frame_.advertising?"Discoverable: on":"Discoverable: off","Forget pairing"};
    list("CONNECTION",rows,3,frame_.connection);break;
   }
   case DeckScreen::DisplayOptions:{const char* rows[]={"Screen timeout"};list("DISPLAY",rows,1,0);break;}
   case DeckScreen::Controls:{
    const char* rows[]={"Volume step","Direction","Detent edges","Button sound"};
    list("CONTROLS",rows,4,frame_.control);break;
   }
   case DeckScreen::About:
    header("ABOUT");normal();center(25,"Rotary Deck v1.2");
    center(38,"XIAO ESP32-S3");small();center(50,"BLE HID / 128 x 64");footer("Hold: back");break;
   case DeckScreen::Edit:edit();break;
   case DeckScreen::Confirm:confirmation();break;
  }
  lcd_.setDrawColor(1);
 }
};
