#pragma once
#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <stdlib.h>
#include <time.h>

// Target: XIAO ESP32-S3; Arduino-ESP32 2.0.17; T-vK BLE Keyboard 0.3.0.
namespace DeckConfig {
constexpr int CLOCK=7,DATA=9,CS=44,ENC_A=1,ENC_B=4,BUTTON=5,BEEPER=6;
  
constexpr unsigned long FEEDBACK=1000,DOUBLE_MS=320,HOLD_MS=700;
  
constexpr const char* SERVICE_UUID="917e0001-d588-4b6b-a9a0-a81a964ce2b1";
  
constexpr const char* RX_UUID="917e0002-d588-4b6b-a9a0-a81a964ce2b1";
  
}
enum class Screen:uint8_t{Boot,Dashboard,Home,Apps,Media,Actions,Settings,Connection,DisplayPage,Controls,About,Edit,Confirm,Shutdown};
  
enum class EditField:uint8_t{Step,Reverse,Edges,Sound,Timeout,Estimate,BackGesture};
  
enum class Confirmation:uint8_t{Power,Forget};
  
struct Options{int step=1,edges=4,timeout=0;
  bool reverse=false,sound=false,invert=false,backDouble=true;
  };
  
struct DeckState {
 Screen screen=Screen::Boot;
  Options options;
  
 int dashboard=1,home=0,app=0,media=0,action=0,setting=0,connection=0,display=0,control=0;
  
 bool connected=false,advertising=true,volumeEdit=false,blank=false;
  
 int estimate=-1;
  EditField editField=EditField::Step;
  int editValue=0;
  
 Confirmation confirmation=Confirmation::Power;
  int choice=0;
  
 unsigned long animationStart=0,feedbackStart=0;
  const char* feedback=nullptr;
  
 bool syncReady=false,pcSeen=false,timeValid=false;
  
 uint32_t utc=0,timeAt=0,lastPcAt=0,lastMediaAt=0,titleAt=0;
  
 int16_t utcOffset=0;
  uint8_t playback=0,mediaSession=0;
  
 char title[129]={};
  
};
  
static portMUX_TYPE encoderMux=portMUX_INITIALIZER_UNLOCKED,rxMux=portMUX_INITIALIZER_UNLOCKED;
  
static volatile int32_t encoderEdges=0;
  static volatile uint8_t previousAB=0;
  
