#pragma once
#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <stdlib.h>

// Physical XIAO ESP32-S3 / EXP3. Target core 2.0.17, T-vK BleKeyboard 0.3.0.
namespace DeckConfig {
constexpr int CLOCK=7, DATA=9, CS=44;      // D8, D10, D7
constexpr int ENC_A=1, ENC_B=4, BUTTON=5, BEEPER=6; // D0,D3,D4,D5
constexpr unsigned long DEBOUNCE=25, DOUBLE_CLICK=320, HOLD=700;
constexpr unsigned long BOOT=1400, SHUTDOWN=1000, FEEDBACK=1000;
}
enum class Screen : uint8_t { Boot,Home,Apps,Media,Actions,Settings,Connection,DisplayPage,Controls,About,Edit,Confirm,Shutdown };
enum class EditField : uint8_t { Step,Reverse,Edges,Sound,Timeout,Estimate };
enum class Confirmation : uint8_t { Power,Forget };
struct Options { int step=1,edges=4,timeout=0;bool reverse=false,sound=false,invert=false; };
struct DeckState {
 Screen screen=Screen::Boot;Options options;
 int home=0,app=0,media=0,action=0,setting=0,connection=0,display=0,control=0;
 bool connected=false,advertising=true,volumeEdit=false,blank=false;
 int estimate=-1; // Unknown until explicitly calibrated by the user. Never host feedback.
 EditField editField=EditField::Step;int editValue=0;
 Confirmation confirmation=Confirmation::Power;int choice=0;
 unsigned long animationStart=0,feedbackStart=0;
 const char* feedback=nullptr;
};
static portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED;
static volatile int32_t encoderEdges=0;
static volatile uint8_t previousAB=0;
static DRAM_ATTR int8_t quadratureTable[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
static void IRAM_ATTR encoderInterrupt(){
 uint8_t ab=(digitalRead(DeckConfig::ENC_A)<<1)|digitalRead(DeckConfig::ENC_B);
 portENTER_CRITICAL_ISR(&encoderMux);
 encoderEdges+=quadratureTable[(previousAB<<2)|ab];previousAB=ab;
 portEXIT_CRITICAL_ISR(&encoderMux);
}
class DeckCore {
public:
 DeckCore():keyboard_("Rotary Deck","DIY",100){}
 void begin(){
  rtc_gpio_deinit(GPIO_NUM_5);
  pinMode(DeckConfig::ENC_A,INPUT_PULLUP);pinMode(DeckConfig::ENC_B,INPUT_PULLUP);
  pinMode(DeckConfig::BUTTON,INPUT_PULLUP);pinMode(DeckConfig::BEEPER,OUTPUT);
  digitalWrite(DeckConfig::BEEPER,LOW);
  storage_=prefs_.begin("rotarydeck",false);
  if(storage_){
   s_.options.step=constrain(prefs_.getInt("step",1),1,5);
   s_.options.edges=prefs_.getInt("detents",4)==2?2:4;
   s_.options.timeout=constrain(prefs_.getInt("timeout",0),0,3);
   s_.options.reverse=prefs_.getBool("reverse",false);
   s_.options.sound=prefs_.getBool("sound",false);
   s_.options.invert=prefs_.getBool("invert",false);
  }
  threshold_=s_.options.edges;
  raw_=stable_=digitalRead(DeckConfig::BUTTON);ignoreRelease_=raw_==LOW;
  changed_=millis();
  previousAB=(digitalRead(DeckConfig::ENC_A)<<1)|digitalRead(DeckConfig::ENC_B);
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_A),encoderInterrupt,CHANGE);
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_B),encoderInterrupt,CHANGE);
  keyboard_.begin();keyboard_.setDelay(8);
  s_.animationStart=activity_=millis();
 }
 const DeckState& state()const{return s_;}
 bool sleepRequested()const{return sleep_;}
 bool restartRequested()const{return restart_;}
 void update(){
  unsigned long now=millis();updateSound();updateLink();updateBondClear(now);
  if(s_.screen==Screen::Boot && now-s_.animationStart>=DeckConfig::BOOT)s_.screen=Screen::Home;
  if(s_.screen==Screen::Shutdown && now-s_.animationStart>=DeckConfig::SHUTDOWN)sleep_=true;
  readEncoder();readButton();sendVolume(now);
  if(restartScheduled_ && now-restartAt_>=800)restart_=true;
  static const unsigned long durations[]={0,30000,60000,120000};
  if(s_.screen!=Screen::Boot && s_.screen!=Screen::Shutdown && !clearing_ && !restartScheduled_ &&
     s_.options.timeout && now-activity_>=durations[s_.options.timeout]){
   s_.blank=true;waitingClick_=false;
  }
 }
 void enterSleep(){
  save();volumeQueue_=0;keyboard_.releaseAll();BLEDevice::stopAdvertising();
  digitalWrite(DeckConfig::BEEPER,LOW);
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_A));
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_B));
  while(digitalRead(DeckConfig::BUTTON)==LOW)delay(10);
  delay(40);
  rtc_gpio_init(GPIO_NUM_5);rtc_gpio_set_direction(GPIO_NUM_5,RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_5);rtc_gpio_pulldown_dis(GPIO_NUM_5);
  if(esp_sleep_enable_ext0_wakeup(GPIO_NUM_5,0)!=ESP_OK){ESP.restart();return;}
  Serial.flush();esp_deep_sleep_start();
 }
