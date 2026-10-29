#pragma once
#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <stdlib.h>

// V1.2 target: XIAO ESP32-S3, Arduino-ESP32 2.0.17,
// T-vK ESP32 BLE Keyboard 0.3.0, Bluedroid (not USE_NIMBLE).
namespace DeckConfig {
constexpr int LCD_CLOCK=7;     // D8 -> EXP3 5
constexpr int LCD_DATA=9;      // D10 -> EXP3 3
constexpr int LCD_CS=44;       // D7 -> EXP3 4
constexpr int ENCODER_A=1;     // D0 -> EXP3 8
constexpr int ENCODER_B=4;     // D3 -> EXP3 6
constexpr int BUTTON=5;        // D4 -> EXP3 9
constexpr int BEEPER=6;        // D5 -> EXP3 10
constexpr unsigned long DEBOUNCE_MS=25, HOLD_MS=700;
constexpr unsigned long BOOT_MS=1400, SHUTDOWN_MS=1000;
constexpr unsigned long FRAME_MS=50, FEEDBACK_MS=1000;
constexpr unsigned long VOLUME_INTERVAL_MS=45;
constexpr int APP_COUNT=6, ACTION_COUNT=6;
}

enum class DeckScreen : uint8_t {
 Boot, Home, Apps, Media, Actions, Settings, Connection,
 DisplayOptions, Controls, About, Edit, Confirm, Shutdown
};
enum class DeckEdit : uint8_t { VolumeStep, Direction, Detents, Sound, Timeout };
enum class DeckConfirm : uint8_t { PowerOff, ForgetPairing };

struct DeckSettings {
 int volumeStep=1, transitions=4, timeoutIndex=0;
 bool reversed=false, sound=false;
};
struct DeckState {
 DeckScreen screen=DeckScreen::Boot;
 DeckSettings settings;
 int home=0, app=0, media=0, action=0, setting=0, connection=0, control=0;
 bool volumeEditing=false, connected=false, advertising=true, displayBlank=false;
 DeckEdit editField=DeckEdit::VolumeStep;
 int editValue=0;
 DeckConfirm confirmation=DeckConfirm::PowerOff;
 int confirmationChoice=0;
 unsigned long animationStart=0, feedbackStart=0;
 const char* feedback=nullptr;
};

