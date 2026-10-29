#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "DeckCore.h"

// RotaryDeck v2.2 UI candidate: nonblocking carousel and procedural pixel icons.
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
 bool wasHome_=false;
 int carouselSelected_=0;
 int32_t carouselOffset_=0,carouselFrom_=0;
 uint32_t carouselAt_=0,iconAt_=0;
 static constexpr uint32_t SLIDE_MS=250;
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
  if(!v_.connected&&!v_.advertising){lcd_.drawLine(115,11,125,1);}
  if(!v_.connected&&v_.advertising&&(now_/500)%2)return;
  lcd_.drawVLine(120,1,11);lcd_.drawLine(120,1,124,4);lcd_.drawLine(124,4,116,9);
  lcd_.drawLine(116,3,124,8);lcd_.drawLine(124,8,120,11);
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
   struct tm t;gmtime_r(&local,&t);
   int hour=t.tm_hour;
   if(v_.options.hour12){hour%=12;if(hour==0)hour=12;}
   char hm[6],seconds[4];
   snprintf(hm,sizeof(hm),"%02d:%02d",hour,t.tm_min);
   snprintf(seconds,sizeof(seconds),":%02d",t.tm_sec);
   normal();const int secondsWidth=lcd_.getStrWidth(seconds);
   lcd_.setFont(u8g2_font_logisoso18_tn);
   int hmWidth=lcd_.getStrWidth(hm);
   if(hmWidth+1+secondsWidth>94){menuFont();hmWidth=lcd_.getStrWidth(hm);}
   const int totalWidth=hmWidth+1+secondsWidth;
   const int clockX=2+(94-totalWidth)/2;
   text(clockX,33,hm);
   normal();text(clockX+hmWidth+1,33,seconds);
   const char* weekdays[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
   const char* months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
   char date[8],year[8];
   snprintf(date,sizeof(date),"%02d %s",t.tm_mday,months[t.tm_mon]);
   snprintf(year,sizeof(year),"%04d",t.tm_year+1900);
   small();const int dateCentre=112;
   text(dateCentre-lcd_.getStrWidth(weekdays[t.tm_wday])/2,20,weekdays[t.tm_wday]);
   text(dateCentre-lcd_.getStrWidth(date)/2,28,date);
   const char* bottomLine=v_.options.hour12?(t.tm_hour>=12?"PM":"AM"):year;
   text(dateCentre-lcd_.getStrWidth(bottomLine)/2,36,bottomLine);
  }else{menuFont();center(30,"--:--:--");}

  const char* title=nullptr;
  if(!pcFresh())title="PC COMPANION OFFLINE";else if(!mediaFresh())title="MEDIA DATA STALE";
  else if(v_.mediaSession==0)title="NO MEDIA SESSION";else if(v_.mediaSession==2||!v_.title[0])title="TITLE UNAVAILABLE";else title=v_.title;
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
 // Icons are original drawing primitives, with no downloaded icon assets.
 void carouselIcon(int x,int y,int which,int radius,bool selected,uint32_t elapsed){
  lcd_.setDrawColor(1);
  if(selected)lcd_.drawDisc(x,y,radius);else lcd_.drawCircle(x,y,radius);
  lcd_.setDrawColor(selected?0:1);
  const bool animate=selected&&elapsed<800;
  const int frame=animate?int(elapsed/100):8;
  if(which==0){
   for(int r=0;r<2;r++)for(int c=0;c<2;c++){
    const int tile=r*2+c;
    if(!animate||frame>=tile){int drift=animate&&frame==tile?2:0;lcd_.drawBox(x-4+c*5,y-4+r*5-drift,3,3);}
   }
  }else if(which==1){
   const int levels[8][3]={{3,7,4},{5,9,3},{8,5,6},{6,3,9},{4,7,5},{7,9,3},{5,6,8},{3,8,5}};
   for(int i=0;i<3;i++){int h=animate?levels[frame%8][i]:(i==1?9:5);lcd_.drawBox(x-5+i*4,y+4-h,2,h);}
  }else if(which==2){
   const int8_t dx[16]={0,3,5,6,7,6,5,3,0,-3,-5,-6,-7,-6,-5,-3};
   const int8_t dy[16]={-7,-6,-5,-3,0,3,5,6,7,6,5,3,0,-3,-5,-6};
   lcd_.drawCircle(x,y,4);lcd_.drawCircle(x,y,2);
   int shift=animate?frame%2:0;
   for(int i=0;i<8;i++){int k=(i*2+shift)%16;int ox=dx[k],oy=dy[k];lcd_.drawLine(x+ox,y+oy,x+ox*5/7,y+oy*5/7);}
  }else{
   // A recognisable lightning bolt, revealed from top to bottom.
   lcd_.drawTriangle(x+1,y-7,x-5,y+1,x+1,y+1);
   if(!animate||frame>=2)lcd_.drawTriangle(x-1,y-1,x+5,y-1,x-1,y+7);
   if(animate&&frame>=4&&frame<=5){lcd_.drawPixel(x-7,y-5);lcd_.drawPixel(x+7,y+5);}
  }
  lcd_.setDrawColor(1);
 }
 int32_t slideOffset(uint32_t elapsed)const{
  if(elapsed>=SLIDE_MS)return 0;
  // Cubic ease-out, Q8 pixel position; elapsed is bounded to avoid overflow.
  int32_t remaining=int32_t(SLIDE_MS-elapsed);
  int32_t q=remaining*256/int32_t(SLIDE_MS);
  return int32_t(int64_t(carouselFrom_)*q*q*q/(256L*256L*256L));
 }
  void home(){
  int selected=constrain(int(v_.home),0,3);

  if(!wasHome_){
   carouselSelected_=selected;
   carouselOffset_=0;
   carouselFrom_=0;
   carouselAt_=now_-SLIDE_MS;
   iconAt_=now_;
   wasHome_=true;
  }

  carouselOffset_=slideOffset(now_-carouselAt_);

  if(selected!=carouselSelected_){
   int step=(selected-carouselSelected_+4)%4;
   if(step==3)step=-1;

   carouselFrom_=carouselOffset_+step*44*256;

   while(carouselFrom_>88*256){
    carouselFrom_-=176*256;
   }

   while(carouselFrom_<-88*256){
    carouselFrom_+=176*256;
   }

   carouselSelected_=selected;
   carouselAt_=now_;
   iconAt_=now_+SLIDE_MS;
  }

  carouselOffset_=slideOffset(now_-carouselAt_);

  const bool moving=now_-carouselAt_<SLIDE_MS;
  const uint32_t iconElapsed=
   moving?0:uint32_t(now_-iconAt_);

  header("MENU / V2.2");

  // Icon area ends before the category-name area.
  lcd_.setClipWindow(0,14,128,39);

  for(int relative=-5;relative<=5;relative++){
   const int x=
    64+relative*44+int(carouselOffset_/256);

   if(x < -14 || x > 141)continue;

   const int index=(selected+relative+12)%4;
   const int distance=abs(x-64);
   const int radius=distance<22?11:9;
   const bool active=relative==0;

   lcd_.setDrawColor(1);

   if(active){
    lcd_.drawRFrame(x-14,14,29,25,3);
   }

   carouselIcon(
    x,
    26,
    index,
    radius,
    active,
    moving?800:iconElapsed
   );
  }

  lcd_.setMaxClipWindow();
  lcd_.setDrawColor(1);

  // Dedicated name area, below the icon frame.
  const char* names[]={
   "APPS",
   "MEDIA",
   "SETTINGS",
   "ACTIONS"
  };

  normal();
  center(47,names[selected]);

  // Four centred position dots.
  // Selected: small filled disc. Others: single pixels.
  for(int i=0;i<4;i++){
   const int x=55+i*6;

   if(i==selected){
    lcd_.drawDisc(x,52,1);
   }else{
    lcd_.drawPixel(x,52);
   }
  }

  // Small navigation arrows alongside the name.
  lcd_.drawLine(10,43,7,45);
  lcd_.drawLine(7,45,10,47);

  lcd_.drawLine(117,43,120,45);
  lcd_.drawLine(120,45,117,47);

  footer("TURN SELECT PRESS OPEN");
 }
 void grid(bool apps){
  header(apps?"APPS":"ACTIONS");int selected=apps?v_.app:v_.action;
  const int x[]={22,64,106},y[]={27,44};
  for(int i=0;i<6;i++){int cx=x[i%3],cy=y[i/3];lcd_.setDrawColor(1);
   if(i==selected)lcd_.drawBox(cx-19,cy-9,38,16);else lcd_.drawFrame(cx-19,cy-9,38,16);
   lcd_.setDrawColor(i==selected?0:1);normal();const char* name=apps?apps_[i]:actions_[i];text(cx-lcd_.getStrWidth(name)/2,cy+2,name);
  }footer(v_.options.backDouble?"PRESS RUN DBL BACK":"PRESS RUN HOLD BACK");
 }
  void speaker(int x,int y,bool muted){
  // Keep the current drawing colour:
  // selected buttons use dark icons on a light background.

  // Solid speaker body.
  lcd_.drawBox(x,y+4,3,5);

  // Solid speaker cone, widening towards the right.
  for(int column=0;column<6;column++){
   const int halfHeight=1+column;

   lcd_.drawVLine(
    x+3+column,
    y+6-halfHeight,
    halfHeight*2+1
   );
  }

  if(muted){
   // Clear mute cross.
   lcd_.drawLine(x+11,y+3,x+15,y+9);
   lcd_.drawLine(x+15,y+3,x+11,y+9);
  }else{
   // Sound wave.
   lcd_.drawLine(x+11,y+2,x+13,y+4);
   lcd_.drawVLine(x+13,y+4,5);
   lcd_.drawLine(x+13,y+8,x+11,y+10);
  }
 }
  void media(){
  header("MEDIA");

  // Prefer fresh actual PC volume; mark manual estimates explicitly.
  const bool live=pcFresh()&&v_.pcVolume>=0&&now_-v_.lastVolumeAt<4000;
  const bool estimated=!live&&v_.estimate>=0;
  const bool known=live||estimated;
  const int percent=live?constrain(int(v_.pcVolume),0,100)
   :estimated?constrain(int(v_.estimate),0,100):0;

  normal();
  text(3,22,live?"VOL":estimated?"EST":"VOL");

  char level[9];

  if(live){
   snprintf(level,sizeof(level),"%d%%",percent);
  }else if(estimated){
   snprintf(level,sizeof(level),"~%d%%",percent);
  }else{
   snprintf(level,sizeof(level),"--%%");
  }

  text(125-lcd_.getStrWidth(level),22,level);

  // Volume bar, with space reserved for the percentage.
  const int barX=26;
  const int barY=15;
  const int barW=64;
  const int barH=8;

  lcd_.drawFrame(barX,barY,barW,barH);

  if(known){
   const int fill=percent*(barW-4)/100;

   if(fill>0){
    lcd_.drawBox(barX+2,barY+2,fill,barH-4);
   }
  }else{
   // Unknown is not the same as zero volume.
   small();
   const char* unavailable="--";

   text(
    barX+(barW-lcd_.getStrWidth(unavailable))/2,
    21,
    unavailable
   );
  }

  const int selected=constrain(int(v_.media),0,4);

  // Shorter button frames leave a separate label area.
  for(int i=0;i<5;i++){
   const int x=2+i*25;
   const bool active=i==selected;

   lcd_.setDrawColor(1);

   if(active){
    lcd_.drawRBox(x,27,23,16,2);
   }else{
    lcd_.drawRFrame(x,27,23,16,2);
   }

   lcd_.setDrawColor(active?0:1);

   switch(i){
    case 0:
     speaker(x+4,29,live&&v_.pcMuted);
     break;

    case 1:
     arrow(x+4,29,false);
     break;

    case 2:
     playCommand(x+4,29,false);
     break;

    case 3:
     arrow(x+4,29,true);
     break;

    case 4:
     speaker(x+3,29,true);
     break;
   }
  }

  lcd_.setDrawColor(1);
  normal();

  const char* names[]={
   "VOLUME",
   "PREVIOUS TRACK",
   "PLAY / PAUSE",
   "NEXT TRACK",
   "MUTE"
  };

  // Button frames finish at y=42.
  // Label sits below them, above the divider at y=55.
  center(
   52,
   v_.volumeEdit?"ADJUST VOLUME":names[selected]
  );

  footer(
   v_.volumeEdit
    ?"TURN ADJUST PRESS DONE"
    :v_.options.backDouble
     ?"PRESS SELECT DBL BACK"
     :"PRESS SELECT HOLD BACK"
  );
 }
 void menu(const char* title,const char* const* rows,int n,int selected){
  header(title);menuFont();lcd_.setFontRefHeightAll();
  const int ascent=lcd_.getAscent(),descent=lcd_.getDescent(),glyphHeight=ascent-descent;
  const int rowHeight=max(12,glyphHeight+2),visible=max(1,min(3,38/rowHeight));
  if(n<=0){footer("NO OPTIONS");return;}
  selected=constrain(selected,0,n-1);const int start=constrain(selected-visible/2,0,max(0,n-visible));
  for(int i=0;i<visible&&start+i<n;i++){
   int index=start+i,top=15+i*rowHeight,baseline=top+(rowHeight-glyphHeight)/2+ascent;bool active=index==selected;
   lcd_.setDrawColor(1);if(active)lcd_.drawBox(3,top,119,rowHeight);
   lcd_.setDrawColor(active?0:1);text(7,baseline,rows[index]);
   int cy=top+rowHeight/2;lcd_.drawLine(115,cy-2,117,cy);lcd_.drawLine(117,cy,115,cy+2);
  }
  lcd_.setDrawColor(1);
  if(n>visible){int h=max(5,37*visible/n),top=15+(37-h)*start/(n-visible);lcd_.drawVLine(125,15,37);lcd_.drawBox(124,top,3,h);}
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
  bool power=v_.confirmation==Confirmation::Power;header(power?"POWER":"CONNECTION");normal();center(26,power?"TURN OFF THE DECK?":"FORGET ALL PAIRINGS?");
  for(int i=0;i<2;i++){int x=28+i*47;lcd_.setDrawColor(1);if(i==v_.choice)lcd_.drawBox(x,34,28,14);lcd_.setDrawColor(i==v_.choice?0:1);text(x+6,44,i?"YES":"NO");}
  footer(v_.options.backDouble?"PRESS OK DBL CANCEL":"PRESS OK HOLD CANCEL");
 }
 void animation(){
  bool boot=v_.screen==Screen::Boot;int elapsed=now_-v_.animationStart;int r=boot?min(17,elapsed/35):max(0,17-elapsed/50);
  if(r)lcd_.drawCircle(64,25,r);
  if(boot){lcd_.drawBox(59,20,4,4);lcd_.drawBox(65,20,4,4);lcd_.drawBox(59,26,4,4);lcd_.drawBox(65,26,4,4);}
  else{lcd_.drawCircle(64,25,6);lcd_.drawVLine(64,17,9);}
  normal();center(51,boot?"ROTARY DECK V2.2":"TURNING OFF...");
 }
 void draw(){
  if(v_.screen!=Screen::Home)wasHome_=false;
  switch(v_.screen){
   case Screen::Boot:case Screen::Shutdown:animation();break;
   case Screen::Dashboard:dashboard();break;
   case Screen::Home:home();break;
   case Screen::Apps:grid(true);break;
   case Screen::Actions:grid(false);break;
   case Screen::Media:media();break;
   case Screen::Settings:{const char* rows[]={"CONNECTION","DISPLAY","CONTROLS","ABOUT","TURN OFF"};menu("SETTINGS",rows,5,v_.setting);break;}
   case Screen::Connection:{const char* rows[]={v_.connected?"CONNECTED":"DISCONNECTED",v_.advertising?"DISCOVERABLE: ON":"DISCOVERABLE: OFF","FORGET PAIRING"};menu("CONNECTION",rows,3,v_.connection);break;}
   case Screen::DisplayPage:{const char* rows[]={v_.options.invert?"INVERT: ON":"INVERT: OFF","SCREEN TIMEOUT","VOLUME ESTIMATE",v_.options.hour12?"CLOCK: 12H":"CLOCK: 24H"};menu("DISPLAY",rows,4,v_.display);break;}
   case Screen::Controls:{const char* rows[]={"VOLUME STEP","DIRECTION","DETENT EDGES","BUTTON SOUND","BACK GESTURE"};menu("CONTROLS",rows,5,v_.control);break;}
   case Screen::About:header("ABOUT");normal();text(4,25,"ROTARY DECK V2.2");text(4,35,v_.syncReady?"PC SYNC SERVICE READY":"PC SYNC INIT FAILED");text(4,45,"XIAO ESP32-S3 / BLE");footer(v_.options.backDouble?"DOUBLE PRESS: BACK":"HOLD: BACK");break;
   case Screen::Edit:edit();break;
   case Screen::Confirm:confirm();break;
  }lcd_.setDrawColor(1);
 }
};
