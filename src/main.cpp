#include <Arduino.h>
#include <TinyGPS++.h>
#include <WS2812FX.h>
#include <EasyTransfer.h>

// Input switch setup
#define FNPIN        18  // aka Pin 18 D18
#define OKPIN        19  // aka Pin 19 D19
int inputInterval = 300; // Check 10x/sec.
unsigned int inputCheck = 0; // Next service timer.
bool btnFn = false;
bool btnOK = false;

// Light sensor setup
const int LIGHTPIN = A6; // aka Analog A6 Pin20
int lightInterval = 5000; // Check every 5 secs.
unsigned int lightCheck = 0; // Next service timer.
byte sensorLight = 0; //Stores current light Level percentage.
byte brightLevel = 0;

// Battery Voltage setup
const int BATTPIN = A0; // aka Analog A0 Pin14
int battInterval = 30000; // Check every 30 secs.
unsigned int battCheck = 0; // Next service timer.
const int battFactor = 777; // Conversion factor.
//float battPct = 0; //Stores current light Level percentage.

// Pixel processing setup
#define PIXELPIN     17 // aka Pin 17 D17 for Teensy Special led driver hw
#define NUMPIXELS    13
WS2812FX ws2812fx = WS2812FX(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);
unsigned int lightModeCheck = 0; // Next service timer.
int lightModeInterval = 50; // millis before servicing pixels

// GPS processing setup
#define GPSPWRPIN    13 // aka Pin 13 D13. High to power up GPS. Serial2 RX,TX D9, D10 Pin 9,Pin10
#define PMTK_SET_NMEA_OUTPUT_RMCONLY "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"
TinyGPSPlus gps;
int gpsInterval = 4000; // Check every 4 secs.
unsigned int gpsCheck = 0; // Next service timer.
//const double Dest_Lat = 13.3248881;
//const double Dest_Lon = -86.5733932;
double distance = 0;
double courseTo = 0;
double course = 0;
bool gpsEnabled = true;
//double myLat = 0;
//double myLon = 0;

// COM Serial Setup
// Esp serial comms on Serial3 - RX D7, TX D8
int comInterval = 10000; // Check every 10 secs.
unsigned int comCheck = 0; // Next service timer.
EasyTransfer ETIn, ETOut;
struct ESP_DATA{ //put your variable definitions here for the data you want to receive
  double destLon;
  double destLat;
};
struct TEENSY_DATA{ //put your variable definitions here for the data you want to receive
  float battPct;
  byte mode;
  double myLon;
  double myLat;
};
ESP_DATA espData;
TEENSY_DATA tnsyData;

//byte mode = 0; 
bool modeInitd = false;
bool rdyToSend = false; // Set to true if we have priority data to send.

void setup() {
  Serial.begin(19200); // Start USB monitor Serial.
  Serial.println("Starting TheTeensyStick.");

  pinMode(FNPIN,INPUT_PULLUP);
  pinMode(BATTPIN,INPUT);
  pinMode(OKPIN,INPUT_PULLUP);
  pinMode(LIGHTPIN,INPUT);
  inputCheck = millis() + inputInterval;
  battCheck = millis();
  ETIn.begin(details(espData),&Serial3);
  ETOut.begin(details(tnsyData),&Serial3);

  Serial.println("Init Pixels.");
  ws2812fx.init();
  ws2812fx.setBrightness(128);
  // parameters: index, start, stop, mode, color, speed, reverse
  ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, 0x000022, 1500, false); // segment 0 is led 0
  ws2812fx.setSegment(1, 1, 3, FX_MODE_SCAN, 0x222222, 1500, false); // segment 1 is leds 1-3
  ws2812fx.setSegment(2, 4, 6, FX_MODE_SCAN, 0x222222, 1500, false);  // segment 2 is leds 4-6
  ws2812fx.setSegment(3, 7, 9, FX_MODE_SCAN, 0x222222, 1500, false);  // segment 3 is leds 7-9
  ws2812fx.setSegment(4, 10, 12, FX_MODE_SCAN, 0x222222, 1500, false);  // segment 3 is leds 10-12
  ws2812fx.start();
  //pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  lightModeCheck = millis() + lightModeInterval;

  Serial.println("GPS ON.");
  pinMode(GPSPWRPIN,OUTPUT);
  digitalWrite(GPSPWRPIN,HIGH);
  gpsEnabled = true;
  delay(2000);
  Serial2.begin(9600); // Start GPS Serial 2
  Serial2.println(PMTK_SET_NMEA_OUTPUT_RMCONLY); // turn on only the second sentence (GPRMC)
  gpsCheck = millis() + gpsInterval;

  Serial.println("ESP Serial ON.");
  Serial3.begin(19200);
  Serial3.println("Teensy is UP.");
  comCheck = comInterval + millis();
  tnsyData.mode = 0;
}

