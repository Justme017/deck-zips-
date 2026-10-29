#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// Dependency target: Arduino-ESP32 2.0.17; T-vK ESP32 BLE Keyboard 0.3.0,
// standard Bluedroid backend (USE_NIMBLE disabled); U8g2 2.x.
class RotaryDeck {
public:
 static constexpr int A=1,B=4,BTN=5,BUZZ=6;
 RotaryDeck():lcd(U8G2_R0,7,9,44,U8X8_PIN_NONE),kbd("Rotary Deck","DIY",100){}
 void begin(){
  rtc_gpio_deinit(GPIO_NUM_5);
  pinMode(A,INPUT_PULLUP);pinMode(B,INPUT_PULLUP);pinMode(BTN,INPUT_PULLUP);
  pinMode(BUZZ,OUTPUT);digitalWrite(BUZZ,LOW);
  store.begin("rotarydeck",false);
  reversed=store.getBool("reverse",false);step=constrain(store.getInt("step",1),1,5);
  timeout=constrain(store.getInt("timeout",0),0,3);sound=store.getBool("sound",false);
  transitions=store.getInt("detents",4)==2?2:4;
  lcd.begin();lcd.setPowerSave(0);
  // Consume a held wake button without opening a menu.
  inhibitButton=digitalRead(BTN)==LOW;
  raw=stable=digitalRead(BTN);changed=millis();
  kbd.begin();kbd.setDelay(8);advertiseWanted=true;
  scene=BOOT;epoch=millis();lastActivity=epoch;
 }
 void turn(int detents){
  if(detents==0||scene==BOOT||scene==SHUTDOWN)return;
  if(wakeDisplay())return;
  lastActivity=millis();beep();
  if(reversed)detents=-detents;
  if(scene==MEDIA && editVolume){
   int count=abs(detents)*step;
   volumePending=constrain(volumePending+(detents>0?count:-count),-30,30);
   return;
  }
  if(scene==EDIT){
   editValue=constrain(editValue+detents,editMin(),editMax());return;
  }
  int n=scene==HOME?4:scene==APPS?6:scene==ACTIONS?6:scene==MEDIA?5:
        scene==SETTINGS?5:scene==CONNECTION?3:scene==DISPLAY?1:
        scene==CONTROLS?4:scene==CONFIRM?2:0;
  if(n){int& value=selection();value=((value+detents)%n+n)%n;}
 }
 void click(){
  if(scene==BOOT||scene==SHUTDOWN)return;
  if(wakeDisplay())return;
  lastActivity=millis();beep();
  switch(scene){
   case HOME:{const Scene pages[]={APPS,MEDIA,ACTIONS,SETTINGS};scene=pages[home];break;}
   case APPS:launch(app);break;
   case ACTIONS:action(actionSel);break;
   case MEDIA:
    if(media==0)editVolume=!editVolume;
    else if(ready()){
     if(media==1)kbd.write(KEY_MEDIA_PREVIOUS_TRACK);
     if(media==2)kbd.write(KEY_MEDIA_PLAY_PAUSE);
     if(media==3)kbd.write(KEY_MEDIA_NEXT_TRACK);
     if(media==4)kbd.write(KEY_MEDIA_MUTE);
     message("Command sent");
    }break;
   case SETTINGS:
    if(setting==0)scene=CONNECTION;
    if(setting==1)scene=DISPLAY;
    if(setting==2)scene=CONTROLS;
    if(setting==3)scene=ABOUT;
    if(setting==4)ask(POWER);break;
   case CONNECTION:
    if(connection==0)message(kbd.isConnected()?"Connected":"Not connected");
    if(connection==1){
     if(kbd.isConnected())message("Disconnect PC first");
     else {advertiseWanted=!advertiseWanted;
      if(advertiseWanted)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();
      message(advertiseWanted?"Discoverable":"Advertising off");}
    }
    if(connection==2)ask(FORGET);break;
   case DISPLAY: startEdit(TIMEOUT,timeout);break;
   case CONTROLS:
    if(control==0)startEdit(STEP,step);
    if(control==1)startEdit(REVERSE,reversed);
    if(control==2)startEdit(DETENTS,transitions==4?1:0);
    if(control==3)startEdit(SOUND,sound);break;
   case EDIT: saveEdit();break;
   case CONFIRM:
    if(confirm==0)scene=confirmReturn;
    else if(confirmKind==POWER){save();volumePending=0;scene=SHUTDOWN;epoch=millis();}
    else forgetPairing();break;
   default:break;
  }
 }
 void hold(){
  if(scene==BOOT||scene==SHUTDOWN)return;
  if(wakeDisplay())return;
  lastActivity=millis();beep();
  if(scene==MEDIA && editVolume){editVolume=false;volumePending=0;return;}
  if(scene==EDIT){scene=editReturn;return;}
  if(scene==CONFIRM){scene=confirmReturn;return;}
  if(scene==CONNECTION||scene==DISPLAY||scene==CONTROLS||scene==ABOUT)scene=SETTINGS;
  else scene=HOME;
 }
 void pollButton(){
  unsigned long now=millis();bool reading=digitalRead(BTN);
  if(reading!=raw){raw=reading;changed=now;}
  if(inhibitButton){
   if(raw==HIGH && now-changed>=25){inhibitButton=false;stable=HIGH;held=false;}
   return;
  }
  if(now-changed>=25 && stable!=raw){
   stable=raw;
   if(stable==LOW){pressed=now;held=false;}
   else if(!held)click();
  }
  if(stable==LOW && !held && now-pressed>=700){held=true;hold();}
 }
 int detentsPerStep()const{return transitions;}
 void tick(){
  unsigned long now=millis();
  buzzerTick();
  bool connected=kbd.isConnected();
  if(connected!=wasConnected){
   volumePending=0;wasConnected=connected;
   if(!connected){
    // Library restarts advertising on disconnect; apply user's desired state.
    if(advertiseWanted)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();
   }
   if(scene!=BOOT&&scene!=SHUTDOWN)message(connected?"PC connected":"PC disconnected");
  }
  if(volumePending && now-lastVolume>=45 && scene!=SHUTDOWN){
   lastVolume=now;
   if(!connected){volumePending=0;message("Not connected");}
   else {bool up=volumePending>0;kbd.write(up?KEY_MEDIA_VOLUME_UP:KEY_MEDIA_VOLUME_DOWN);
    volumePending+=up?-1:1;message(up?"Volume +":"Volume -");}
  }
  if(scene==BOOT && now-epoch>=1400)scene=HOME;
  if(scene==SHUTDOWN && now-epoch>=1000){powerOff();return;}
  static const unsigned long durations[]={0,30000,60000,120000};
  if(!displaySleeping && scene!=BOOT && scene!=SHUTDOWN && timeout && now-lastActivity>=durations[timeout]){
   lcd.firstPage();do{}while(lcd.nextPage());displaySleeping=true;
  }
  if(displaySleeping||now-lastFrame<50)return;
  lastFrame=now;frameNow=now;frameConnected=connected;
  lcd.firstPage();do{draw();}while(lcd.nextPage());
 }
private:
 enum Scene{BOOT,HOME,APPS,MEDIA,ACTIONS,SETTINGS,CONNECTION,DISPLAY,CONTROLS,ABOUT,EDIT,CONFIRM,SHUTDOWN};
 enum EditKind{STEP,REVERSE,DETENTS,SOUND,TIMEOUT};
 enum ConfirmKind{POWER,FORGET};
 U8G2_ST7920_128X64_1_SW_SPI lcd;
 BleKeyboard kbd;Preferences store;
 Scene scene=BOOT,editReturn=CONTROLS,confirmReturn=SETTINGS;
 EditKind editKind=STEP;ConfirmKind confirmKind=POWER;
 int home=0,app=0,media=0,actionSel=0,setting=0,connection=0,control=0,confirm=0,dummy=0;
 int step=1,transitions=4,timeout=0,editValue=0,volumePending=0;
 bool reversed=false,sound=false,editVolume=false,advertiseWanted=false,wasConnected=false;
 bool raw=HIGH,stable=HIGH,held=false,inhibitButton=false,displaySleeping=false,frameConnected=false;
 unsigned long changed=0,pressed=0,epoch=0,lastFrame=0,frameNow=0,lastActivity=0,lastVolume=0,toastUntil=0;
 unsigned long buzzEnd=0,buzzToggle=0;bool buzzHigh=false;
 const char* toast=nullptr;
 const char* apps[6]={"Code","Chrome","Spotify","Discord","Files","OBS"};
 const char* actions[6]={"Shot","Mute","Lock","Search","Desktop","Tasks"};
 const char* categories[4]={"APPS","MEDIA","ACTIONS","SETTINGS"};
 void message(const char* s){toast=s;toastUntil=millis()+1000;}
 bool ready(){if(!kbd.isConnected()){message("Not connected");return false;}return true;}
 bool wakeDisplay(){if(!displaySleeping)return false;displaySleeping=false;lastActivity=millis();return true;}
 int& selection(){
  switch(scene){case HOME:return home;case APPS:return app;case MEDIA:return media;
   case ACTIONS:return actionSel;case SETTINGS:return setting;case CONNECTION:return connection;
   case CONTROLS:return control;case CONFIRM:return confirm;default:return dummy;}
 }
 void chord(uint8_t modifier,uint8_t key,uint8_t extra=0){
  kbd.press(modifier);if(extra)kbd.press(extra);kbd.press(key);kbd.releaseAll();
 }
 void launch(int index){
  if(!ready())return;
  if(index==4)chord(KEY_LEFT_GUI,'e');
  else {
   const uint8_t keys[]={KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6};
   kbd.press(KEY_LEFT_CTRL);kbd.press(KEY_LEFT_ALT);kbd.press(KEY_LEFT_SHIFT);
   kbd.press(keys[index]);kbd.releaseAll();
  }
  message("Shortcut sent");
 }
 void action(int index){
  if(!ready())return;
  switch(index){
   case 0:chord(KEY_LEFT_GUI,'s',KEY_LEFT_SHIFT);break;
   case 1:kbd.write(KEY_MEDIA_MUTE);break;
   case 2:chord(KEY_LEFT_GUI,'l');break;
   case 3:chord(KEY_LEFT_GUI,'s');break;
   case 4:chord(KEY_LEFT_GUI,'d');break;
   case 5:chord(KEY_LEFT_GUI,KEY_TAB);break;
  }message("Command sent");
 }
 void save(){
  store.putInt("step",step);store.putBool("reverse",reversed);
  store.putInt("timeout",timeout);store.putBool("sound",sound);store.putInt("detents",transitions);
 }
 int editMin(){return editKind==STEP?1:0;}
 int editMax(){return editKind==STEP?5:editKind==TIMEOUT?3:1;}
 void startEdit(EditKind kind,int value){editKind=kind;editValue=value;editReturn=scene;scene=EDIT;}
 void saveEdit(){
  switch(editKind){case STEP:step=editValue;break;case REVERSE:reversed=editValue;break;
   case DETENTS:transitions=editValue?4:2;break;case SOUND:sound=editValue;break;case TIMEOUT:timeout=editValue;break;}
  save();scene=editReturn;message("Setting saved");
 }
 void ask(ConfirmKind kind){confirmKind=kind;confirmReturn=scene;confirm=0;scene=CONFIRM;}
 void forgetPairing(){
  int count=esp_ble_get_bond_device_num();bool ok=true;
  if(count>0){
   esp_ble_bond_dev_t* list=(esp_ble_bond_dev_t*)calloc(count,sizeof(esp_ble_bond_dev_t));
   if(!list){scene=CONNECTION;message("Out of memory");return;}
   if(esp_ble_get_bond_device_list(&count,list)!=ESP_OK)ok=false;
   else for(int i=0;i<count;i++)if(esp_ble_remove_bond_device(list[i].bd_addr)!=ESP_OK)ok=false;
   free(list);
  }
  if(!ok){scene=CONNECTION;message("Bond clear failed");return;}
  // GAP removal completes asynchronously; allow time before resetting.
  message("Restarting...");lastFrame=0;renderOnce();delay(800);ESP.restart();
 }
 void powerOff(){
  digitalWrite(BUZZ,LOW);kbd.releaseAll();BLEDevice::stopAdvertising();
  lcd.firstPage();do{}while(lcd.nextPage());lcd.setPowerSave(1);save();
  // No 5V/backlight switch exists in the provided wiring.
  while(digitalRead(BTN)==LOW)delay(10);delay(40);
  rtc_gpio_init(GPIO_NUM_5);rtc_gpio_set_direction(GPIO_NUM_5,RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_5);rtc_gpio_pulldown_dis(GPIO_NUM_5);
  if(esp_sleep_enable_ext0_wakeup(GPIO_NUM_5,0)!=ESP_OK){ESP.restart();return;}
  Serial.flush();esp_deep_sleep_start();
 }
 void beep(){if(sound){buzzEnd=millis()+25;buzzToggle=micros();}}
 void buzzerTick(){
  if((long)(buzzEnd-millis())>0){if((unsigned long)(micros()-buzzToggle)>=250){buzzToggle=micros();buzzHigh=!buzzHigh;digitalWrite(BUZZ,buzzHigh);}}
  else {buzzHigh=false;digitalWrite(BUZZ,LOW);}
 }
 void text(int x,int y,const char* s){lcd.drawStr(x,y,s);}
 void center(int y,const char* s){text((128-lcd.getStrWidth(s))/2,y,s);}
 void bt(){
  if(!frameConnected && advertiseWanted && (frameNow/500)%2)return;
  lcd.drawLine(120,0,120,10);lcd.drawLine(120,0,124,3);lcd.drawLine(124,3,116,8);
  lcd.drawLine(116,2,124,7);lcd.drawLine(124,7,120,10);
  if(!frameConnected&&!advertiseWanted)lcd.drawLine(115,10,125,0);
 }
 void header(const char* title){text(1,8,title);bt();lcd.drawHLine(0,11,128);}
 void footer(const char* s){lcd.setDrawColor(1);lcd.drawHLine(0,55,128);text(1,63,s);}
 void glyph(int x,int y,int type){
  switch(type){
   case 0:for(int r=0;r<2;r++)for(int c=0;c<2;c++)lcd.drawBox(x+c*6,y+r*6,4,4);break;
   case 1:lcd.drawLine(x+4,y,x+4,y+8);lcd.drawLine(x+4,y,x+10,y+2);lcd.drawDisc(x+2,y+9,2);break;
   case 2:lcd.drawLine(x+8,y,x+2,y+5);lcd.drawLine(x+2,y+5,x+7,y+5);lcd.drawLine(x+7,y+5,x+3,y+11);break;
   case 3:lcd.drawCircle(x+5,y+5,4);lcd.drawDisc(x+5,y+5,1);lcd.drawLine(x+5,y-1,x+5,y+11);lcd.drawLine(x-1,y+5,x+11,y+5);break;
   case 4:lcd.drawLine(x+4,y,x,y+5);lcd.drawLine(x,y+5,x+4,y+10);lcd.drawLine(x+8,y,x+12,y+5);lcd.drawLine(x+12,y+5,x+8,y+10);break;
   case 5:lcd.drawCircle(x+5,y+5,5);lcd.drawVLine(x+5,y,11);lcd.drawHLine(x,y+5,11);break;
   case 6:lcd.drawCircle(x+5,y+5,5);lcd.drawLine(x+2,y+3,x+8,y+3);lcd.drawLine(x+2,y+5,x+7,y+5);lcd.drawLine(x+3,y+7,x+7,y+7);break;
   case 7:lcd.drawRFrame(x,y+1,12,9,2);lcd.drawDisc(x+3,y+5,1);lcd.drawDisc(x+8,y+5,1);break;
   case 8:lcd.drawFrame(x,y+3,12,8);lcd.drawFrame(x+1,y,5,4);break;
   case 9:lcd.drawCircle(x+5,y+5,5);lcd.drawCircle(x+3,y+4,2);lcd.drawCircle(x+7,y+4,2);lcd.drawCircle(x+5,y+8,2);break;
   case 10:lcd.drawFrame(x,y+2,12,8);lcd.drawCircle(x+6,y+6,2);lcd.drawHLine(x+3,y,5);break;
   case 11:lcd.drawBox(x,y+3,3,5);lcd.drawTriangle(x+3,y+3,x+7,y,x+7,y+11);lcd.drawLine(x+8,y+2,x+12,y+8);break;
   case 12:lcd.drawFrame(x+1,y+5,10,7);lcd.drawRFrame(x+3,y,6,8,2);break;
   case 13:lcd.drawCircle(x+4,y+4,4);lcd.drawLine(x+7,y+7,x+11,y+11);break;
   case 14:lcd.drawFrame(x,y,12,8);lcd.drawVLine(x+6,y+8,3);lcd.drawHLine(x+3,y+11,7);break;
   case 15:lcd.drawFrame(x,y,8,8);lcd.drawFrame(x+4,y+4,8,8);break;
   case 16:lcd.drawCircle(x+5,y+6,5);lcd.drawVLine(x+5,y-1,7);break;
  }
 }
 void list(const char* title,const char* const* rows,int count,int selected){
  header(title);int start=selected>=4?selected-3:0;
  for(int i=0;i<4&&start+i<count;i++){
   int y=13+i*10;bool active=start+i==selected;
   lcd.setDrawColor(1);if(active)lcd.drawBox(0,y,128,10);
   lcd.setDrawColor(active?0:1);text(3,y+8,rows[start+i]);text(119,y+8,">");
  }footer("Press: open / Hold: back");
 }
 void renderOnce(){frameNow=millis();frameConnected=kbd.isConnected();lcd.firstPage();do{draw();}while(lcd.nextPage());}
 void draw(){
  lcd.setFont(u8g2_font_5x7_tr);lcd.setDrawColor(1);
  if(scene==BOOT||scene==SHUTDOWN){
   int elapsed=frameNow-epoch;int r=scene==BOOT?min(18,elapsed/35):max(0,18-elapsed/50);
   if(r>0)lcd.drawCircle(64,24,r);
   glyph(59,19,scene==BOOT?0:16);center(50,scene==BOOT?"ROTARY DECK":"Turning off...");return;
  }
  if(scene==HOME){
   header("ROTARY DECK");lcd.drawCircle(64,32,18);
   const int xs[]={58,96,58,19},ys[]={14,27,41,27};
   for(int i=0;i<4;i++){
    lcd.setDrawColor(1);if(i==home)lcd.drawBox(xs[i]-2,ys[i]-1,16,14);
    lcd.setDrawColor(i==home?0:1);glyph(xs[i],ys[i],i);
   }
   lcd.setDrawColor(1);center(35,categories[home]);footer("Turn: browse / Press: open");
  }else if(scene==APPS||scene==ACTIONS){
   bool appPage=scene==APPS;int sel=appPage?app:actionSel;header(appPage?"APPS":"ACTIONS");
   for(int i=0;i<6;i++){
    int x=(i%3)*43,y=13+(i/3)*21;
    lcd.setDrawColor(1);if(sel==i)lcd.drawBox(x,y,41,20);else lcd.drawFrame(x,y,41,20);
    lcd.setDrawColor(sel==i?0:1);glyph(x+15,y+1,appPage?i+4:i+10);
    const char* s=appPage?apps[i]:actions[i];text(x+(41-lcd.getStrWidth(s))/2,y+19,s);
   }footer("Press: send / Hold: back");
  }else if(scene==MEDIA){
   header("MEDIA");center(23,editVolume?"ADJUST VOLUME":"MEDIA CONTROLS");
   for(int i=0;i<5;i++){
    int x=i*25+1;lcd.setDrawColor(1);if(i==media)lcd.drawBox(x,28,24,24);
    lcd.setDrawColor(i==media?0:1);
    if(i==0){lcd.drawBox(x+4,34,3,6);lcd.drawTriangle(x+7,34,x+13,30,x+13,43);}
    if(i==1||i==3){lcd.drawTriangle(x+(i==1?16:7),31,x+(i==1?16:7),42,x+(i==1?7:16),36);lcd.drawVLine(x+(i==1?5:18),31,12);}
    if(i==2){lcd.drawTriangle(x+6,31,x+6,42,x+14,36);lcd.drawVLine(x+17,31,12);}
    if(i==4)glyph(x+5,31,11);
   }footer(editVolume?"Turn: adjust / Press: OK":"Press: send / Hold: back");
  }else if(scene==SETTINGS){
   const char* rows[]={"Connection","Display","Controls","About","Turn off"};list("SETTINGS",rows,5,setting);
  }else if(scene==CONNECTION){
   const char* rows[]={frameConnected?"Status: connected":"Status: disconnected",advertiseWanted?"Discoverable: on":"Discoverable: off","Forget pairing"};list("CONNECTION",rows,3,connection);
  }else if(scene==DISPLAY){const char* rows[]={"Screen timeout"};list("DISPLAY",rows,1,0);
  }else if(scene==CONTROLS){const char* rows[]={"Volume command step","Encoder direction","Encoder detents","Button sound"};list("CONTROLS",rows,4,control);
  }else if(scene==ABOUT){header("ABOUT");center(25,"Rotary Deck v1.0");center(37,"XIAO ESP32-S3");center(49,"BLE HID / 128 x 64");footer("Hold: back");
  }else if(scene==EDIT){
   header("EDIT SETTING");char value[25];const char* label="";
   if(editKind==STEP){label="Commands per detent";snprintf(value,sizeof(value),"%d",editValue);}
   if(editKind==REVERSE){label="Encoder direction";snprintf(value,sizeof(value),"%s",editValue?"Reversed":"Normal");}
   if(editKind==DETENTS){label="Transitions / detent";snprintf(value,sizeof(value),"%d",editValue?4:2);}
   if(editKind==SOUND){label="Button sound";snprintf(value,sizeof(value),"%s",editValue?"On":"Off");}
   if(editKind==TIMEOUT){label="Screen timeout";const char* values[]={"Never","30 seconds","60 seconds","120 seconds"};snprintf(value,sizeof(value),"%s",values[editValue]);}
   center(27,label);center(44,value);footer("Press: save / Hold: cancel");
  }else if(scene==CONFIRM){
   header(confirmKind==POWER?"POWER":"CONNECTION");center(27,confirmKind==POWER?"Turn off the deck?":"Forget all pairings?");
   for(int i=0;i<2;i++){int x=28+i*47;lcd.setDrawColor(1);if(i==confirm)lcd.drawBox(x,34,28,14);lcd.setDrawColor(i==confirm?0:1);text(x+6,44,i?"YES":"NO");}
   footer("Press: confirm / Hold: back");
  }
  if(toast && (long)(toastUntil-frameNow)>0){lcd.setDrawColor(1);lcd.drawBox(7,24,114,16);lcd.setDrawColor(0);center(35,toast);lcd.setDrawColor(1);}
 }
};
