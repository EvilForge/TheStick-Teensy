#include <Arduino.h>
#include <TinyGPS++.h>
#include <WS2812FX.h>
#include <EasyTransfer.h>

// Input switch setup
#define FNPIN        18  // aka Pin 18 D18
int inputInterval = 300; // Check 10x/sec.
unsigned int inputCheck = 0; // Next service timer.
bool btnFn = false;

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
#define NUMPIXELS    15
WS2812FX ws2812fx = WS2812FX(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// GPS processing setup
#define GPSPWRPIN    13 // aka Pin 13 D13. High to power up GPS. Serial2 RX,TX D9, D10 Pin 9,Pin10
#define PMTK_SET_NMEA_OUTPUT_RMCONLY "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29"
TinyGPSPlus gps;
double distance = 0;
double courseTo = 0;
bool gpsEnabled = true;

// COM Serial Setup
// Esp serial comms on Serial3 - RX D7, TX D8
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
  double course;
};
ESP_DATA espData;
TEENSY_DATA tnsyData;

//byte mode = 0; 
bool modeInitd = false;
double lastCourse = 0;
double lastCourseTo = 0;

void gpsUpdate() {
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
      tnsyData.course = gps.course.deg();
      Serial.println();
      Serial.println("DATA-RECEIVED: GPS");
      Serial.print(tnsyData.myLat, 6);
      Serial.print(F(","));
      Serial.println(tnsyData.myLon, 6);
      Serial.print("Distance m: ");
      Serial.println(distance*1000);
      Serial.print("Course: ");
      Serial.println(tnsyData.course);
      Serial.print("CourseCard: ");
      Serial.println(gps.cardinal(tnsyData.course));
      Serial.print("CourseTo: ");
      Serial.println(courseTo);
      Serial.print("CourseToCard: ");
      Serial.println(gps.cardinal(courseTo));
      ETOut.sendData();
    } else {
      //Serial.println("No GPS Change");
    }
  } else {
    Serial.println("DATA-RECEIVED: GPS fix invalid.");
  }
}

void setCourse() {
  if ((distance*1000) < 3) { // We're 3m or less - show "Arrived"
    // parameters: index, start, stop, mode, color, speed, reverse
    ws2812fx.setSegment(0, 0, 7, FX_MODE_BLINK, GREEN, 250, false); // segment 0 is led 0-7, the ring
    ws2812fx.start();
    Serial.println("Arrived.");
  } else {
    // CourseTo - Course = Adjusted Course
    int adjCourse = courseTo - tnsyData.course;
    adjCourse = adjCourse < 0 ? adjCourse+360 : adjCourse;
    Serial.print("adjCourse:");
    Serial.println(adjCourse);
    int quadrant = int(adjCourse / 22.5);
    Serial.println(quadrant);
    for (int i=0;i<8;i++) {
      ws2812fx.setPixelColor(i,BLACK);
    }
    ws2812fx.setPixelColor(quadrant,GREEN);
  }
}

void setup() {
  Serial.begin(19200); // Start USB monitor Serial.
  Serial.println("Starting TheTeensyStick.");
  pinMode(FNPIN,INPUT_PULLUP);
  pinMode(BATTPIN,INPUT);
  pinMode(LIGHTPIN,INPUT);
  inputCheck = millis() + inputInterval;
  battCheck = millis();
  ETIn.begin(details(espData),&Serial3);
  ETOut.begin(details(tnsyData),&Serial3);
  ws2812fx.init();
  ws2812fx.setBrightness(128);
  pinMode(GPSPWRPIN,OUTPUT);
  digitalWrite(GPSPWRPIN,HIGH);
  gpsEnabled = true;
  delay(2000);
  Serial2.begin(9600); // Start GPS Serial 2
  Serial2.println(PMTK_SET_NMEA_OUTPUT_RMCONLY); // turn on only the second sentence (GPRMC)
  Serial3.begin(19200);
  tnsyData.mode = 0;
}