private:
 BleKeyboard keyboard_;Preferences prefs_;DeckState s_;
 Screen editReturn_=Screen::Controls,confirmReturn_=Screen::Settings;
 bool storage_=false,sleep_=false,restart_=false,clearing_=false,restartScheduled_=false;
 bool raw_=HIGH,stable_=HIGH,held_=false,ignoreRelease_=false,waitingClick_=false;
 int remainder_=0,threshold_=4,volumeQueue_=0;
 unsigned long changed_=0,pressed_=0,clickAt_=0,activity_=0,lastVolume_=0,clearAt_=0,restartAt_=0;
 bool soundActive_=false,soundLevel_=false;
 unsigned long soundAt_=0,soundToggle_=0;
 void note(const char* value){s_.feedback=value;s_.feedbackStart=millis();}
 bool blocked()const{return s_.screen==Screen::Boot || s_.screen==Screen::Shutdown || clearing_ || restartScheduled_;}
 bool wakeInput(){
  activity_=millis();
  if(!s_.blank)return false;
  s_.blank=false;waitingClick_=false;return true;
 }
 bool ready(){if(!s_.connected){note("NOT CONNECTED");return false;}return true;}
 void touch(){activity_=millis();if(s_.options.sound){soundActive_=true;soundAt_=millis();soundToggle_=micros();}}
 void readEncoder(){
  portENTER_CRITICAL(&encoderMux);int32_t edges=encoderEdges;encoderEdges=0;portEXIT_CRITICAL(&encoderMux);
  if(threshold_!=s_.options.edges){threshold_=s_.options.edges;remainder_=0;}
  remainder_+=edges;int turns=remainder_/threshold_;remainder_-=turns*threshold_;
  if(!turns || blocked())return;
  waitingClick_=false; // A pending click must not act on a newly rotated selection.
  if(wakeInput())return;
  touch();if(s_.options.reverse)turns=-turns;
  if(s_.screen==Screen::Media && s_.volumeEdit){volumeQueue_=constrain(volumeQueue_+turns*s_.options.step,-30,30);return;}
  if(s_.screen==Screen::Edit){
   int low=s_.editField==EditField::Step?1:0;
   int high=s_.editField==EditField::Step?5:s_.editField==EditField::Timeout?3:s_.editField==EditField::Estimate?100:1;
   s_.editValue=constrain(s_.editValue+turns,low,high);return;
  }
  int* value=nullptr;int count=0;
  switch(s_.screen){
   case Screen::Home:value=&s_.home;count=4;break;
   case Screen::Apps:value=&s_.app;count=6;break;
   case Screen::Actions:value=&s_.action;count=6;break;
   case Screen::Media:value=&s_.media;count=5;break;
   case Screen::Settings:value=&s_.setting;count=5;break;
   case Screen::Connection:value=&s_.connection;count=3;break;
   case Screen::DisplayPage:value=&s_.display;count=3;break;
   case Screen::Controls:value=&s_.control;count=4;break;
   case Screen::Confirm:value=&s_.choice;count=2;break;
   default:break;
  }
  if(value)*value=((*value+turns)%count+count)%count;
 }
 void readButton(){
  unsigned long now=millis();bool value=digitalRead(DeckConfig::BUTTON);
  if(value!=raw_){raw_=value;changed_=now;}
  if(ignoreRelease_){
   if(raw_==HIGH && now-changed_>=DeckConfig::DEBOUNCE){ignoreRelease_=false;stable_=HIGH;held_=false;}
   return;
  }
  if(now-changed_>=DeckConfig::DEBOUNCE && stable_!=raw_){
   stable_=raw_;
   if(stable_==LOW){
    pressed_=now;held_=false;
    if(s_.blank && !blocked()){wakeInput();ignoreRelease_=true;waitingClick_=false;return;}
   }else if(!held_ && !blocked()){
    touch();
    if(waitingClick_ && now-clickAt_<=DeckConfig::DOUBLE_CLICK){waitingClick_=false;home();}
    else {waitingClick_=true;clickAt_=now;}
   }
  }
  if(stable_==LOW && !held_ && now-pressed_>=DeckConfig::HOLD){
   held_=true;waitingClick_=false;if(!blocked()){touch();back();}
  }
  // Defer single click so the first half of a double click sends no action.
  // Do not resolve it while a second press is physically held/debouncing.
  if(waitingClick_ && raw_==HIGH && stable_==HIGH && now-clickAt_>DeckConfig::DOUBLE_CLICK){
   waitingClick_=false;if(!blocked())activate();
  }
 }
 void home(){s_.screen=Screen::Home;s_.volumeEdit=false;volumeQueue_=0;s_.feedback=nullptr;}
 void back(){
  if(s_.screen==Screen::Media && s_.volumeEdit){s_.volumeEdit=false;volumeQueue_=0;return;}
  if(s_.screen==Screen::Edit){s_.screen=editReturn_;return;}
  if(s_.screen==Screen::Confirm){s_.screen=confirmReturn_;return;}
  if(s_.screen==Screen::Connection || s_.screen==Screen::DisplayPage || s_.screen==Screen::Controls || s_.screen==Screen::About)
   s_.screen=Screen::Settings;
  else home();
 }
 void activate(){
  switch(s_.screen){
   case Screen::Home:{const Screen pages[]={Screen::Apps,Screen::Media,Screen::Settings,Screen::Actions};s_.screen=pages[s_.home];break;}
   case Screen::Apps:launch();break;
   case Screen::Actions:action();break;
   case Screen::Media:
    if(s_.media==0){s_.volumeEdit=!s_.volumeEdit;volumeQueue_=0;}
    else if(ready()){
     if(s_.media==1)keyboard_.write(KEY_MEDIA_PREVIOUS_TRACK);
     if(s_.media==2)keyboard_.write(KEY_MEDIA_PLAY_PAUSE);
     if(s_.media==3)keyboard_.write(KEY_MEDIA_NEXT_TRACK);
     if(s_.media==4)keyboard_.write(KEY_MEDIA_MUTE);
     note("COMMAND SENT");
    }break;
   case Screen::Settings:{
    const Screen pages[]={Screen::Connection,Screen::DisplayPage,Screen::Controls,Screen::About};
    if(s_.setting<4)s_.screen=pages[s_.setting];else ask(Confirmation::Power);break;
   }
   case Screen::Connection:
    if(s_.connection==0)note(s_.connected?"CONNECTED":"NOT CONNECTED");
    else if(s_.connection==1){
     if(s_.connected)note("DISCONNECT PC FIRST");
     else {s_.advertising=!s_.advertising;if(s_.advertising)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();}
    }else ask(Confirmation::Forget);break;
   case Screen::DisplayPage:
    if(s_.display==0){s_.options.invert=!s_.options.invert;note(save()?"DISPLAY SAVED":"SAVE FAILED");}
    if(s_.display==1)edit(EditField::Timeout,s_.options.timeout);
    if(s_.display==2)edit(EditField::Estimate,s_.estimate<0?50:s_.estimate);
    break;
   case Screen::Controls:
    if(s_.control==0)edit(EditField::Step,s_.options.step);
    if(s_.control==1)edit(EditField::Reverse,s_.options.reverse);
    if(s_.control==2)edit(EditField::Edges,s_.options.edges==4);
    if(s_.control==3)edit(EditField::Sound,s_.options.sound);
    break;
   case Screen::Edit:commitEdit();break;
   case Screen::Confirm:
    if(s_.choice==0)s_.screen=confirmReturn_;
    else if(s_.confirmation==Confirmation::Power){s_.screen=Screen::Shutdown;s_.animationStart=millis();s_.feedback=nullptr;volumeQueue_=0;s_.volumeEdit=false;}
    else clearPairing();
    break;
   case Screen::About:s_.screen=Screen::Settings;break;
   default:break;
  }
 }
 void chord(uint8_t modifier,uint8_t key,uint8_t extra=0){
  keyboard_.press(modifier);if(extra)keyboard_.press(extra);keyboard_.press(key);keyboard_.releaseAll();
 }
 void launch(){
  if(!ready())return;
  // UI order: Chrome, Spotify, Discord, Files, OBS, Code.
  if(s_.app==3)chord(KEY_LEFT_GUI,'e');
  else {
   const uint8_t keys[]={KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F1};
   keyboard_.press(KEY_LEFT_CTRL);keyboard_.press(KEY_LEFT_ALT);keyboard_.press(KEY_LEFT_SHIFT);
   keyboard_.press(keys[s_.app]);keyboard_.releaseAll();
  }note("SHORTCUT SENT");
 }
 void action(){
  if(!ready())return;
  switch(s_.action){
   case 0:chord(KEY_LEFT_GUI,'s',KEY_LEFT_SHIFT);break;
   case 1:keyboard_.write(KEY_MEDIA_MUTE);break;
   case 2:chord(KEY_LEFT_GUI,'l');break;
   case 3:chord(KEY_LEFT_GUI,'s');break;
   case 4:chord(KEY_LEFT_GUI,'d');break;
   case 5:chord(KEY_LEFT_GUI,KEY_TAB);break;
  }note("COMMAND SENT");
 }
 void updateLink(){
  bool linked=keyboard_.isConnected();if(linked==s_.connected)return;
  s_.connected=linked;volumeQueue_=0;s_.estimate=-1;
  if(!linked){if(s_.advertising)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();}
  if(s_.screen!=Screen::Boot && s_.screen!=Screen::Shutdown)note(linked?"PC CONNECTED":"PC DISCONNECTED");
 }
 void sendVolume(unsigned long now){
  if(!volumeQueue_ || now-lastVolume_<45)return;
  lastVolume_=now;
  if(!s_.connected){volumeQueue_=0;note("NOT CONNECTED");return;}
  bool up=volumeQueue_>0;keyboard_.write(up?KEY_MEDIA_VOLUME_UP:KEY_MEDIA_VOLUME_DOWN);
  volumeQueue_+=up?-1:1;
  if(s_.estimate>=0)s_.estimate=constrain(s_.estimate+(up?2:-2),0,100);
  note(up?"VOLUME + SENT":"VOLUME - SENT");
 }
 void edit(EditField field,int value){editReturn_=s_.screen;s_.editField=field;s_.editValue=value;s_.screen=Screen::Edit;}
 void commitEdit(){
  if(s_.editField==EditField::Estimate){s_.estimate=s_.editValue;s_.screen=editReturn_;note("ESTIMATE SET; NOT LIVE");return;}
  switch(s_.editField){
   case EditField::Step:s_.options.step=s_.editValue;break;
   case EditField::Reverse:s_.options.reverse=s_.editValue;break;
   case EditField::Edges:s_.options.edges=s_.editValue?4:2;break;
   case EditField::Sound:s_.options.sound=s_.editValue;break;
   case EditField::Timeout:s_.options.timeout=s_.editValue;break;
   default:break;
  }
  s_.screen=editReturn_;note(save()?"SETTING SAVED":"SAVE FAILED");
 }
 bool save(){
  if(!storage_)return false;
  bool ok=true;
  ok=(prefs_.putInt("step",s_.options.step)>0)&&ok;
  ok=(prefs_.putInt("detents",s_.options.edges)>0)&&ok;
  ok=(prefs_.putInt("timeout",s_.options.timeout)>0)&&ok;
  ok=(prefs_.putBool("reverse",s_.options.reverse)>0)&&ok;
  ok=(prefs_.putBool("sound",s_.options.sound)>0)&&ok;
  ok=(prefs_.putBool("invert",s_.options.invert)>0)&&ok;return ok;
 }
 void ask(Confirmation kind){confirmReturn_=s_.screen;s_.confirmation=kind;s_.choice=0;s_.screen=Screen::Confirm;}
 void clearPairing(){
  s_.screen=Screen::Connection;
  if(s_.connected){note("DISCONNECT PC FIRST");return;}
  int count=esp_ble_get_bond_device_num();bool ok=true;
  if(count>0){
   esp_ble_bond_dev_t* devices=(esp_ble_bond_dev_t*)calloc(count,sizeof(esp_ble_bond_dev_t));
   if(!devices){note("OUT OF MEMORY");return;}
   if(esp_ble_get_bond_device_list(&count,devices)!=ESP_OK)ok=false;
   else for(int i=0;i<count;i++)if(esp_ble_remove_bond_device(devices[i].bd_addr)!=ESP_OK)ok=false;
   free(devices);
  }
  if(!ok){note("PAIRING CLEAR FAILED");return;}
  clearing_=true;clearAt_=millis();note("CLEARING PAIRING...");
 }
 void updateBondClear(unsigned long now){
  if(!clearing_)return;
  if(esp_ble_get_bond_device_num()==0){clearing_=false;restartScheduled_=true;restartAt_=now;note("CLEARED; RESTARTING");}
  else if(now-clearAt_>4000){clearing_=false;note("PAIRING CLEAR TIMEOUT");}
 }
 void updateSound(){
  if(soundActive_ && millis()-soundAt_<25){
   if(micros()-soundToggle_>=250){soundToggle_=micros();soundLevel_=!soundLevel_;digitalWrite(DeckConfig::BEEPER,soundLevel_);}
  }else {soundActive_=false;digitalWrite(DeckConfig::BEEPER,LOW);}
 }
};