void loop() {
  // First see if its time to process subs.
  if (millis() > lightCheck) {     // Time to check for light level.
    lightCheck = millis() + lightInterval;
    byte valueAnalog = 0;
    valueAnalog = int(analogRead(LIGHTPIN) * 100/1023);
    valueAnalog = valueAnalog > 100 ? 100 : valueAnalog;
    valueAnalog = valueAnalog < 1 ? 1 : valueAnalog;
    sensorLight = valueAnalog;
    brightLevel = sensorLight*255/100;
//    Serial.print("Brightness: ");
//    Serial.println(brightLevel);
  }
  if (millis() > battCheck) {     // Time to check for battery level.
    battCheck = millis() + battInterval;
    float valueAnalog = 0;
    valueAnalog = analogRead(BATTPIN);
    valueAnalog = valueAnalog > 1023 ? 1023 : valueAnalog;
    valueAnalog = valueAnalog < 1 ? 1 : valueAnalog;
    tnsyData.battPct = valueAnalog*5/battFactor;
//    Serial.print("Battery: ");
//    Serial.println(tnsyData.battPct);
  }
  if (millis() > inputCheck) {     // Time to check for input buttons.
    inputCheck = millis() + inputInterval;
    if ((digitalRead(FNPIN) == LOW) && !(btnFn)) {
      Serial.println("FN Pressed");
      btnFn = true;
      rdyToSend = true;
    }
    if ((digitalRead(FNPIN) == HIGH) && (btnFn)) {
      Serial.println("FN Released");
      btnFn = false;
      rdyToSend = true;
    }
    if ((digitalRead(OKPIN) == LOW) && !(btnOK)) {
      Serial.println("OK Pressed");
      btnOK = true;
      rdyToSend = true;
    }
    if ((digitalRead(OKPIN) == HIGH) && (btnOK)) {
      Serial.println("OK Released");
      btnOK = false;
      rdyToSend = true;
    }
  }
  if ((millis() > gpsCheck) && (gpsEnabled)) {     // Time to service GPS.
    gpsCheck = millis() + gpsInterval;
    Serial.println("");
    if (gps.location.isValid()) {
      double newLat = 0;
      double newLon = 0;
      newLat = gps.location.lat();
      newLon = gps.location.lng();
      if ((newLat != tnsyData.myLat) || (newLon != tnsyData.myLon)) {
        tnsyData.myLon = newLon;
        tnsyData.myLat = newLat;
        distance = gps.distanceBetween(tnsyData.myLat,tnsyData.myLon,espData.destLat,espData.destLon) / 1000.0;
        courseTo = gps.courseTo(tnsyData.myLat,tnsyData.myLon,espData.destLat,espData.destLon);
        course = gps.course.value();
        Serial.print(tnsyData.myLat, 6);
        Serial.print(F(","));
        Serial.println(tnsyData.myLon, 6);
        Serial.print("Distance: ");
        Serial.println(distance);
        Serial.print("Course: ");
        Serial.println(gps.cardinal(course));
        Serial.print("CourseTo: ");
        Serial.println(courseTo);
        Serial.print("CardinalTo: ");
        Serial.println(gps.cardinal(courseTo));
      } else {
        Serial.println("No GPS Change");
      }
    } else {
      Serial.println("no valid GPS Data.");
    }
  }
  if ((millis() > comCheck) || (rdyToSend)) {
    ETOut.sendData();
    comCheck = millis() + comInterval;
    rdyToSend = false;
  }
  // End of event servicing.
  if (btnFn) {
    tnsyData.mode ++;
    tnsyData.mode = tnsyData.mode > 4 ? 0 : tnsyData.mode;
    inputCheck = millis() + 1000;
    btnFn = false;
    modeInitd = false;
  }
  while (Serial2.available() > 0) { // GPS Data is ready.
    gps.encode(Serial2.read());
    yield();
  }
  while (Serial3.available() > 0) { // ESP Data is ready.
    if (ETIn.receiveData()) {
      Serial.println("New ESP Data Receieved.");
      Serial.print(espData.destLat, 6);
      Serial.print(F(","));
      Serial.println(espData.destLon, 6);
    }
  }
  ws2812fx.service();
  switch (tnsyData.mode) { //0=setup, 1=findcache, 2=headlight walking, 3=headlight warn walking, 4= area light, 
    case 0:
    // Setup Mode
      ws2812fx.setBrightness(128);
      if (!modeInitd) {
        Serial.println("Setup Mode");
        Serial.println("GPS ON.");
        digitalWrite(GPSPWRPIN,HIGH);
        gpsEnabled = true;
        ws2812fx.stop();
        Serial2.begin(9600);
        Serial2.flush();
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, 0x000022, 1500, false); // segment 0 is led 0
        ws2812fx.setSegment(1, 1, 3, FX_MODE_STATIC, 0x010101, 0, false); // segment 1 is leds 1-3 Rear
        ws2812fx.setSegment(2, 4, 6, FX_MODE_STATIC, 0x010101, 0, false);  // segment 2 is leds 4-6 Left
        ws2812fx.setSegment(3, 7, 9, FX_MODE_STATIC, 0x010101, 0, false);  // segment 3 is leds 7-9 Front
        ws2812fx.setSegment(4, 10, 12, FX_MODE_STATIC, 0x010101, 0, false);  // segment 3 is leds 10-12 Right
        ws2812fx.start();
        modeInitd = true;
      }
    break;
    case 1:
    // FindCache Mode
      ws2812fx.setBrightness(brightLevel);
      if (!modeInitd) {
        Serial.println("Find Cache Mode");
        Serial.println("GPS ON.");
        digitalWrite(GPSPWRPIN,HIGH);
        gpsEnabled = true;
        ws2812fx.stop();
        Serial2.begin(9600);
        Serial2.flush();
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.setSegment(0, 0, 0, FX_MODE_STATIC, 0x008800, 0, false); // segment 0 is led 0
        ws2812fx.setSegment(1, 1, 3, FX_MODE_STATIC, 0x010101, 0, false); // segment 1 is leds 1-3 Rear
        ws2812fx.setSegment(2, 4, 6, FX_MODE_STATIC, 0x010101, 0, false);  // segment 2 is leds 4-6 Left
        ws2812fx.setSegment(3, 7, 9, FX_MODE_STATIC, 0x010101, 0, false);  // segment 3 is leds 7-9 Front
        ws2812fx.setSegment(4, 10, 12, FX_MODE_STATIC, 0x010101, 0, false);  // segment 3 is leds 10-12 Right
        ws2812fx.start();
        modeInitd = true;
      }
    break;
    case 2:
    // Headlight Walking Mode
      ws2812fx.setBrightness(brightLevel);
      if (!modeInitd) {
        Serial.println("Headlight Walking Mode");
        Serial.println("GPS OFF.");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        ws2812fx.stop();
        Serial2.end();
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.setSegment(0, 0, 0, FX_MODE_STATIC, 0x333333, 0, false); // segment 0 is led 0
        ws2812fx.setSegment(1, 1, 3, FX_MODE_STATIC, 0x010101, 0, false); // segment 1 is leds 1-3 Rear
        ws2812fx.setSegment(2, 4, 6, FX_MODE_STATIC, 0x010101, 0, false);  // segment 2 is leds 4-6 Left
        ws2812fx.setSegment(3, 7, 9, FX_MODE_STATIC, 0xFFFFFF, 0, false);  // segment 3 is leds 7-9 Front
        ws2812fx.setSegment(4, 10, 12, FX_MODE_STATIC, 0x010101, 0, false);  // segment 3 is leds 10-12 Right
        ws2812fx.start();
        modeInitd = true;
      }
    break;
    case 3:
    // Headlight Warn Walking Mode
      ws2812fx.setBrightness(brightLevel);
      if (!modeInitd) {
        Serial.println("Headlight Warn Walking Mode");
        Serial.println("GPS OFF.");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.stop();
        Serial2.end();
        ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, 0x220000, 500, false); // segment 0 is led 0
        ws2812fx.setSegment(1, 1, 3, FX_MODE_BLINK, 0x220000, 500, false); // segment 1 is leds 1-3 Rear
        ws2812fx.setSegment(2, 4, 6, FX_MODE_BLINK, 0xFFFF00, 500, false);  // segment 2 is leds 4-6 Left
        ws2812fx.setSegment(3, 7, 9, FX_MODE_STATIC, 0xFFFFFF, 0, false);  // segment 3 is leds 7-9 Front
        ws2812fx.setSegment(4, 10, 12, FX_MODE_BLINK, 0xFFFF00, 500, false);  // segment 3 is leds 10-12 Right
        ws2812fx.start();
        modeInitd = true;
      }
      break;
    case 4:
    // Area Light Mode
      ws2812fx.setBrightness(brightLevel);
      if (!modeInitd) {
        // parameters: index, start, stop, mode, color, speed, reverse
        Serial.println("Area Light Mode");
        Serial.println("GPS OFF.");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        ws2812fx.stop();
        Serial2.end();
        ws2812fx.setSegment(0, 0, 0, FX_MODE_STATIC, 0xFFFFFF, 0, false); // segment 0 is led 0
        ws2812fx.setSegment(1, 1, 3, FX_MODE_STATIC, 0xFFFFFF, 0, false); // segment 1 is leds 1-3 Rear
        ws2812fx.setSegment(2, 4, 6, FX_MODE_STATIC, 0xFFFFFF, 0, false);  // segment 2 is leds 4-6 Left
        ws2812fx.setSegment(3, 7, 9, FX_MODE_STATIC, 0xFFFFFF, 0, false);  // segment 3 is leds 7-9 Front
        ws2812fx.setSegment(4, 10, 12, FX_MODE_STATIC, 0xFFFFFF, 0, false);  // segment 3 is leds 10-12 Right
        ws2812fx.start();
        modeInitd = true;
      }
    break;

    default:
    Serial.println("case error.");
  }
 
}