#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "AudioTools/AudioLibs/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "BfButton.h"

//#define LED_BUILTIN 2
#define bit_clk_pin 27
#define word_select_pin 14
#define data_pin 26

//This definition will control the encoder module
int btnPin=32; 
int DT_pin=33; 
int CLK=25; 
BfButton btn(BfButton::STANDALONE_DIGITAL, btnPin, true, LOW);
int counter = 0;
int aState;
int aLastState;  

// This definitions are for the audio processing
AudioInfo info(44100, 2, 32); //Convert 44.1khz to 32 bits resolution
I2SStream out;
BluetoothA2DPSink a2dp_sink(out);

//SPIFFS stored music
const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSPIFFS source(startFilePath, ext);
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


//A fucntion to detect the push button in an rotatory encoder
void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  switch (pattern) {
    case BfButton::SINGLE_PRESS:
      Serial.println("Single push");
      break;
      
    case BfButton::DOUBLE_PRESS:
      Serial.println("Double push");
      break;
      
    case BfButton::LONG_PRESS:
      Serial.println("Long push");
      break;
  }
}
//A function to control the in-built LED
//void blinkLED(int onTime, int offTime) {
//  digitalWrite(LED_BUILTIN, HIGH);
//  delay(onTime);
//  digitalWrite(LED_BUILTIN, LOW);
//  delay(offTime);
//}

//Playback Status CallBack
void avrc_rn_playstatus_callback(esp_avrc_playback_stat_t playback) {
  switch (playback) {
    case esp_avrc_playback_stat_t::ESP_AVRC_PLAYBACK_STOPPED:
      Serial.println("Stopped.");
      break;
    case esp_avrc_playback_stat_t::ESP_AVRC_PLAYBACK_PLAYING:
      Serial.println("Playing.");
      break;
     case esp_avrc_playback_stat_t::ESP_AVRC_PLAYBACK_PAUSED:
      Serial.println("Paused.");
      break;
    case esp_avrc_playback_stat_t::ESP_AVRC_PLAYBACK_ERROR:
      Serial.println("Error.");
      break;
    default:
      Serial.printf("Got unknown playback status %d\n", playback);
  }
}

//Function to handle the title of the song
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  //Serial.printf("==> AVRC metadata rsp: attribute id 0x%x, %s\n", id, text);
  if (id == ESP_AVRC_MD_ATTR_TITLE){
  Serial.println("Title:");
  Serial.println((const char*)text);
  
  }
}

//Handle flash playback
void spiff_data_music(String pattern_str){
source.setFileFilter(pattern_str.c_str()); //Start a playback music 
  player.stop(); 
  player.begin();
  while (true){
    int out_val = player.copy();
    if (out_val == 0){
      player.stop();
      break;
    }
  }
}

void handle_volume_control(){
  int get_device_volume = a2dp_sink.get_volume();
  btn.read();
  
  aState = digitalRead(CLK);
  if (aState != aLastState){   
     counter = get_device_volume;
     if (digitalRead(DT_pin) != aState) { 
       counter = counter+10;
     }
      else {
       counter = counter-10;
     }
     if (counter >=128 ) {
       counter =127;
     }
     if (counter <=0 ) {
       counter = 0;
     }
     a2dp_sink.set_volume(counter);
     Serial.println(counter); 
  }   
  aLastState = aState;
}

int connected_sound_indicator = 0;
void handleConnectionStatus(){
    if (!a2dp_sink.is_connected()) {
    a2dp_sink.clean_last_connection();
    // Blink fast (100ms on, 100ms off) when not connected
    //blinkLED(100, 100); //Disabled due to conflict arises on volume control

  if (connected_sound_indicator == 0){  
  spiff_data_music("*dis*");
  connected_sound_indicator = 1;
  }
  }
     else if(a2dp_sink.is_connected()) 
  {  
    if (connected_sound_indicator == 1){
    spiff_data_music("*conn*");
  }
  connected_sound_indicator = 0;
}
}


void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Configure I2S audio stream
  auto cfg = out.defaultConfig();
  cfg.pin_bck = bit_clk_pin;  // Bit Clock Pin
  cfg.pin_ws = word_select_pin;   // Word Select Pin
  cfg.pin_data = data_pin; // Data Pin
  cfg.copyFrom(info); //Conversion starts
  
  out.begin(cfg); //I2S Begins

  spiff_data_music("*start*"); //Regex Pattern(start.mp3)

  // start a2dp
  a2dp_sink.set_avrc_rn_playstatus_callback(avrc_rn_playstatus_callback);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.start("Manjunathan"); //Bluetooth name
  
  // Set LED pin as output
  //pinMode(LED_BUILTIN, OUTPUT);

  pinMode(CLK,INPUT_PULLUP);
  pinMode(DT_pin,INPUT_PULLUP);
  aLastState = digitalRead(CLK);

  //Button settings
  btn.onPress(pressHandler)
  .onDoublePress(pressHandler) // default timeout
  .onPressFor(pressHandler, 1000); // custom timeout for 1 second
}


void loop() {
  handle_volume_control();
  handleConnectionStatus();
}