static DRAM_ATTR int8_t quadrature[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  
static void IRAM_ATTR encoderISR(){
 uint8_t ab=(digitalRead(DeckConfig::ENC_A)<<1)|digitalRead(DeckConfig::ENC_B);
  
 portENTER_CRITICAL_ISR(&encoderMux);
  encoderEdges+=quadrature[(previousAB<<2)|ab];
  previousAB=ab;
  portEXIT_CRITICAL_ISR(&encoderMux);
  
}
struct RxPacket{uint8_t size,data[20];
  };
  
static RxPacket rxQueue[16];
  static volatile uint8_t rxHead=0,rxTail=0;
  
class DeckRxCallbacks:public BLECharacteristicCallbacks {
 void onWrite(BLECharacteristic* characteristic) override {
  std::string value=characteristic->getValue();
  if(value.empty()||value.size()>20)return;
  
  portENTER_CRITICAL(&rxMux);
  uint8_t next=(rxHead+1)%16;
  
  if(next!=rxTail){RxPacket& p=rxQueue[rxHead];
  p.size=value.size();
  memcpy(p.data,value.data(),value.size());
  rxHead=next;
  }
  portEXIT_CRITICAL(&rxMux);
  
 }
};
  
static DeckRxCallbacks rxCallbacks;
  
// Capture the HID server via T-vK BLE Keyboard's supported startup hook.
class DeckKeyboard : public BleKeyboard {
public:
  DeckKeyboard(const char* name, const char* manufacturer, uint8_t battery)
    : BleKeyboard(name, manufacturer, battery) {}

  BLEServer* server() const { return server_; }

protected:
  void onStarted(BLEServer* server) override {
    server_ = server;
  }

private:
  BLEServer* server_ = nullptr;
};
class DeckCore {
public:
 DeckCore():keyboard_("Rotary Deck","DIY",100){}
 void begin(){
  rtc_gpio_deinit(GPIO_NUM_5);
  
  pinMode(DeckConfig::ENC_A,INPUT_PULLUP);
  pinMode(DeckConfig::ENC_B,INPUT_PULLUP);
  pinMode(DeckConfig::BUTTON,INPUT_PULLUP);
  
  pinMode(DeckConfig::BEEPER,OUTPUT);
  digitalWrite(DeckConfig::BEEPER,LOW);
  
  storage_=prefs_.begin("rotarydeck",false);
  
  if(storage_){
   s_.options.step=constrain(prefs_.getInt("step",1),1,5);
  s_.options.edges=prefs_.getInt("detents",4)==2?2:4;
  
   s_.options.timeout=constrain(prefs_.getInt("timeout",0),0,3);
  s_.options.reverse=prefs_.getBool("reverse",false);
  
   s_.options.sound=prefs_.getBool("sound",false);
  s_.options.invert=prefs_.getBool("invert",false);
  
   s_.options.backDouble=prefs_.getBool("backdbl",true);
  
  }
  threshold_=s_.options.edges;
  raw_=stable_=digitalRead(DeckConfig::BUTTON);
  ignoreRelease_=raw_==LOW;
  changed_=millis();
  
  previousAB=(digitalRead(DeckConfig::ENC_A)<<1)|digitalRead(DeckConfig::ENC_B);
  
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_A),encoderISR,CHANGE);
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_B),encoderISR,CHANGE);
  
  keyboard_.begin();
  keyboard_.setDelay(8);
  
  // Add a separate encrypted-write service to the same server used by HID.
  BLEDevice::stopAdvertising();
  BLEServer* server=keyboard_.server();
  
  if(server){
   BLEService* service=server->createService(DeckConfig::SERVICE_UUID);
  
   BLECharacteristic* rx=service->createCharacteristic(DeckConfig::RX_UUID,BLECharacteristic::PROPERTY_WRITE);
  
   rx->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  rx->setCallbacks(&rxCallbacks);
  service->start();
  
   BLEAdvertising* advertising=BLEDevice::getAdvertising();
  
   advertising->addServiceUUID(DeckConfig::SERVICE_UUID);
  advertising->setScanResponse(true);
  s_.syncReady=true;
  
  }
  BLEDevice::startAdvertising();
  s_.animationStart=activity_=millis();
  
 }
 const DeckState& state()const{return s_;
  }
 bool sleepRequested()const{return sleep_;
  }bool restartRequested()const{return restart_;
  }
 void update(){
  uint32_t now=millis();
  soundTick();
  linkTick();
  receivePc(now);
  bondTick(now);
  
  if(s_.screen==Screen::Boot && now-s_.animationStart>=1400)s_.screen=Screen::Dashboard;
  
  if(s_.screen==Screen::Shutdown && now-s_.animationStart>=1000)sleep_=true;
  
  readEncoder();
  readButton();
  volumeTick(now);
  
  if(restartScheduled_ && now-restartAt_>=800)restart_=true;
  
  const unsigned long durations[]={0,30000,60000,120000};
  
  if(!blocked() && s_.options.timeout && now-activity_>=durations[s_.options.timeout]){s_.blank=true;
  waiting_=false;
  }
 }
 void enterSleep(){
  save();
  volumeQueue_=0;
  keyboard_.releaseAll();
  BLEDevice::stopAdvertising();
  digitalWrite(DeckConfig::BEEPER,LOW);
  
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_A));
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENC_B));
  
  while(digitalRead(DeckConfig::BUTTON)==LOW)delay(10);
  
  delay(40);
  rtc_gpio_init(GPIO_NUM_5);
  rtc_gpio_set_direction(GPIO_NUM_5,RTC_GPIO_MODE_INPUT_ONLY);
  
  rtc_gpio_pullup_en(GPIO_NUM_5);
  rtc_gpio_pulldown_dis(GPIO_NUM_5);
  
  if(esp_sleep_enable_ext0_wakeup(GPIO_NUM_5,0)!=ESP_OK){ESP.restart();
  return;
  }
  Serial.flush();
  esp_deep_sleep_start();
  
 }