// Defined once: include this header through the main sketch, not separate .cpp units.
static portMUX_TYPE deckEncoderMux=portMUX_INITIALIZER_UNLOCKED;
static volatile int32_t deckEncoderEdges=0;
static volatile uint8_t deckPreviousAB=0;
static DRAM_ATTR int8_t deckTransitionTable[16]={0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
static void IRAM_ATTR deckEncoderISR(){
 const uint8_t ab=(digitalRead(DeckConfig::ENCODER_A)<<1)|digitalRead(DeckConfig::ENCODER_B);
 portENTER_CRITICAL_ISR(&deckEncoderMux);
 deckEncoderEdges+=deckTransitionTable[(deckPreviousAB<<2)|ab];
 deckPreviousAB=ab;
 portEXIT_CRITICAL_ISR(&deckEncoderMux);
}

class DeckCore {
public:
 DeckCore():keyboard_("Rotary Deck","DIY",100){}
 void begin(){
  rtc_gpio_deinit(GPIO_NUM_5);
  pinMode(DeckConfig::ENCODER_A,INPUT_PULLUP);
  pinMode(DeckConfig::ENCODER_B,INPUT_PULLUP);
  pinMode(DeckConfig::BUTTON,INPUT_PULLUP);
  pinMode(DeckConfig::BEEPER,OUTPUT);digitalWrite(DeckConfig::BEEPER,LOW);
  storageReady_=prefs_.begin("rotarydeck",false);
  if(storageReady_){
   state_.settings.volumeStep=constrain(prefs_.getInt("step",1),1,5);
   state_.settings.transitions=prefs_.getInt("detents",4)==2?2:4;
   state_.settings.timeoutIndex=constrain(prefs_.getInt("timeout",0),0,3);
   state_.settings.reversed=prefs_.getBool("reverse",false);
   state_.settings.sound=prefs_.getBool("sound",false);
  }
  lastThreshold_=state_.settings.transitions;
  rawButton_=stableButton_=digitalRead(DeckConfig::BUTTON);
  ignoreUntilReleased_=rawButton_==LOW;
  buttonChanged_=millis();
  deckPreviousAB=(digitalRead(DeckConfig::ENCODER_A)<<1)|digitalRead(DeckConfig::ENCODER_B);
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENCODER_A),deckEncoderISR,CHANGE);
  attachInterrupt(digitalPinToInterrupt(DeckConfig::ENCODER_B),deckEncoderISR,CHANGE);
  keyboard_.begin();keyboard_.setDelay(8);
  state_.advertising=true;
  state_.animationStart=lastActivity_=millis();
 }
 const DeckState& state()const{return state_;}
 bool sleepRequested()const{return sleepRequested_;}
 bool restartRequested()const{return restartRequested_;}
 void update(){
  const unsigned long now=millis();
  updateBeeper();updateConnection();updatePairingClear(now);
  if(state_.screen==DeckScreen::Boot && now-state_.animationStart>=DeckConfig::BOOT_MS)
   state_.screen=DeckScreen::Home;
  if(state_.screen==DeckScreen::Shutdown && now-state_.animationStart>=DeckConfig::SHUTDOWN_MS)
   sleepRequested_=true;
  readEncoder();readButton();processVolume(now);
  if(restartScheduled_ && now-restartStarted_>=800)restartRequested_=true;
  static const unsigned long timeouts[]={0,30000,60000,120000};
  if(state_.screen!=DeckScreen::Boot && state_.screen!=DeckScreen::Shutdown &&
     !clearPending_ && !restartScheduled_ && state_.settings.timeoutIndex &&
     now-lastActivity_>=timeouts[state_.settings.timeoutIndex])state_.displayBlank=true;
 }
 void enterSleep(){
  // Main must blank the LCD before calling this. No backlight power switch exists.
  saveSettings();pendingVolume_=0;
  keyboard_.releaseAll();BLEDevice::stopAdvertising();
  digitalWrite(DeckConfig::BEEPER,LOW);
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENCODER_A));
  detachInterrupt(digitalPinToInterrupt(DeckConfig::ENCODER_B));
  while(digitalRead(DeckConfig::BUTTON)==LOW)delay(10);
  delay(40);
  rtc_gpio_init(GPIO_NUM_5);
  rtc_gpio_set_direction(GPIO_NUM_5,RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_5);rtc_gpio_pulldown_dis(GPIO_NUM_5);
  if(esp_sleep_enable_ext0_wakeup(GPIO_NUM_5,0)!=ESP_OK){ESP.restart();return;}
  Serial.flush();esp_deep_sleep_start();
 }