void loop() {
  // Event Timer Processing.
  if (millis() > lightCheck) {     // Time to check for light level.
    lightCheck = millis() + lightInterval;
    byte valueAnalog = 0;
    valueAnalog = int(analogRead(LIGHTPIN) * 100/1023);
    valueAnalog = valueAnalog > 100 ? 100 : valueAnalog;
    valueAnalog = valueAnalog < 1 ? 1 : valueAnalog;
    sensorLight = valueAnalog;
    brightLevel = sensorLight*255/100;
    Serial.println("Light: ");
    Serial.println(sensorLight);
  }
  if (millis() > battCheck) {     // Time to check for battery level.
    battCheck = millis() + battInterval;
    float valueAnalog = 0;
    valueAnalog = analogRead(BATTPIN);
    valueAnalog = valueAnalog > 1023 ? 1023 : valueAnalog;
    valueAnalog = valueAnalog < 1 ? 1 : valueAnalog;
    tnsyData.battPct = valueAnalog*5/battFactor;
  }
  if (millis() > inputCheck) {     // Time to check for input buttons.
    inputCheck = millis() + inputInterval;
    if ((digitalRead(FNPIN) == LOW) && !(btnFn)) {
      Serial.println("INPUT: FN Pressed");
      btnFn = true;
    }
    if ((digitalRead(FNPIN) == HIGH) && (btnFn)) {
      Serial.println("INPUT: FN Released");
      btnFn = false;
    }
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
    if (gps.encode(Serial2.read())) {
      gpsUpdate();
    }
  }
  while (Serial3.available() > 0) { // ESP Data is ready.
    if (ETIn.receiveData()) {
      Serial.println();
      Serial.println("DATA-RECEIVED: ESP8266");
      Serial.print(espData.destLat, 6);
      Serial.print(F(","));
      Serial.println(espData.destLon, 6);
    }
  }
  ws2812fx.service();
  ws2812fx.setBrightness(brightLevel);
  switch (tnsyData.mode) { //0=setup, 1=findcache, 2=headlight walking, 3=headlight warn walking, 4= area light, 
    case 0:
    // Setup Mode
      if (!modeInitd) {
        Serial.println("NEW-MODE: Setup");
        digitalWrite(GPSPWRPIN,HIGH);
        gpsEnabled = true;
        ws2812fx.stop();
        Serial2.begin(9600);
        Serial2.flush();
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.setSegment(0, 0, 7, FX_MODE_SCAN, BLUE, 1000, false); // segment 0 is led 0-7, the ring
        ws2812fx.setSegment(1, 8, 11, FX_MODE_SCAN, BLUE, 1000, false); // segment 1 is leds 8-11 Front
        ws2812fx.setSegment(2, 12, 14, FX_MODE_SCAN, BLUE, 1000, false);  // segment 2 is leds 12-14 bottom side & rear
        ws2812fx.start();
        modeInitd = true;
      }
      if (tnsyData.battPct < 3.00) {
        ws2812fx.setSegment(0, 0, 7, FX_MODE_BLINK, RED, 500, false); // segment 0 is led 0          
      } else {
        ws2812fx.setSegment(0, 0, 7, FX_MODE_SCAN, BLUE, 1000, false); // segment 0 is led 0
      }
    break;
    case 1:
    // FindCache Mode
      if (!modeInitd) {
        Serial.println("NEW-MODE: Find Cache");
        digitalWrite(GPSPWRPIN,HIGH);
        gpsEnabled = true;
        ws2812fx.stop();
        Serial2.begin(9600);
        Serial2.flush();
        // parameters: index, start, stop, mode, color, speed, reverse
        ws2812fx.setSegment(0, 0, 7, FX_MODE_STATIC, BLACK, 0, false); // segment 0 is led 0-7, the ring
        ws2812fx.setSegment(1, 8, 11, FX_MODE_STATIC, BLACK, 0, false); // segment 1 is leds 8-11 Front
        ws2812fx.setSegment(2, 12, 14, FX_MODE_STATIC, BLACK, 0, false);  // segment 2 is leds 12-14 bottom side & rear
        ws2812fx.start();
        modeInitd = true;
      }
      if ((lastCourse != tnsyData.course) || (lastCourseTo != courseTo)) {
        // Course Change
        setCourse();
      }
    break;
    case 2:
    // Headlight Walking Mode
      if (!modeInitd) {
        Serial.println("NEW-MODE: Walking light");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        ws2812fx.stop();
        Serial2.end();
        ws2812fx.setSegment(0, 0, 7, FX_MODE_STATIC, BLACK, 0, false); // segment 0 is led 0-7, the ring
        ws2812fx.setSegment(1, 8, 11, FX_MODE_STATIC, WHITE, 0, false); // segment 1 is leds 8-11 Front
        ws2812fx.setSegment(2, 12, 14, FX_MODE_STATIC, BLACK, 0, false);  // segment 2 is leds 12-14 bottom side & rear
        ws2812fx.start();
        modeInitd = true;
      }
      if (tnsyData.battPct < 3.00) {
        ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, RED, 500, false); // segment 0 is led 0          
      } else {
        ws2812fx.setSegment(0, 0, 0, FX_MODE_STATIC, BLACK, 0, false); // segment 0 is led 0
      }
    break;
    case 3:
    // Headlight Warn Walking Mode
      if (!modeInitd) {
        Serial.println("NEW-MODE: Walking warning");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        ws2812fx.stop();
        Serial2.end();
        ws2812fx.setSegment(0, 0, 7, FX_MODE_STATIC, BLACK, 0, false); // segment 0 is led 0-7, the ring
        ws2812fx.setSegment(1, 8, 10, FX_MODE_STATIC, WHITE, 0, false); // segment 1 is leds 8-10 Front
        ws2812fx.setSegment(2, 11, 14, FX_MODE_BLINK, YELLOW, 1500, false);  // segment 2 is leds 11-14 bottom
        ws2812fx.start();
        modeInitd = true;
      }
      if (tnsyData.battPct < 3.00) {
        ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, RED, 500, false); // segment 0 is led 0          
      } else {
        ws2812fx.setSegment(0, 0, 0, FX_MODE_STATIC, BLACK, 0, false); // segment 0 is led 0
      }
      break;
    case 4:
    // Area Light Mode
      if (!modeInitd) {
        Serial.println("NEW-MODE: Area light");
        Serial.println("GPS OFF.");
        digitalWrite(GPSPWRPIN,LOW);
        gpsEnabled = false;
        ws2812fx.stop();
        Serial2.end();
        ws2812fx.setSegment(0, 0, 7, FX_MODE_STATIC, WHITE, 0, false); // segment 0 is led 0-7, the ring
        ws2812fx.setSegment(1, 8, 11, FX_MODE_STATIC, WHITE, 0, false); // segment 1 is leds 8-11 Front
        ws2812fx.setSegment(2, 12, 14, FX_MODE_STATIC, WHITE, 0, false);  // segment 2 is leds 12-14 bottom side & rear
        ws2812fx.start();
        modeInitd = true;
      }
      if (tnsyData.battPct < 3.00) {
        ws2812fx.setSegment(0, 0, 0, FX_MODE_BLINK, RED, 500, false); // segment 0 is led 0          
      } else {
        ws2812fx.setSegment(0, 0, 7, FX_MODE_STATIC, WHITE, 0, false); // segment 0 is led 0
      }
    break;
    default:
    // No Operation.
    break;
  }
}