private:
 DeckKeyboard keyboard_;
  Preferences prefs_;
  DeckState s_;
  
 Screen editReturn_=Screen::Controls,confirmReturn_=Screen::Settings;
  
 bool storage_=false,sleep_=false,restart_=false,clearing_=false,restartScheduled_=false;
  
 bool raw_=HIGH,stable_=HIGH,held_=false,ignoreRelease_=false,waiting_=false;
  
 int threshold_=4,remainder_=0,volumeQueue_=0;
  
 uint32_t changed_=0,pressed_=0,clickAt_=0,activity_=0,lastVolume_=0,clearAt_=0,restartAt_=0;
  
 bool soundActive_=false,soundLevel_=false;
  uint32_t soundAt_=0,soundToggle_=0;
  
 char titleAssembly_[129]={};
  uint8_t titleSequence_=0,titleNext_=0,titleCount_=0;
  size_t titleBytes_=0;
  
 void note(const char* v){s_.feedback=v;
  s_.feedbackStart=millis();
  }
 bool blocked()const{return s_.screen==Screen::Boot||s_.screen==Screen::Shutdown||clearing_||restartScheduled_;
  }
 bool wakeInput(){activity_=millis();
  if(!s_.blank)return false;
  s_.blank=false;
  waiting_=false;
  return true;
  }
 void touch(){activity_=millis();
  if(s_.options.sound){soundActive_=true;
  soundAt_=millis();
  soundToggle_=micros();
  }}
 bool ready(){if(!s_.connected){note("NOT CONNECTED");
  return false;
  }return true;
  }
 void receivePc(uint32_t now){
  RxPacket p;
  
  for(int budget=0;budget<16;budget++){
   bool available=false;
  portENTER_CRITICAL(&rxMux);
  
   if(rxTail!=rxHead){p=rxQueue[rxTail];
  rxTail=(rxTail+1)%16;
  available=true;
  }
   portEXIT_CRITICAL(&rxMux);
  if(!available)break;
  
   if(!s_.connected)continue;
  
   if(p.data[0]==1 && p.size==8){
    uint32_t utc=uint32_t(p.data[2])|(uint32_t(p.data[3])<<8)|(uint32_t(p.data[4])<<16)|(uint32_t(p.data[5])<<24);
  
    int16_t offset=int16_t(uint16_t(p.data[6])|(uint16_t(p.data[7])<<8));
  
    if(utc<1577836800UL||offset< -840||offset>840)continue;
  
    s_.utc=utc;
  s_.utcOffset=offset;
  s_.timeAt=now;
  s_.timeValid=true;
  
   }else if(p.data[0]==3 && p.size==4){
    if(p.data[2]>3||p.data[3]>2)continue;
  
    s_.playback=p.data[2];
  s_.mediaSession=p.data[3];
  s_.lastMediaAt=now;
  
    if(s_.mediaSession!=1)s_.title[0]=0;
  
   }else if(p.data[0]==2 && p.size>=4){
    uint8_t seq=p.data[1],index=p.data[2],count=p.data[3];
  size_t length=p.size-4;
  
    if(count<1||count>8||index>=count||(index+1<count&&length!=16))continue;
  
    if(index==0){titleSequence_=seq;
  titleNext_=0;
  titleCount_=count;
  titleBytes_=0;
  }
    if(seq!=titleSequence_||index!=titleNext_||count!=titleCount_||titleBytes_+length>128)continue;
  
    for(size_t i=0;i<length;i++)titleAssembly_[titleBytes_++]=p.data[4+i]>=32&&p.data[4+i]<=126?char(p.data[4+i]):'?';
  
    titleNext_++;
  
    if(titleNext_==titleCount_){
     titleAssembly_[titleBytes_]=0;
  
     if(strcmp(s_.title,titleAssembly_)!=0){strcpy(s_.title,titleAssembly_);
  s_.titleAt=now;
  }
     titleCount_=0;
  
    }
   }else continue;
  
   s_.pcSeen=true;
  s_.lastPcAt=now;
  
  }
 }
 void linkTick(){
  bool linked=keyboard_.isConnected();
  if(linked==s_.connected)return;
  
  s_.connected=linked;
  s_.pcSeen=false;
  s_.estimate=-1;
  s_.title[0]=0;
  s_.mediaSession=0;
  s_.playback=0;
  volumeQueue_=0;
  
  titleCount_=0;
  titleNext_=0;
  
  if(!linked){portENTER_CRITICAL(&rxMux);
  rxTail=rxHead;
  portEXIT_CRITICAL(&rxMux);
  }
  if(!linked){if(s_.advertising)BLEDevice::startAdvertising();
  else BLEDevice::stopAdvertising();
  }
  if(s_.screen!=Screen::Boot && s_.screen!=Screen::Shutdown)note(linked?"PC CONNECTED":"PC DISCONNECTED");
  
 }
 void readEncoder(){
  portENTER_CRITICAL(&encoderMux);
  int32_t edges=encoderEdges;
  encoderEdges=0;
  portEXIT_CRITICAL(&encoderMux);
  
  if(threshold_!=s_.options.edges){threshold_=s_.options.edges;
  remainder_=0;
  }
  remainder_+=edges;
  int turns=remainder_/threshold_;
  remainder_-=turns*threshold_;
  if(!turns||blocked())return;
  
  waiting_=false;
  if(wakeInput())return;
  touch();
  if(s_.options.reverse)turns=-turns;
  
  if(s_.screen==Screen::Media&&s_.volumeEdit){volumeQueue_=constrain(volumeQueue_+turns*s_.options.step,-30,30);
  return;
  }
  if(s_.screen==Screen::Edit){int lo=s_.editField==EditField::Step?1:0;
  
   int hi=s_.editField==EditField::Step?5:s_.editField==EditField::Timeout?3:s_.editField==EditField::Estimate?100:1;
  
   s_.editValue=constrain(s_.editValue+turns,lo,hi);
  return;
  }
  int* selected=nullptr;
  int n=0;
  
  switch(s_.screen){
   case Screen::Dashboard:selected=&s_.dashboard;
  n=4;
  break;
  case Screen::Home:selected=&s_.home;
  n=4;
  break;
  
   case Screen::Apps:selected=&s_.app;
  n=6;
  break;
  case Screen::Media:selected=&s_.media;
  n=5;
  break;
  
   case Screen::Actions:selected=&s_.action;
  n=6;
  break;
  case Screen::Settings:selected=&s_.setting;
  n=5;
  break;
  
   case Screen::Connection:selected=&s_.connection;
  n=3;
  break;
  case Screen::DisplayPage:selected=&s_.display;
  n=3;
  break;
  
   case Screen::Controls:selected=&s_.control;
  n=5;
  break;
  case Screen::Confirm:selected=&s_.choice;
  n=2;
  break;
  default:break;
  
  }if(selected)*selected=((*selected+turns)%n+n)%n;
  
 }
 void readButton(){
  uint32_t now=millis();
  bool v=digitalRead(DeckConfig::BUTTON);
  if(v!=raw_){raw_=v;
  changed_=now;
  }
  if(ignoreRelease_){if(raw_==HIGH&&now-changed_>=25){ignoreRelease_=false;
  stable_=HIGH;
  held_=false;
  }return;
  }
  if(now-changed_>=25 && stable_!=raw_){
   stable_=raw_;
  
   if(stable_==LOW){pressed_=now;
  held_=false;
  if(s_.blank&&!blocked()){wakeInput();
  ignoreRelease_=true;
  waiting_=false;
  return;
  }}
   else if(!held_&&!blocked()){
    touch();
  if(!s_.options.backDouble){activate();
  }
    else if(waiting_&&now-clickAt_<=DeckConfig::DOUBLE_MS){waiting_=false;
  back();
  }
    else {waiting_=true;
  clickAt_=now;
  }
   }
  }
  if(stable_==LOW&&!held_&&now-pressed_>=DeckConfig::HOLD_MS){held_=true;
  waiting_=false;
  if(!blocked()&&!s_.options.backDouble){touch();
  back();
  }}
  if(waiting_&&raw_==HIGH&&stable_==HIGH&&now-clickAt_>DeckConfig::DOUBLE_MS){waiting_=false;
  if(!blocked())activate();
  }
 }
 void back(){
  waiting_=false;
  volumeQueue_=0;
  
  if(s_.screen==Screen::Media&&s_.volumeEdit){s_.volumeEdit=false;
  return;
  }
  if(s_.screen==Screen::Edit){s_.screen=editReturn_;
  return;
  }if(s_.screen==Screen::Confirm){s_.screen=confirmReturn_;
  return;
  }
  if(s_.screen==Screen::Connection||s_.screen==Screen::DisplayPage||s_.screen==Screen::Controls||s_.screen==Screen::About)s_.screen=Screen::Settings;
  
  else if(s_.screen==Screen::Home)s_.screen=Screen::Dashboard;
  
  else if(s_.screen!=Screen::Dashboard)s_.screen=Screen::Home;
  
  s_.volumeEdit=false;
  s_.feedback=nullptr;
  
 }
 void activate(){
  switch(s_.screen){
   case Screen::Dashboard:
    if(s_.dashboard==3)s_.screen=Screen::Home;
  
    else if(ready()){if(s_.dashboard==0)keyboard_.write(KEY_MEDIA_PREVIOUS_TRACK);
  if(s_.dashboard==1)keyboard_.write(KEY_MEDIA_PLAY_PAUSE);
  if(s_.dashboard==2)keyboard_.write(KEY_MEDIA_NEXT_TRACK);
  note("COMMAND SENT");
  }
  break;
  
   case Screen::Home:{const Screen pages[]={Screen::Apps,Screen::Media,Screen::Settings,Screen::Actions};
  s_.screen=pages[s_.home];
  break;
  }
   case Screen::Apps:launch();
  break;
  case Screen::Actions:action();
  break;
  
   case Screen::Media:
    if(s_.media==0){s_.volumeEdit=!s_.volumeEdit;
  volumeQueue_=0;
  }else if(ready()){
     if(s_.media==1)keyboard_.write(KEY_MEDIA_PREVIOUS_TRACK);
  if(s_.media==2)keyboard_.write(KEY_MEDIA_PLAY_PAUSE);
  
     if(s_.media==3)keyboard_.write(KEY_MEDIA_NEXT_TRACK);
  if(s_.media==4)keyboard_.write(KEY_MEDIA_MUTE);
  note("COMMAND SENT");
  }
  break;
  
   case Screen::Settings:{const Screen pages[]={Screen::Connection,Screen::DisplayPage,Screen::Controls,Screen::About};
  if(s_.setting<4)s_.screen=pages[s_.setting];
  else ask(Confirmation::Power);
  break;
  }
   case Screen::Connection:
    if(s_.connection==0)note(s_.connected?"CONNECTED":"NOT CONNECTED");
  
    else if(s_.connection==1){if(s_.connected)note("DISCONNECT PC FIRST");
  else{s_.advertising=!s_.advertising;
  if(s_.advertising)BLEDevice::startAdvertising();
  else BLEDevice::stopAdvertising();
  }}
    else ask(Confirmation::Forget);
  
    break;
  
   case Screen::DisplayPage:
    if(s_.display==0){s_.options.invert=!s_.options.invert;
  note(save()?"DISPLAY SAVED":"SAVE FAILED");
  }
    if(s_.display==1)edit(EditField::Timeout,s_.options.timeout);
  if(s_.display==2)edit(EditField::Estimate,s_.estimate<0?50:s_.estimate);
  break;
  
   case Screen::Controls:
    if(s_.control==0)edit(EditField::Step,s_.options.step);
  if(s_.control==1)edit(EditField::Reverse,s_.options.reverse);
  
    if(s_.control==2)edit(EditField::Edges,s_.options.edges==4);
  if(s_.control==3)edit(EditField::Sound,s_.options.sound);
  
    if(s_.control==4)edit(EditField::BackGesture,s_.options.backDouble?0:1);
  break;
  
   case Screen::Edit:commit();
  break;
  
   case Screen::Confirm:
    if(s_.choice==0)s_.screen=confirmReturn_;
  
    else if(s_.confirmation==Confirmation::Power){s_.screen=Screen::Shutdown;
  s_.animationStart=millis();
  s_.feedback=nullptr;
  volumeQueue_=0;
  s_.volumeEdit=false;
  }
    else clearBonds();
  
    break;
  
   case Screen::About:s_.screen=Screen::Settings;
  break;
  default:break;
  
  }
 }
 void chord(uint8_t modifier,uint8_t key,uint8_t extra=0){keyboard_.press(modifier);
  if(extra)keyboard_.press(extra);
  keyboard_.press(key);
  keyboard_.releaseAll();
  }
 void launch(){
  if(!ready())return;
  if(s_.app==3)chord(KEY_LEFT_GUI,'e');
  else{
   const uint8_t keys[]={KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F1};
  keyboard_.press(KEY_LEFT_CTRL);
  keyboard_.press(KEY_LEFT_ALT);
  keyboard_.press(KEY_LEFT_SHIFT);
  keyboard_.press(keys[s_.app]);
  keyboard_.releaseAll();
  }note("SHORTCUT SENT");
  
 }
 void action(){if(!ready())return;
  switch(s_.action){case 0:chord(KEY_LEFT_GUI,'s',KEY_LEFT_SHIFT);
  break;
  case 1:keyboard_.write(KEY_MEDIA_MUTE);
  break;
  case 2:chord(KEY_LEFT_GUI,'l');
  break;
  case 3:chord(KEY_LEFT_GUI,'s');
  break;
  case 4:chord(KEY_LEFT_GUI,'d');
  break;
  case 5:chord(KEY_LEFT_GUI,KEY_TAB);
  break;
  }note("COMMAND SENT");
  }
 void volumeTick(uint32_t now){if(!volumeQueue_||now-lastVolume_<45)return;
  lastVolume_=now;
  if(!s_.connected){volumeQueue_=0;
  note("NOT CONNECTED");
  return;
  }bool up=volumeQueue_>0;
  keyboard_.write(up?KEY_MEDIA_VOLUME_UP:KEY_MEDIA_VOLUME_DOWN);
  volumeQueue_+=up?-1:1;
  if(s_.estimate>=0)s_.estimate=constrain(s_.estimate+(up?2:-2),0,100);
  note(up?"VOLUME + SENT":"VOLUME - SENT");
  }
 void edit(EditField field,int value){editReturn_=s_.screen;
  s_.editField=field;
  s_.editValue=value;
  s_.screen=Screen::Edit;
  }
 void commit(){
  if(s_.editField==EditField::Estimate){s_.estimate=s_.editValue;
  s_.screen=editReturn_;
  note("ESTIMATE SET; NOT LIVE");
  return;
  }
  switch(s_.editField){case EditField::Step:s_.options.step=s_.editValue;
  break;
  case EditField::Reverse:s_.options.reverse=s_.editValue;
  break;
  case EditField::Edges:s_.options.edges=s_.editValue?4:2;
  break;
  case EditField::Sound:s_.options.sound=s_.editValue;
  break;
  case EditField::Timeout:s_.options.timeout=s_.editValue;
  break;
  case EditField::BackGesture:s_.options.backDouble=s_.editValue==0;
  break;
  default:break;
  }
  waiting_=false;
  s_.screen=editReturn_;
  note(save()?"SETTING SAVED":"SAVE FAILED");
  
 }
 bool save(){if(!storage_)return false;
  bool ok=true;
  ok=(prefs_.putInt("step",s_.options.step)>0)&&ok;
  ok=(prefs_.putInt("detents",s_.options.edges)>0)&&ok;
  ok=(prefs_.putInt("timeout",s_.options.timeout)>0)&&ok;
  ok=(prefs_.putBool("reverse",s_.options.reverse)>0)&&ok;
  ok=(prefs_.putBool("sound",s_.options.sound)>0)&&ok;
  ok=(prefs_.putBool("invert",s_.options.invert)>0)&&ok;
  ok=(prefs_.putBool("backdbl",s_.options.backDouble)>0)&&ok;
  return ok;
  }
 void ask(Confirmation c){confirmReturn_=s_.screen;
  s_.confirmation=c;
  s_.choice=0;
  s_.screen=Screen::Confirm;
  }
 void clearBonds(){s_.screen=Screen::Connection;
  if(s_.connected){note("DISCONNECT PC FIRST");
  return;
  }int count=esp_ble_get_bond_device_num();
  bool ok=true;
  if(count>0){esp_ble_bond_dev_t* list=(esp_ble_bond_dev_t*)calloc(count,sizeof(esp_ble_bond_dev_t));
  if(!list){note("OUT OF MEMORY");
  return;
  }if(esp_ble_get_bond_device_list(&count,list)!=ESP_OK)ok=false;
  else for(int i=0;i<count;i++)if(esp_ble_remove_bond_device(list[i].bd_addr)!=ESP_OK)ok=false;
  free(list);
  }if(!ok){note("PAIRING CLEAR FAILED");
  return;
  }clearing_=true;
  clearAt_=millis();
  note("CLEARING PAIRING...");
  }
 void bondTick(uint32_t now){if(!clearing_)return;
  if(esp_ble_get_bond_device_num()==0){clearing_=false;
  restartScheduled_=true;
  restartAt_=now;
  note("CLEARED; RESTARTING");
  }else if(now-clearAt_>4000){clearing_=false;
  note("PAIRING CLEAR TIMEOUT");
  }}
 void soundTick(){if(soundActive_&&millis()-soundAt_<25){if(micros()-soundToggle_>=250){soundToggle_=micros();
  soundLevel_=!soundLevel_;
  digitalWrite(DeckConfig::BEEPER,soundLevel_);
  }}else{soundActive_=false;
  digitalWrite(DeckConfig::BEEPER,LOW);
  }}
};
  