private:
 BleKeyboard keyboard_;Preferences prefs_;DeckState state_;
 DeckScreen editReturn_=DeckScreen::Controls, confirmReturn_=DeckScreen::Settings;
 bool storageReady_=false,sleepRequested_=false,restartRequested_=false;
 bool rawButton_=HIGH,stableButton_=HIGH,holdSent_=false,ignoreUntilReleased_=false;
 bool clearPending_=false,restartScheduled_=false,buzzerLevel_=false;
 int edgeRemainder_=0,lastThreshold_=4,pendingVolume_=0;
 unsigned long buttonChanged_=0,buttonPressed_=0,lastActivity_=0,lastVolume_=0;
 unsigned long clearStarted_=0,restartStarted_=0,buzzerStarted_=0,buzzerToggle_=0;
 bool buzzerActive_=false;
 void notify(const char* message){state_.feedback=message;state_.feedbackStart=millis();}
 bool inputAllowed(){
  if(state_.screen==DeckScreen::Boot || state_.screen==DeckScreen::Shutdown ||
     clearPending_ || restartScheduled_)return false;
  lastActivity_=millis();
  if(state_.displayBlank){state_.displayBlank=false;return false;}
  beep();return true;
 }
 bool connected(){if(!state_.connected){notify("Not connected");return false;}return true;}
 void updateConnection(){
  const bool value=keyboard_.isConnected();
  if(value==state_.connected)return;
  state_.connected=value;pendingVolume_=0;
  if(!value){
   if(state_.advertising)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();
  }
  if(state_.screen!=DeckScreen::Boot && state_.screen!=DeckScreen::Shutdown)
   notify(value?"PC connected":"PC disconnected");
 }
 void readEncoder(){
  portENTER_CRITICAL(&deckEncoderMux);
  int32_t edges=deckEncoderEdges;deckEncoderEdges=0;
  portEXIT_CRITICAL(&deckEncoderMux);
  if(lastThreshold_!=state_.settings.transitions){edgeRemainder_=0;lastThreshold_=state_.settings.transitions;}
  edgeRemainder_+=edges;
  int detents=edgeRemainder_/lastThreshold_;edgeRemainder_-=detents*lastThreshold_;
  if(detents)rotate(detents);
 }
 void readButton(){
  const unsigned long now=millis();const bool value=digitalRead(DeckConfig::BUTTON);
  if(value!=rawButton_){rawButton_=value;buttonChanged_=now;}
  if(ignoreUntilReleased_){
   if(rawButton_==HIGH && now-buttonChanged_>=DeckConfig::DEBOUNCE_MS){
    ignoreUntilReleased_=false;stableButton_=HIGH;holdSent_=false;
   }return;
  }
  if(now-buttonChanged_>=DeckConfig::DEBOUNCE_MS && stableButton_!=rawButton_){
   stableButton_=rawButton_;
   if(stableButton_==LOW){buttonPressed_=now;holdSent_=false;}
   else if(!holdSent_)click();
  }
  if(stableButton_==LOW && !holdSent_ && now-buttonPressed_>=DeckConfig::HOLD_MS){holdSent_=true;hold();}
 }
 int* selection(){
  switch(state_.screen){
   case DeckScreen::Home:return &state_.home;case DeckScreen::Apps:return &state_.app;
   case DeckScreen::Media:return &state_.media;case DeckScreen::Actions:return &state_.action;
   case DeckScreen::Settings:return &state_.setting;case DeckScreen::Connection:return &state_.connection;
   case DeckScreen::Controls:return &state_.control;case DeckScreen::Confirm:return &state_.confirmationChoice;
   default:return nullptr;
  }
 }
 int selectionCount(){
  switch(state_.screen){
   case DeckScreen::Home:return 4;case DeckScreen::Apps:return DeckConfig::APP_COUNT;
   case DeckScreen::Actions:return DeckConfig::ACTION_COUNT;
   case DeckScreen::Media:case DeckScreen::Settings:return 5;
   case DeckScreen::Connection:return 3;case DeckScreen::Controls:return 4;
   case DeckScreen::Confirm:return 2;default:return 0;
  }
 }
 void rotate(int detents){
  if(!inputAllowed())return;
  if(state_.settings.reversed)detents=-detents;
  if(state_.screen==DeckScreen::Media && state_.volumeEditing){
   pendingVolume_=constrain(pendingVolume_+detents*state_.settings.volumeStep,-30,30);return;
  }
  if(state_.screen==DeckScreen::Edit){
   const int lo=state_.editField==DeckEdit::VolumeStep?1:0;
   const int hi=state_.editField==DeckEdit::VolumeStep?5:state_.editField==DeckEdit::Timeout?3:1;
   state_.editValue=constrain(state_.editValue+detents,lo,hi);return;
  }
  int* selected=selection();const int count=selectionCount();
  if(selected && count)*selected=((*selected+detents)%count+count)%count;
 }
 void click(){
  if(!inputAllowed())return;
  switch(state_.screen){
   case DeckScreen::Home:{
    const DeckScreen screens[]={DeckScreen::Apps,DeckScreen::Media,DeckScreen::Actions,DeckScreen::Settings};
    state_.screen=screens[state_.home];break;
   }
   case DeckScreen::Apps:launchApp(state_.app);break;
   case DeckScreen::Actions:executeAction(state_.action);break;
   case DeckScreen::Media:
    if(state_.media==0){state_.volumeEditing=!state_.volumeEditing;pendingVolume_=0;}
    else if(connected()){
     if(state_.media==1)keyboard_.write(KEY_MEDIA_PREVIOUS_TRACK);
     if(state_.media==2)keyboard_.write(KEY_MEDIA_PLAY_PAUSE);
     if(state_.media==3)keyboard_.write(KEY_MEDIA_NEXT_TRACK);
     if(state_.media==4)keyboard_.write(KEY_MEDIA_MUTE);
     notify("Command sent");
    }break;
   case DeckScreen::Settings:{
    const DeckScreen screens[]={DeckScreen::Connection,DeckScreen::DisplayOptions,DeckScreen::Controls,DeckScreen::About};
    if(state_.setting<4)state_.screen=screens[state_.setting];else ask(DeckConfirm::PowerOff);break;
   }
   case DeckScreen::Connection:
    if(state_.connection==0)notify(state_.connected?"Connected":"Not connected");
    else if(state_.connection==1){
     if(state_.connected)notify("Disconnect PC first");
     else {state_.advertising=!state_.advertising;
      if(state_.advertising)BLEDevice::startAdvertising();else BLEDevice::stopAdvertising();
      notify(state_.advertising?"Discoverable":"Advertising off");}
    }else ask(DeckConfirm::ForgetPairing);break;
   case DeckScreen::DisplayOptions:startEdit(DeckEdit::Timeout,state_.settings.timeoutIndex);break;
   case DeckScreen::Controls:
    if(state_.control==0)startEdit(DeckEdit::VolumeStep,state_.settings.volumeStep);
    if(state_.control==1)startEdit(DeckEdit::Direction,state_.settings.reversed);
    if(state_.control==2)startEdit(DeckEdit::Detents,state_.settings.transitions==4);
    if(state_.control==3)startEdit(DeckEdit::Sound,state_.settings.sound);
    break;
   case DeckScreen::Edit:saveEdit();break;
   case DeckScreen::Confirm:
    if(state_.confirmationChoice==0)state_.screen=confirmReturn_;
    else if(state_.confirmation==DeckConfirm::PowerOff){
     state_.volumeEditing=false;pendingVolume_=0;state_.feedback=nullptr;
     state_.screen=DeckScreen::Shutdown;state_.animationStart=millis();
    }else clearPairing();break;
   default:break;
  }
 }
 void hold(){
  if(!inputAllowed())return;
  if(state_.screen==DeckScreen::Media && state_.volumeEditing){state_.volumeEditing=false;pendingVolume_=0;return;}
  if(state_.screen==DeckScreen::Edit){state_.screen=editReturn_;return;}
  if(state_.screen==DeckScreen::Confirm){state_.screen=confirmReturn_;return;}
  if(state_.screen==DeckScreen::Connection || state_.screen==DeckScreen::DisplayOptions ||
     state_.screen==DeckScreen::Controls || state_.screen==DeckScreen::About)state_.screen=DeckScreen::Settings;
  else {state_.screen=DeckScreen::Home;pendingVolume_=0;}
 }
 void chord(uint8_t modifier,uint8_t key,uint8_t extra=0){
  keyboard_.press(modifier);if(extra)keyboard_.press(extra);keyboard_.press(key);keyboard_.releaseAll();
 }
 void launchApp(int index){
  if(!connected())return;
  if(index==4)chord(KEY_LEFT_GUI,'e');
  else {
   const uint8_t keys[]={KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6};
   keyboard_.press(KEY_LEFT_CTRL);keyboard_.press(KEY_LEFT_ALT);keyboard_.press(KEY_LEFT_SHIFT);
   keyboard_.press(keys[index]);keyboard_.releaseAll();
  }notify("Shortcut sent");
 }
 void executeAction(int index){
  if(!connected())return;
  switch(index){
   case 0:chord(KEY_LEFT_GUI,'s',KEY_LEFT_SHIFT);break;
   case 1:keyboard_.write(KEY_MEDIA_MUTE);break;
   case 2:chord(KEY_LEFT_GUI,'l');break;
   case 3:chord(KEY_LEFT_GUI,'s');break;
   case 4:chord(KEY_LEFT_GUI,'d');break;
   case 5:chord(KEY_LEFT_GUI,KEY_TAB);break;
  }notify("Command sent");
 }
 void processVolume(unsigned long now){
  if(!pendingVolume_ || now-lastVolume_<DeckConfig::VOLUME_INTERVAL_MS)return;
  lastVolume_=now;
  if(!state_.connected){pendingVolume_=0;notify("Not connected");return;}
  const bool up=pendingVolume_>0;
  keyboard_.write(up?KEY_MEDIA_VOLUME_UP:KEY_MEDIA_VOLUME_DOWN);
  pendingVolume_+=up?-1:1;notify(up?"Volume +":"Volume -");
 }
 void startEdit(DeckEdit field,int value){
  editReturn_=state_.screen;state_.editField=field;state_.editValue=value;state_.screen=DeckScreen::Edit;
 }
 void saveEdit(){
  switch(state_.editField){
   case DeckEdit::VolumeStep:state_.settings.volumeStep=state_.editValue;break;
   case DeckEdit::Direction:state_.settings.reversed=state_.editValue;break;
   case DeckEdit::Detents:state_.settings.transitions=state_.editValue?4:2;break;
   case DeckEdit::Sound:state_.settings.sound=state_.editValue;break;
   case DeckEdit::Timeout:state_.settings.timeoutIndex=state_.editValue;break;
  }
  const bool saved=saveSettings();state_.screen=editReturn_;notify(saved?"Setting saved":"Save failed");
 }
 bool saveSettings(){
  if(!storageReady_)return false;
  bool ok=true;
  ok=(prefs_.putInt("step",state_.settings.volumeStep)>0)&&ok;
  ok=(prefs_.putBool("reverse",state_.settings.reversed)>0)&&ok;
  ok=(prefs_.putInt("detents",state_.settings.transitions)>0)&&ok;
  ok=(prefs_.putInt("timeout",state_.settings.timeoutIndex)>0)&&ok;
  ok=(prefs_.putBool("sound",state_.settings.sound)>0)&&ok;return ok;
 }
 void ask(DeckConfirm action){
  confirmReturn_=state_.screen;state_.confirmation=action;
  state_.confirmationChoice=0;state_.screen=DeckScreen::Confirm;
 }
 void clearPairing(){
  state_.screen=DeckScreen::Connection;
  if(state_.connected){notify("Disconnect PC first");return;}
  int count=esp_ble_get_bond_device_num();bool ok=true;
  if(count>0){
   esp_ble_bond_dev_t* devices=(esp_ble_bond_dev_t*)calloc(count,sizeof(esp_ble_bond_dev_t));
   if(!devices){notify("Out of memory");return;}
   if(esp_ble_get_bond_device_list(&count,devices)!=ESP_OK)ok=false;
   else for(int i=0;i<count;i++)if(esp_ble_remove_bond_device(devices[i].bd_addr)!=ESP_OK)ok=false;
   free(devices);
  }
  if(!ok){notify("Bond clear failed");return;}
  clearPending_=true;clearStarted_=millis();notify("Clearing pairing...");
 }
 void updatePairingClear(unsigned long now){
  if(!clearPending_)return;
  if(esp_ble_get_bond_device_num()==0){
   clearPending_=false;restartScheduled_=true;restartStarted_=now;notify("Cleared; restarting");
  }else if(now-clearStarted_>4000){clearPending_=false;notify("Clear pairing timed out");}
 }
 void beep(){if(state_.settings.sound){buzzerActive_=true;buzzerStarted_=millis();buzzerToggle_=micros();}}
 void updateBeeper(){
  if(buzzerActive_ && millis()-buzzerStarted_<25){
   if(micros()-buzzerToggle_>=250){buzzerToggle_=micros();buzzerLevel_=!buzzerLevel_;digitalWrite(DeckConfig::BEEPER,buzzerLevel_);}
  }else {buzzerActive_=false;buzzerLevel_=false;digitalWrite(DeckConfig::BEEPER,LOW);}
 }
};
