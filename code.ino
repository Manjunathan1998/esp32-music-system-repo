#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "BfButton.h"

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

I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);


//A funtion to detect the push button in an rotatory encoder
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

void handle_volume_control(){
  //get device volume
  int get_device_volume = a2dp_sink.get_volume();
  btn.read();

  //Calibrate
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
     //Set Volume to remote device
     a2dp_sink.set_volume(counter);
     Serial.println(counter); 
  }   
  aLastState = aState;
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Configure I2S audio stream
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = bit_clk_pin;  // Bit Clock Pin
  cfg.pin_ws = word_select_pin;   // Word Select Pin
  cfg.pin_data = data_pin; // Data Pin
  
  i2s.begin(cfg); //I2S Begins

  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.start("ESP"); //Bluetooth Device name
  
  //Setup the Rotary Encoder
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
}