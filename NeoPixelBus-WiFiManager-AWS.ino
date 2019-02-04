// NeoPixelFunFadeInOut
// This example will randomly pick a color and fade all pixels to that color, then
// it will fade them to black and restart over
// 
// This example demonstrates the use of a single animation channel to animate all
// the pixels at once.
//
//#include <NeoPixelBus.h>
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include "WiFiManager.h" 
#include <WiFiClientSecure.h>
#include <MQTTClient.h>   //you need to install this library: https://github.com/256dpi/arduino-mqtt
#include "Config.h"

const uint16_t PixelCount = 12; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 12;  // make sure to set this to the correct pin, ignored for Esp8266
const uint8_t AnimationChannels = 1; // we only need one as all the pixels are animated at once

//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// For Esp8266, the Pin is omitted and it uses GPIO3 due to DMA hardware use.  
// There are other Esp8266 alternative methods that provide more pin options, but also have
// other side effects.
// for details see wiki linked here https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods 

NeoPixelAnimator animations(AnimationChannels); // NeoPixel animation management object

boolean fadeToColor = true;  // general purpose variable used to store effect state

// what is stored for state is specific to the need, in this case, the colors.
// basically what ever you need inside the animation update function
struct MyAnimationState
{
    RgbColor StartingColor;\
    RgbColor EndingColor;
};

/* Animations ----*/
unsigned long patternInterval = 20 ; // time between steps in the pattern
unsigned long lastUpdate = 0;
// one entry per pixel to match the animation timing manager
MyAnimationState animationState[AnimationChannels];
unsigned long animationSpeed [] = { 50 } ; // speed for each animation (order counts!)
#define ANIMATIONS sizeof(animationSpeed) / sizeof(animationSpeed[0])
// Colors for sparkle
uint8_t myFavoriteColors[][3] = {{200,   0, 200},   // purple
                                 {200,   0,   0},   // red 
                                 {200, 200, 200},   // white
                               };
#define FAVCOLORS sizeof(myFavoriteColors) / 3

/* WiFi -----*/
WiFiManager wm; // global wm instance
bool res;       // Boolean letting us know if we can connect to saved WiFi

/* AWS -----*/
WiFiClientSecure net;
MQTTClient client;

void SetRandomSeed()
{
    uint32_t seed;

    // random works best with a seed that can use 31 bits
    // analogRead on a unconnected pin tends toward less than four bits
    seed = analogRead(0);
    delay(1);

    for (int shifts = 3; shifts < 31; shifts += 3)
    {
        seed ^= analogRead(0) << shifts;
        delay(1);
    }

    randomSeed(seed);
}

// simple blend function
void BlendAnimUpdate(const AnimationParam& param)
{
    // this gets called for each animation on every time step
    // progress will start at 0.0 and end at 1.0
    // we use the blend function on the RgbColor to mix
    // color based on the progress given to us in the animation
    RgbColor updatedColor = RgbColor::LinearBlend(
        animationState[param.index].StartingColor,
        animationState[param.index].EndingColor,
        param.progress);

    // apply the color to the strip
    for (uint16_t pixel = 0; pixel < PixelCount; pixel++)
    {
        strip.SetPixelColor(pixel, updatedColor);
    }
}

void FadeInFadeOutRinseRepeat(float luminance)
{
    if (fadeToColor)
    {
        // Fade upto a random color
        // we use HslColor object as it allows us to easily pick a hue
        // with the same saturation and luminance so the colors picked
        // will have similiar overall brightness
        RgbColor target = HslColor(random(360) / 360.0f, 1.0f, luminance);
        uint16_t time = random(800, 2000);

        animationState[0].StartingColor = strip.GetPixelColor(0);
        animationState[0].EndingColor = target;

        animations.StartAnimation(0, time, BlendAnimUpdate);
    }
    else 
    {
        // fade to black
        uint16_t time = random(600, 700);

        animationState[0].StartingColor = strip.GetPixelColor(0);
        animationState[0].EndingColor = RgbColor(0);

        animations.StartAnimation(0, time, BlendAnimUpdate);
    }

    // toggle to the next effect state
    fadeToColor = !fadeToColor;
}


void connect(){
    awsConnect();
    mqttTopicSubscribe();
}

bool awsConnect(){
  static int awsConnectTimer = 0;
  int awsConnectTimout = 10000;
  net.setCACert(rootCABuff);
  net.setCertificate(certificateBuff);
  net.setPrivateKey(privateKeyBuff);
  client.begin(awsEndPoint, 8883, net);

  Serial.print("\nConnecting to AWS");
  while (!client.connect(CLIENT_ID)) {
    if(awsConnectTimer == 0) awsConnectTimer = millis();
    if (millis() - awsConnectTimer > awsConnectTimout) {
      Serial.println("CONNECTION FAILURE: Could not connect to aws");
      return false;
    }
    Serial.print(".");
    delay(100);
  }

  Serial.println("Connected to AWS"); 
  awsConnectTimer = 0;

  return true;

}

bool mqttTopicSubscribe(){
  if(client.subscribe(subscribeTopic)) Serial.print("Subscribed to topic: "); Serial.println(subscribeTopic);
  client.onMessage(messageReceived);
}

void messageReceived(String &topic, String &payload) {
  Serial.println("Message received!");
  Serial.println(payload);
}


// LED sparkling. 
void sparkle(uint8_t howmany) {
  static int x = 0;
  static bool goingUp = true;

  for(uint16_t i=0; i<howmany; i++) {
    // pick a random favorite color!
    int c = random(FAVCOLORS);
    int red = myFavoriteColors[c][0];
    int green = myFavoriteColors[c][1];
    int blue = myFavoriteColors[c][2];

    // get a random pixel from the list
    //int j = random(strip.numPixels());
    int j = random(PixelCount);

    // now we will 'fade' it in 5 steps
    if(goingUp){
      if(x < 5) {
        x++;
      } else {
        goingUp = false;
      }
    } else {
      if(x>0){
        x--;
      } else {
        goingUp = true;
      }
    }

    int r = red * (x+1); r /= 5;
    int g = green * (x+1); g /= 5;
    int b = blue * (x+1); b /= 5;
    strip.SetPixelColor(j, RgbColor(r,g,b));
    strip.Show();
  }
  lastUpdate = millis();
}


void setup()
{
    WiFi.disconnect(true);
    delay(5000);
    Serial.begin(115200);
    Serial.println("\n Starting up...");
  
    strip.Begin();
    strip.SetBrightness(30);
    strip.Show();

    SetRandomSeed();

        // Initiatize WiFiManager
    res = wm.autoConnect(CLIENT_ID,"password"); // password protected ap
    wm.setConfigPortalTimeout(30); // auto close configportal after n seconds

    // Seeing if we can re-connect to known WiFi
    if(!res) {
        Serial.println("Failed to connect or hit timeout");
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected to wifi :)");
    }

  connect();
}

void loop()
{
  static int animationTimer = 0;
  int nextAnimation = 5000; // Change animation every 5 seconds
  static bool ab = true;
  
  if(!client.connected()){
    connect();
  }
  client.loop();


  if(millis() - animationTimer > nextAnimation){
    ab = !ab;
    animationTimer = millis();
    client.publish(publishTopic, "Animation changed");
  }

  if(ab){
    if (animations.IsAnimating())
    {
        // the normal loop just needs these two to run the active animations
        animations.UpdateAnimations();
        strip.Show();
    }
    else
    {
        // no animation runnning, start some 
        //
        FadeInFadeOutRinseRepeat(0.2f); // 0.0 = black, 0.25 is normal, 0.5 is bright
    }    
  }else{
    // Update animation frame
    if(millis() - lastUpdate > patternInterval) { 
      sparkle(3);
    }
  }




}
