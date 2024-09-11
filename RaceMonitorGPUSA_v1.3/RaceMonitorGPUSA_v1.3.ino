/*

Race Monitor... race monitor
  This code pings the Race Montior server for evaluating competition data such as laps and lap times.
se
  1) Receive data from Race Monitor (require internet connection, phone hot spot, other....
  2) incoming JSON data processed and evaluated, namely to determine if any car has 3 consecutive laps < 15 MPH
  3) a small display reports any violating cars and a buzzer give an audible alert

  This code was adapted from the Patriot Racing (c) Race Monitor (TM)/ Usage is dependent on having a security 
  token provided by Race Monitor. The free developer license, which is Greenpower USA has allows limited 
  server reads per month.

  GPUSA Token: 9f383b38-e448-405c-b3ab-babd695414f6
  
Licensing
  Bob Jones Patriot Racing
  Copyright 2016-2024, All Rights reserved
  This code is property of Patriot Racing and Kris Kasprzak
  This code cannot be used outside of Bob Jones High School without written consent.
  Usage of this code is licensed to Greenpower USA for the purpose of reading live race time data from RaceMonitor for 
  determining if a car has 3 consecutive laps that are under a limit


  if ESP32 fails to program press BOOT button on ESP32 during programming process
  must install ESP32 software
  compile for ESP23 dev module
  all defaults should be fine

  Ver   Data    Author        Description
  1.0   7/2024  Kasprzak      Adapted from Patriot Racing code
  1.1   7/2024  Kasprzak      Added graphing, buzzer and other capabilities

*/

#include <WiFi.h>           // Use in additional boards manager https://dl.espressif.com/dl/package_esp32_index.json
#include <WebServer.h>      // Use in additional boards manager https://dl.espressif.com/dl/package_esp32_index.json
#include <HTTPClient.h>     // Use in additional boards manager https://dl.espressif.com/dl/package_esp32_index.json
#include "time.h"           // https://github.com/lattera/glibc/blob/master/time/time.h
#include "elapsedMillis.h"  // https://github.com/pfeerick/elapsedMillis/blob/master/elapsedMillis.h
#include "Wire.h"
#include "WiFiClientSecure.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>  // https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include "Colors.h"               // simple color definitions for example, C_RED = 0xF800
#include "Adafruit_ILI9341.h"
#include "Adafruit_GFX.h"
#include "fonts\FreeSans18pt7b.h"  // fonts come with Adafr
#include "fonts\FreeSans12pt7b.h"
#include "fonts\FreeSansBold12pt7b.h"
#include "fonts\FreeSans9pt7b.h"
#include <FlickerFreePrint.h>           // https://github.com/KrisKasprzak/FlickerFreePrint
#include <Adafruit_ILI9341_Controls.h>  // https://github.com/KrisKasprzak/Adafruit_ILI9341_Controls
#include <EEPROM.h>
#include <ArduinoJson.h>                // https://github.com/bblanchon/ArduinoJson
#include "Adafruit_ILI9341_Keypad.h"    // https://github.com/KrisKasprzak/Adafruit_ILI9341_Keypad
#include <GPUSA_Icons.h>                // inludes only arrow icons
#include <SdFat.h>                      // SD card           https://github.com/greiman/SdFat

#define CODE_VERSION "1.3"

// #define SHOW_DEBUG

#define FONT_DATA FreeSans9pt7b
#define FONT_HEADING FreeSans18pt7b
#define FONT_ITEM FreeSans12pt7b
#define FONT_BUTTON FreeSans12pt7b
#define C_RADIUS 4
#define BORDER_THICKNESS 0

#define T_CS 2
#define T_IRQ 15

#define TFT_DC 12
#define TFT_CS 27
#define TFT_RST 5
#define PIN_TONE 25
#define PIN_SDCS 32

#define STATUS_NEW 0
#define STATUS_DRAW 2
#define STATUS_DONE 3
#define SD_SPI_SPEED 20
#define DISABLE_COLOR 0x39e7

#define SSID_START 10
#define SSID_LENGTH 30
#define HTTP_TIMEOUT 15000
#define PW_START 50
#define PW_LENGTH 30

#define MAX_CARS 100
#define MAX_LAPS 100

#define X_ORIGIN 30
#define Y_ORIGIN 170
#define X_WIDE 260
#define Y_HIGH 110
#define X_LOSCALE 0
#define X_HISCALE 100
#define X_INC 10
#define Y_LOSCALE 0
#define Y_HISCALE 30
#define Y_INC 5

#define NO_VIOLATION 0
#define REMOVE_VIOLATION 1

// the sample text which we are storing in EEPROM
char CarNumber[MAX_CARS][5];
uint8_t NewLap[MAX_CARS];
uint8_t OldLap[MAX_CARS];
uint16_t CarLapTime[MAX_CARS][MAX_LAPS];
uint8_t CarStatus[MAX_CARS];
bool CarIsPrinted[MAX_CARS];
uint8_t SimulationIteration = 0;
bool found = false;
bool FoundInternet = false;
bool FoundRace = false;
int SpeedID = 0;
uint16_t ScreenLeft = 270, ScreenRight = 3720, ScreenTop = 320, ScreenBottom = 3750;
int Row;
char buf[10];
char timebuf[10];
char buffer[12];
char CRDSeriesName[70];
char CRDName[70];
char CRDTrack[70];

char RDSessionName[70];
char RDTrackName[70];

char RDStartTime[20];
char CurrentRaceTime[10];
char FileName[15] = "GPUSA_000.csv";
uint16_t TotalServerPings = 0;
uint16_t MaxLapTime = 0;
bool ViolationsFound = false;
int BtnX, BtnY, BtnZ;
int HTTPCodeRaceData = 0;
int HTTPCodeCurrentRace = 0;
int x;
bool KeepIn = true;
int i;
float Progress = 0.0f;
bool Alarm = false;
int h = 0, m = 0, s = 0;
float TrackLength = 0.0f;
uint8_t RaceMaxLaps = 0;
int RefreshID0, RefreshID1, RefreshID2;
uint8_t RefreshRateID = 0;
uint8_t RefreshRate = 15;
uint8_t NumberOfCars = 0;
int NumberOfSites = 0, CurrentSite = 0, SiteToUse = 0;
int NumberOfRaces = 0, CurrentRace = 0, SelectedRace = 999;
int32_t RaceTypeID = 0, SelectedRaceID = 0;


String SSID;
String PW;

String JSONCurrentRace;
String JSONRaceData;

byte Start = 0;

Adafruit_ILI9341 Display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

XPT2046_Touchscreen Touch(T_CS, T_IRQ);

TS_Point TP;

IPAddress Actual_IP;
IPAddress PR_IP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

WebServer server(443);

JsonDocument CurrentRaceDoc;
JsonDocument RaceDoc;

DeserializationError CurrentRaceError;
DeserializationError RaceDataError;

elapsedSeconds UpdateRaceData;
elapsedMillis ProgressUpdate;
elapsedMillis AlarmUpdate;

Button MonitorBtn(&Display);
Button DownloadBtn(&Display);
Button SetRaceBtn(&Display);
Button SettingsBtn(&Display);
Button InternetConnectBtn(&Display);

Button CancelAlarm(&Display);
Button SDStatusBTN(&Display);
Button DoneBtn(&Display);
Button TrackLengthBtn(&Display);
Button RefreshBtn(&Display);
Button UpBtn(&Display);
Button DownBtn(&Display);

Button ConnectBtn(&Display);
Button PasswordBtn(&Display);

Button RaceIDBtn(&Display);
Button MenuBtn(&Display);

Button RemoveBtn(&Display);
Button SilenceBtn(&Display);

OptionButton RaceMonitorRefresh(&Display);
Keyboard KeyboardInput(&Display, &Touch);
NumberPad GetTrackLength(&Display, &Touch);

FlickerFreePrint<Adafruit_ILI9341> HeaderText(&Display, C_WHITE, C_BLACK);

FlickerFreePrint<Adafruit_ILI9341> ConnectStatus(&Display, C_WHITE, C_BLACK);
FlickerFreePrint<Adafruit_ILI9341> RaceProgressText(&Display, C_WHITE, C_BLACK);

CGraph Violator(&Display, X_ORIGIN, Y_ORIGIN, X_WIDE, Y_HIGH, X_LOSCALE, X_HISCALE, X_INC, Y_LOSCALE, Y_HISCALE, Y_INC);

/*

standard setup for initializing objects
main menu in here, once menu complete, loop monitors race

*/

void setup() {

  Serial.begin(115200);


#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG setup()_____________________________________"));
#endif


  // disable the watchdog timers--not really sure these work
  // disableCore0WDT();
  // disableCore1WDT();

  EEPROM.begin(200);

  Display.begin();

  analogWriteResolution(PIN_TONE, 127);
  analogWriteFrequency(PIN_TONE, 500);

  analogWrite(PIN_TONE, 127);
  delay(50);
  analogWrite(PIN_TONE, 0);
  delay(50);
  analogWrite(PIN_TONE, 127);
  delay(50);
  analogWrite(PIN_TONE, 0);


  Display.setRotation(3);
  Display.fillScreen(C_BLACK);

  draw565Bitmap(12, 20, GPUSA_Logo, 295, 125);

  Display.setTextColor(C_WHITE);
  Display.setFont(&FONT_DATA);
  Display.setCursor(70, 170);
  Display.print(F("Lap Speed Monitor "));
  Display.print(F(CODE_VERSION));
#ifdef SHOW_DEBUG
  Display.print(F("-DEBUG"));
#endif
  Display.setFont(&FONT_ITEM);

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 10, 30, 4, C_GREY);
  Display.setCursor(30, 223);
  Display.print(F("Creating interface"));

  Touch.begin();
  Touch.setRotation(3);

  //debuging touch sceen press location
  //while (1) {
  //  ProcessTouch();
  //}

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 20, 30, 4, C_GREY);
  Display.setCursor(30, 223);
  Display.print(F("Reading settings"));

  /*
  Serial.println("Clearing EEPROM");
  for (int i = 0; i < 200; i++) {
    EEPROM.put(i, 255);
    EEPROM.commit();
  }
  */

  GetParameters();
  CreateInterface();
  RebuildArrays();


  /*
  CreateTestData();
  DrawRaceProgressScreen();
  UpdateRaceProgress();

  RefreshRate = 15;

  delay(RefreshRate * 1000);
  for (SimulationIteration = 1; SimulationIteration < 30; SimulationIteration++) {
    SimulateRace();
    FindViolations();
    UpdateRaceProgress();
    delay(RefreshRate * 1000);
  }

  while (1) {}
*/

  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setFont(&FONT_ITEM);

  // can we connect to internet based on cached data?

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 25, 30, 4, C_GREY);
  Display.setCursor(30, 223);
  Display.print(F("Finding: "));

  if (SSID.length() > 10) {
    for (i = 0; i < 10; i++) {
      Display.print(SSID[i]);
    }
    Display.print(F("..."));
  } else {
    Display.print(SSID);
  }


  if (FindMatchingSite()) {

    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 30, 30, 4, C_GREY);
    Display.setCursor(30, 223);
    Display.print(F("Joining: "));
    String temp = WiFi.SSID(SiteToUse);
    if (temp.length() > 10) {
      for (i = 0; i < 10; i++) {
        Display.print(temp[i]);
      }
      Display.print(F("..."));
    } else {
      Display.print(temp);
    }

    // try to connect...
    // Serial.println("found matching site");
    if (ConnectToMatchingSite()) {
      //Serial.println("connected");
      FoundInternet = true;
    } else {
      //Serial.println("Could not connect to cached site");
      FoundInternet = false;
    }
  } else {
    //Serial.println("Could not find cached site");
  }

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_GREY);
  Display.setCursor(30, 223);
  Display.print(F("Setup complete"));
  delay(500);

  MainMenu();

  DrawRaceProgressScreen();

  UpdateRaceData = 99000;  // force initial read

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG setup()_____________________________________"));
  Serial.println();
#endif
}

/*

loop where all race monitoring is done. by this tim valid internet connect and race is found
user can access settings to change intgernet, race, or settings

*/
void loop() {

  if (Touch.touched()) {
    ProcessTouch();
    if (PressIt(MenuBtn) == true) {
      MainMenu();
      DrawRaceProgressScreen();
      UpdateRaceProgress();
      UpdateRaceData = 9900;  // force update
    }
  }

  if (UpdateRaceData >= RefreshRate) {

    UpdateRaceData = 0;

    if (FoundInternet) {
      Display.setTextColor(C_BLACK);
      Display.setFont(&FONT_ITEM);

      Display.fillRoundRect(20, 210, 280, 28, 4, C_DKGREEN);

      Display.setCursor(30, 231);
      Display.print(F("Contacting server"));

      // SimulateRace();

      GetCarLapTimes();

      FindViolations();

      Display.fillRoundRect(20, 210, 280, 28, 4, C_DKGREEN);

      UpdateRaceProgress();
    }
  }

  if (ProgressUpdate >= 1000) {

    ProgressUpdate = 0;
    Progress = (280.0f * UpdateRaceData) / RefreshRate;

    if (Progress > 280) {
      Progress = 280;
    }

    Display.fillRoundRect(20, 210, Progress, 28, 4, C_GREEN);

    Display.setTextColor(C_BLACK);
    Display.setFont(&FONT_ITEM);

    Display.setCursor(25, 231);
    Display.print(F("Max lap time: "));
    m = MaxLapTime / 60;
    s = MaxLapTime % 60;
    sprintf(buf, "%d:%02d", m, s);
    Display.print(buf);
  }
}

/*

function to update the screen with race data, sorted by slowest car that is still on the track
if car is slow (<15 MPH for 3 laps) GPUSA will pull it off the track

*/
void UpdateRaceProgress() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG UpdateRaceProgress()_____________________________________"));
#endif

  int i = 0, j = 0;
  uint16_t LongestLapTime = 0;
  uint8_t Marker = 0;
  uint16_t Row = 0, RowCount = 0;

  Display.setFont(&FONT_HEADING);
  Display.setCursor(10, 30);
  RaceProgressText.setTextColor(C_MDRED, C_LTGREY);

  // error trap
  if (!FoundInternet) {
    RaceProgressText.print("No Internet");
  } else if (!FoundRace) {
    RaceProgressText.print("Race Lost");
  } else if (HTTPCodeCurrentRace == 430) {
    RaceProgressText.print("Read Error");
  } else if (HTTPCodeCurrentRace == 429) {
    RaceProgressText.print("Limit Error");
  } else if (HTTPCodeCurrentRace == 500) {
    RaceProgressText.print("Server Error.");
  } else if (HTTPCodeRaceData == 430) {
    RaceProgressText.print("Read Error");
  } else if (HTTPCodeRaceData == 429) {
    RaceProgressText.print("Limit Error");
  } else if (HTTPCodeRaceData == 500) {
    RaceProgressText.print("Server Error.");
  } else {
    RaceProgressText.setTextColor(C_BLACK, C_LTGREY);
    RaceProgressText.print("Progress");
  }



  Display.setFont(&FONT_ITEM);
  Display.setTextColor(C_WHITE, C_BLACK);

  for (i = 0; i < MAX_CARS; i++) {
    CarIsPrinted[i] = false;
  }

  Display.fillRect(5, 65, 310, 135, C_BLACK);

  for (j = 0; j < 5; j++) {
    Row = (28 * RowCount) + 85;
    LongestLapTime = 0;
    Marker = 0;
    for (i = 0; i < NumberOfCars; i++) {
      if ((!CarIsPrinted[i]) && (CarLapTime[i][NewLap[i]] >= LongestLapTime)) {
        Marker = i;
        LongestLapTime = CarLapTime[i][NewLap[i]];
      }
    }
#ifdef SHOW_DEBUG
    Serial.println("UpdateRaceProgress() ____________________________");
    Serial.print("j=");
    Serial.print(j);
    Serial.print(", i=");
    Serial.print(i);
    Serial.print(", Marker=");
    Serial.print(Marker);
    Serial.print(", Car=");
    Serial.print(CarNumber[Marker]);
    Serial.print(", New lap = ");
    Serial.print(NewLap[Marker]);
    Serial.print(", Old Lap = ");
    Serial.print(OldLap[Marker]);
    Serial.print(", CarLapTime=");
    Serial.print(CarLapTime[Marker][NewLap[Marker]]);
    Serial.print(", CarIsPrinted[Marker]=");
    Serial.print(CarIsPrinted[Marker]);
    Serial.print(", CarStatus=");
    Serial.print(CarStatus[Marker]);
    Serial.print(", LongestLapTime=");
    Serial.println(LongestLapTime);
#endif
    if (!CarIsPrinted[Marker]) {
      CarIsPrinted[Marker] = true;
      if (CarStatus[Marker] == NO_VIOLATION) {
        RowCount++;
        // print stuff found at marker
        Display.setCursor(5, Row);
        Display.print(CarNumber[Marker]);
        Display.setCursor(60, Row);
        Display.print(NewLap[Marker]);
        Display.setCursor(130, Row);
        m = CarLapTime[Marker][NewLap[Marker]] / 60;
        s = CarLapTime[Marker][NewLap[Marker]] % 60;
        sprintf(timebuf, "%01d:%02d", m, s);
        // shit
        //Display.print(CarLapTime[Marker][NewLap[Marker]]);
        Display.print(timebuf);
        Display.setCursor(240, Row);
        Display.print(ComputeMPH(CarLapTime[Marker][NewLap[Marker]]));
      }
    }
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG UpdateRaceProgress()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw the fixed text

*/

void DrawRaceProgressScreen() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawRaceProgressScreen()_____________________________________"));

#endif
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);

  Display.setFont(&FONT_HEADING);
  Display.fillRect(0, 0, 320, 40, C_LTGREY);
  RaceProgressText.setTextColor(C_BLACK, C_LTGREY);

  Display.setCursor(10, 30);
  RaceProgressText.print("Waiting...");

  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setFont(&FONT_ITEM);

  Display.setCursor(5, 60);
  Display.print(F("Car"));
  Display.setCursor(50, 60);
  Display.print(F("Laps"));
  Display.setCursor(130, 60);
  Display.print(F("Lap Time"));
  Display.setCursor(240, 60);
  Display.print(F("Speed"));

  MenuBtn.draw();
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawRaceProgressScreen()_____________________________________"));
  Serial.println();
#endif
}

/*

function to find any violations (cars < 15 MPH for 3 laps)

*/

void FindViolations() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG FindViolations()_____________________________________"));
#endif
  uint16_t CarLapTime3 = 0;
  uint16_t CarLapTime2 = 0;
  uint16_t CarLapTime1 = 0;
  ViolationsFound = false;

  for (i = 0; i < NumberOfCars; i++) {

    if (NewLap[i] >= 3) {

      CarLapTime3 = CarLapTime[i][NewLap[i] - 2];
      CarLapTime2 = CarLapTime[i][NewLap[i] - 1];
      CarLapTime1 = CarLapTime[i][NewLap[i]];

#ifdef SHOW_DEBUG
      Serial.print("Car: ");
      Serial.print(CarNumber[i]);
      Serial.print(", L3: ");
      Serial.print(CarLapTime[i][NewLap[i] - 2]);
      Serial.print(", Speed: ");
      Serial.print(ComputeMPH(CarLapTime3), 1);
      Serial.print(", L2: ");
      Serial.print(CarLapTime[i][NewLap[i] - 1]);
      Serial.print(", Speed: ");
      Serial.print(ComputeMPH(CarLapTime2), 1);
      Serial.print(", L1: ");
      Serial.print(CarLapTime[i][NewLap[i] - 0]);
      Serial.print(", Speed: ");
      Serial.println(ComputeMPH(CarLapTime1), 1);
      Serial.print("565 Car #: ");
      Serial.print(CarNumber[i]);
      Serial.print("Laps: ");
      Serial.print(NewLap[i]);
      Serial.print("Laps -1: ");
      Serial.println(OldLap[i]);
#endif

      if (OldLap[i] != NewLap[i]) {
        if ((CarLapTime3 > 0) && (ComputeMPH(CarLapTime3) < 15) && (CarLapTime2 > 0) && (ComputeMPH(CarLapTime2) < 15) && (CarLapTime1 > 0) && (ComputeMPH(CarLapTime1) < 15)) {
          if (CarStatus[i] == NO_VIOLATION) {
            ShowViolations(i);
            CarStatus[i] = REMOVE_VIOLATION;
            ViolationsFound = true;
          }
        }
      }
    }
  }

  if (ViolationsFound) {
    DrawRaceProgressScreen();
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG FindViolations()_____________________________________"));
  Serial.println();
#endif
}

float ComputeMPH(uint16_t LapTime) {
  float TempMPH = 0.0f;
  if (LapTime > 0) {
    TempMPH = (TrackLength * 3600.0f) / (LapTime);
  }

  return TempMPH;
}

/*

function to SHOW any violations (cars < 15 MPH for 3 laps)
draws a graph so GPUSA can see speed trend

*/

void ShowViolations(uint8_t CarID) {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG ShowViolations()_____________________________________"));
#endif
  // UI to draw screen and capture input
  bool MainMenuKeepIn = true;
  uint8_t i = 0;
  bool SoundAlarm = true;
  float CarSpeed = 0.0f;
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);

  Display.fillRect(0, 0, 320, 40, C_LTGREY);

  DoneBtn.draw();
  RemoveBtn.draw();
  SilenceBtn.draw();

  if (NewLap[CarID] < 10) {
    Violator.setXAxis(0, 10, 1);
  } else if (NewLap[CarID] < 20) {
    Violator.setXAxis(0, 20, 2);
  } else if (NewLap[CarID] < 40) {
    Violator.setXAxis(0, 40, 5);
  } else {
    Violator.setXAxis(0, 100, 10);
  }

  Violator.drawGraph();  // draw empty graph if you have a long delay before any plottable data

  Display.fillRect(X_ORIGIN, Y_ORIGIN - (Y_HIGH / 2) - 2, X_WIDE, 4, C_RED);  // line at 15 MPH
  Display.setTextColor(C_YELLOW);
  Display.setFont(&FONT_HEADING);
  Display.setCursor(X_ORIGIN + 10, Y_ORIGIN - (Y_HIGH / 2) + 32);  // line at 15 MPH

  m = MaxLapTime / 60;
  s = MaxLapTime % 60;

  sprintf(buf, "%d:%02d", m, s);
  Display.print(buf);

  for (i = 0; i < NewLap[CarID]; i++) {  // pass violatuing car number
    CarSpeed = ComputeMPH(CarLapTime[CarID][i]);
    Violator.setX(i);
    Violator.plot(SpeedID, CarSpeed);
  }

  while (MainMenuKeepIn) {
    delay(50);

    Display.setFont(&FONT_HEADING);
    Display.setCursor(10, 30);
    if (AlarmUpdate > 500) {
      AlarmUpdate = 0;
      Alarm = !Alarm;
      if (Alarm) {
        Display.setTextColor(C_DKRED, C_LTGREY);
        Display.print(F("Car: "));
        Display.print(CarNumber[CarID]);
        if (SoundAlarm) {
          analogWrite(PIN_TONE, 255);
        }

      } else {
        Display.setTextColor(C_BLACK, C_LTGREY);
        Display.print(F("Car: "));
        Display.print(CarNumber[CarID]);
        analogWrite(PIN_TONE, 0);
      }
    }

    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();

      if (PressIt(DoneBtn) == true) {
        analogWrite(PIN_TONE, 0);
        MainMenuKeepIn = false;
      }

      if (PressIt(RemoveBtn) == true) {
        // mark car status as remove
        analogWrite(PIN_TONE, 0);
        CarStatus[CarID] = REMOVE_VIOLATION;
        MainMenuKeepIn = false;
      }
      if (PressIt(SilenceBtn) == true) {
        // cancel alarm
        SoundAlarm = !SoundAlarm;
      }
    }
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG ShowViolations()_____________________________________"));
  Serial.println();
#endif
}

/*

function to let user cycle through known current races from Race Monitor
goal is for user to select the appropraite GPUSA race

*/
void SelectRace() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG SelectRace()_____________________________________"));
#endif
  // UI to draw screen and capture input
  bool SelectRaceKeepIn = true;

  DrawSelectRaceScreen();

  DrawStatusBar(STATUS_NEW, 0);
  FoundRace = GetListOfRaces();

  Display.setFont(&FONT_HEADING);
  Display.setCursor(10, 30);

  if (!FoundRace) {
    ConnectStatus.setTextColor(C_MDRED, C_LTGREY);
    ConnectStatus.print("No Races");
  } else {
    ConnectStatus.setTextColor(C_BLACK, C_LTGREY);
    ConnectStatus.print("Select Race");
  }

  DrawStatusBar(STATUS_DRAW, 100);
  delay(100);
  DrawStatusBar(STATUS_DONE, 0);

  CurrentRace = 0;
  DrawSelectedRace();

  while (SelectRaceKeepIn) {
    delay(50);

    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();

      if (PressIt(DoneBtn) == true) {
        SelectedRace = CurrentRace;
        SelectRaceKeepIn = false;
      }
      if (PressIt(DownBtn) == true) {
        CurrentRace++;
        if (CurrentRace >= NumberOfRaces) {
          CurrentRace = 0;
        }
        DrawSelectedRace();
      }

      if (PressIt(UpBtn) == true) {

        if (CurrentRace == 0) {
          CurrentRace = NumberOfRaces - 1;
        } else {
          CurrentRace--;
        }
        DrawSelectedRace();
      }
      if (PressIt(RefreshBtn) == true) {
        DrawStatusBar(STATUS_NEW, 0);
        GetListOfRaces();
        DrawStatusBar(STATUS_DRAW, 100);
        delay(100);
        DrawStatusBar(STATUS_DONE, 0);
        CurrentRace = 0;
        DrawSelectedRace();
      }

      if (PressIt(ConnectBtn) == true) {
        DrawStatusBar(STATUS_NEW, 0);
        DrawStatusBar(STATUS_DRAW, 10);
        GetInitialRaceData();
        RebuildArrays();
        DrawStatusBar(STATUS_DRAW, 50);
        FoundRace = BuildCarList();
        DrawStatusBar(STATUS_DRAW, 100);
        DrawStatusBar(STATUS_DONE, 0);

        Display.setFont(&FONT_HEADING);
        Display.setCursor(10, 30);

        if (!FoundRace) {
          ConnectStatus.setTextColor(C_MDRED, C_LTGREY);
          TrackLength = 0;
        } else {
          ConnectStatus.setTextColor(C_DKGREEN, C_LTGREY);
        }
        sprintf(buffer, "Cars:%d,%0.2f", NumberOfCars, TrackLength);
        ConnectStatus.print(buffer);
      }

      if (PressIt(RaceIDBtn) == true) {
        Display.fillScreen(C_BLACK);
        strcpy(KeyboardInput.data, "");
        KeyboardInput.getInput();
        strcpy(buffer, KeyboardInput.data);
        SelectedRaceID = (uint32_t)atoi(buffer);
#ifdef SHOW_DEBUG
        Serial.print("847 SelectedRaceID ");
        Serial.println(SelectedRaceID);
#endif
        DrawSelectRaceScreen();

        Display.setFont(&FONT_HEADING);
        Display.setCursor(10, 30);
        ConnectStatus.setTextColor(C_BLACK, C_LTGREY);
        ConnectStatus.print("Pending");

        Display.fillRect(10, 45, 320, 40, C_BLACK);
        Display.setCursor(60, 128);
        Display.print("Race ID: ");
        Display.print(SelectedRaceID);

        Display.setTextColor(C_WHITE, C_BLACK);
        Display.setCursor(15, 64);
        Display.println("Series: Pending");
        Display.setCursor(15, 88);
        Display.println("Name: Pending");
      }
    }
  }

  EEPROM.put(5, SelectedRaceID);
  EEPROM.commit();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG SelectRace()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw the race selection screen--the fixed text

*/
void DrawSelectRaceScreen() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawSelectRaceScreen()_____________________________________"));
#endif
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);


  Display.setFont(&FONT_HEADING);
  Display.fillRect(0, 0, 320, 40, C_LTGREY);
  Display.setCursor(10, 30);
  Display.setTextColor(C_BLACK, C_WHITE);

  ConnectStatus.setTextColor(C_BLACK, C_LTGREY);
  ConnectStatus.print("Select Race");

  DoneBtn.draw();
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setFont(&FONT_ITEM);

  ConnectBtn.enable();
  RefreshBtn.draw();
  ConnectBtn.draw();
  UpBtn.draw();
  DownBtn.draw();
  RaceIDBtn.draw();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawSelectRaceScreen()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw the data from Race Monitor

*/
void DrawSelectedRace() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawSelectedRace()_____________________________________"));
#endif
  Display.setFont(&FONT_ITEM);
  Display.setTextColor(C_MDRED, C_BLACK);

  // do some error processing....
  Display.setCursor(15, 75);
  if (WiFi.status() != WL_CONNECTED) {
    Display.print("No Intenet.");
  } else if (NumberOfRaces == 0) {
    Display.print("No races found, Refresh.");
  } else if (HTTPCodeCurrentRace == 430) {
    Display.print("Read error, bad endpoint.");
  } else if (HTTPCodeCurrentRace == 429) {
    Display.print("Usage limit exceeded.");
  } else if (HTTPCodeCurrentRace == 500) {
    Display.print("Race Monitor server error.");
  } else if (HTTPCodeRaceData == 430) {
    Display.print("Read Error");
  } else if (HTTPCodeRaceData == 429) {
    Display.print("Limit Error");
  } else if (HTTPCodeRaceData == 500) {
    Display.print("Server Error.");
  }

  Display.setTextColor(C_WHITE, C_BLACK);

  if (NumberOfRaces > 0) {

    Display.setTextColor(C_WHITE, C_BLACK);
    Display.setFont(&FONT_ITEM);

    Display.fillRect(57, 100, 200, 40, C_BLACK);

    RaceTypeID = CurrentRaceDoc["Races"][CurrentRace]["RaceTypeID"];
    SelectedRaceID = CurrentRaceDoc["Races"][CurrentRace]["ID"];

    const char *TempName = CurrentRaceDoc["Races"][CurrentRace]["Name"];

    if (TempName != nullptr) {
      strcpy(CRDName, TempName);
    } else {
      strcpy(CRDName, "Name ?");
    }

    const char *TempSeriesName = CurrentRaceDoc["Races"][CurrentRace]["SeriesName"];

    if (TempSeriesName != nullptr) {
      strcpy(CRDSeriesName, TempSeriesName);
    } else {
      strcpy(CRDSeriesName, "Series Name: ?");
    }

    const char *TempTrack = CurrentRaceDoc["Races"][CurrentRace]["Track"];

    if (TempTrack != nullptr) {
      strcpy(CRDTrack, TempTrack);
    } else {
      strcpy(CRDTrack, "Track Name: ?");
    }

#ifdef SHOW_DEBUG
    Serial.print("Name: ");
    Serial.print(CRDName);
    Serial.print(", SeriesName: ");
    Serial.print(CRDSeriesName);
    Serial.print(", Track: ");
    Serial.print(CRDTrack);
    Serial.print(", RaceTypeID = ");
    Serial.print(RaceTypeID);
    Serial.print(", ID = ");
    Serial.println(SelectedRaceID);
#endif

    Display.setCursor(60, 128);
    Display.print("Race ID: ");
    Display.print(SelectedRaceID);

    Display.fillRect(10, 45, 320, 51, C_BLACK);

    // show series name
    Display.setCursor(15, 64);
    if (strlen(CRDSeriesName) > 24) {
      for (i = 0; i < 24; i++) {
        Display.print(CRDSeriesName[i]);
      }
      Display.print("...");
    } else {
      Display.println(CRDSeriesName);
    }

    // show  name
    Display.setCursor(15, 88);
    if (strlen(CRDName) > 24) {
      for (i = 0; i < 24; i++) {
        Display.print(CRDName[i]);
      }
      Display.print("...");
    } else {
      Display.println(CRDName);
    }
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawSelectedRace()_____________________________________"));
  Serial.println();
#endif
}


/*

function to let users choose found SSID and connect to the internet

*/

void ConnectToInternet() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG ConnectToInternet()_____________________________________"));
#endif

  uint8_t s = 0;
  uint8_t temp = 0;

  // UI to draw screen and capture input
  bool ConnectToInternetKeepIn = true;

  DrawConnectToInternet();
  DrawConnectToInternetHeader();

  DrawStatusBar(STATUS_NEW, 0);
  GetSites();
  DrawStatusBar(STATUS_DRAW, 100);
  delay(100);
  DrawStatusBar(STATUS_DONE, 0);

  SiteToUse = 0;
  DrawSSID();

  while (ConnectToInternetKeepIn) {
    delay(50);
    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();

      if (PressIt(DoneBtn) == true) {
        ConnectToInternetKeepIn = false;
      }
      if (PressIt(DownBtn) == true) {
        SiteToUse++;
        if (SiteToUse >= NumberOfSites) {
          SiteToUse = 0;
        }
        DrawSSID();
      }

      if (PressIt(UpBtn) == true) {

        if (SiteToUse == 0) {
          SiteToUse = NumberOfSites - 1;
        } else {
          SiteToUse--;
        }
        DrawSSID();
      }
      if (PressIt(RefreshBtn) == true) {
        DrawStatusBar(STATUS_NEW, 0);
        GetSites();
        DrawStatusBar(STATUS_DRAW, 100);
        delay(100);
        DrawStatusBar(STATUS_DONE, 0);
        SiteToUse = 0;
        DrawSSID();
      }

      if (PressIt(PasswordBtn) == true) {
        Display.fillScreen(C_BLACK);
        KeyboardInput.clearInput();
        KeyboardInput.getInput();
        DrawConnectToInternet();
        DrawConnectToInternetHeader();
        DrawSSID();
        ConnectBtn.enable();
        ConnectBtn.draw();
      }

      if (PressIt(ConnectBtn) == true) {
        ConnectToSelectedInternet();
        DrawSSID();
        DrawConnectToInternetHeader();
      }
    }
  }

  // got data

  // save all to the EEPROM

  if (WiFi.status() == WL_CONNECTED) {

    SSID = WiFi.SSID(SiteToUse).c_str();
    s = 0;
    //for (i = SSID_START; i < SSID_LENGTH; i++) {
    for (i = SSID_START; i < (SSID_START + WiFi.SSID(SiteToUse).length()); i++) {
      temp = (uint8_t)SSID.charAt(s);
      //Serial.print(temp);
      //Serial.print("-");
      EEPROM.put(i, temp);
      s++;
    }
    EEPROM.put(i, 255);  // this is considered an end terminator

    s = 0;
    PW = KeyboardInput.data;
    for (i = PW_START; i < (PW_START + PW.length()); i++) {
      temp = (uint8_t)PW[s];
      //Serial.print(temp);
      //Serial.print("-");
      EEPROM.put(i, temp);
      s++;
    }
    EEPROM.put(i, 255);  // this is considered an end terminator
  }
  EEPROM.commit();
  // Close the Preferences
  delay(100);
  GetParameters();

  delay(10);
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG ConnectToInternet()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw connect to the internet screen

*/

void DrawConnectToInternet() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawConnectToInternet()_____________________________________"));
#endif
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);
  DrawStatusBar(STATUS_NEW, 0);

  PasswordBtn.draw();
  ConnectBtn.draw();
  RefreshBtn.draw();
  UpBtn.draw();
  DownBtn.draw();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawConnectToInternet()_____________________________________"));
  Serial.println();
#endif
}

bool ConnectToMatchingSite() {
  i = 0;
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG ConnectToMatchingSite()_____________________________________"));
  Serial.println("starting server");
  Serial.println(SiteToUse);
  Serial.println(WiFi.SSID(SiteToUse).c_str());
  Serial.println(PW);
#endif

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 50 + i, 30, 4, C_GREY);
  Display.setCursor(30, 223);
  Display.print(F("Connecting "));

  WiFi.begin(WiFi.SSID(SiteToUse).c_str(), PW.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef SHOW_DEBUG
    Serial.print(".");
#endif
    i++;
    Display.fillRoundRect(20, 200, 50 + i, 30, 4, C_GREY);
    Display.setCursor(30, 223);
    Display.print(F("Connecting "));

    if (i >= 100) {

      return false;
    }
  }

  Actual_IP = WiFi.localIP();

#ifdef SHOW_DEBUG
  printWifiStatus();
#endif
  delay(100);
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG ConnectToMatchingSite()_____________________________________"));
  Serial.println();
#endif


  return true;
}

/*

function to let users connect to selected SSID and connect to the internet

*/
void ConnectToSelectedInternet() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG ConnectToSelectedInternet()_____________________________________"));
#endif

  i = 0;

  FoundInternet = false;
  DrawStatusBar(STATUS_NEW, 0);

#ifdef SHOW_DEBUG
  Serial.println("starting server");
  Serial.println(WiFi.SSID(SiteToUse).c_str());
  Serial.println(KeyboardInput.data);
#endif
  WiFi.begin(WiFi.SSID(SiteToUse).c_str(), KeyboardInput.data);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef SHOW_DEBUG
    Serial.print(".");
#endif
    DrawStatusBar(STATUS_DRAW, i += 2);
    if (i >= 100) {
      DrawStatusBar(STATUS_DONE, 0);
      return;
    }
  }

  FoundInternet = true;

  Actual_IP = WiFi.localIP();
  DrawStatusBar(STATUS_DRAW, 100);
#ifdef SHOW_DEBUG
  printWifiStatus();
#endif
  delay(100);
  DrawStatusBar(STATUS_DONE, 0);

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG ConnectToSelectedInternet()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw SSID 

*/
void DrawSSID() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawSSID()_____________________________________"));
#endif

  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setFont(&FONT_ITEM);
  Display.fillRect(10, 45, 320, 40, C_BLACK);

  if (NumberOfSites > 0) {
    Display.setTextColor(C_WHITE, C_BLACK);
    Display.setFont(&FONT_ITEM);

    Display.setCursor(60, 128);
    Display.print("Choose SSID");
    Display.setCursor(15, 75);
    if (WiFi.SSID(SiteToUse).length() > 13) {
      Display.print(WiFi.SSID(SiteToUse).substring(0, 13));
      Display.print("...");
    } else {
      Display.print(WiFi.SSID(SiteToUse));
    }
    Display.print(" :");
    Display.print(WiFi.RSSI(SiteToUse));
    ConnectBtn.enable();
    if (WiFi.encryptionType(SiteToUse) != WIFI_AUTH_OPEN) {
      drawBitmap(Display.getCursorX(), 57, Key, 20, 20, C_RED, C_BLACK);
      ConnectBtn.disable();
    } else {
      drawBitmap(Display.getCursorX(), 57, Unlock, 20, 20, C_GREEN, C_BLACK);
    }
    ConnectBtn.draw();

  } else {
    Display.setCursor(15, 75);
    Display.print("No sites found");
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawSSID()_____________________________________"));
  Serial.println();
#endif
}

/*

function to show list of races, selection made in a different function

*/
bool GetListOfRaces() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG GetListOfRaces()_____________________________________"));
#endif

  const char *sendString = "https://api.race-monitor.com/v2/Common/CurrentRaces?apiToken=9f383b38-e448-405c-b3ab-babd695414f6";

  bool HTTPSuccess = false;

  FoundRace = false;

  HTTPClient http;
  WiFiClientSecure client;

  http.setTimeout(HTTP_TIMEOUT);

  found = false;

  if (WiFi.status() == WL_CONNECTED) {

    DrawStatusBar(STATUS_DRAW, 10);
    HTTPSuccess = http.begin(sendString);
    if (!HTTPSuccess) {
#ifdef SHOW_DEBUG
      Serial.print("1257 HTTPSuccess FAIL: ");
      Serial.println(HTTPSuccess);
#endif
      http.end();
      return false;
    }

    HTTPCodeCurrentRace = http.POST(sendString);

    TotalServerPings++;

    DrawStatusBar(STATUS_DRAW, 20);
    //Serial.println("Line 1362 BEFORE CRASH");
    TotalServerPings++;

#ifdef SHOW_DEBUG

    Serial.print("HTTPSuccess: ");
    Serial.println(HTTPSuccess);

    Serial.print("HTTP to get current list of races: ");
    Serial.println(sendString);

    Serial.print("HTTP Response code: ");
    Serial.println(HTTPCodeCurrentRace);
#endif

    //Serial.println("Line 1372 BEFORE CRASH");

    String JSONCurrentRace = http.getString();
    //Serial.println("Line 1368 BEFORE CRASH");

    DrawStatusBar(STATUS_DRAW, 70);

    http.end();

#ifdef SHOW_DEBUG
    Serial.println(JSONCurrentRace);
#endif

    CurrentRaceError = deserializeJson(CurrentRaceDoc, JSONCurrentRace);
#ifdef SHOW_DEBUG
    Serial.println(F("deserializeJson() return code: "));
    Serial.println(CurrentRaceError.f_str());
#endif

    //Serial.println("Line 1385 BEFORE CRASH");

    NumberOfRaces = CurrentRaceDoc["Races"].size();
    DrawStatusBar(STATUS_DRAW, 80);

    if (NumberOfRaces == 0) {
      return false;
    }


    if ((CurrentRaceDoc["Successful"]) && (NumberOfRaces > 0)) {
      FoundRace = true;
    }

#ifdef SHOW_DEBUG
    const char *buffer = "                                    ";
    Serial.print("number of races ");
    Serial.println(NumberOfRaces);

    for (i = 0; i < NumberOfRaces; i++) {
      Serial.print("Data: ");
      Serial.print(i);
      Serial.print(", ");
      int32_t sID = CurrentRaceDoc["Races"][i]["ID"];
      Serial.print(sID);
      Serial.print(", ");
      int32_t sRaceTypeID = CurrentRaceDoc["Races"][i]["RaceTypeID"];
      Serial.print(sRaceTypeID);
      Serial.print(", ");
      buffer = CurrentRaceDoc["Races"][i]["SeriesName"];
      if (buffer != nullptr) {
        Serial.print(buffer);
        Serial.print(", ");
      }

      buffer = CurrentRaceDoc["Races"][i]["Track"];
      if (buffer != nullptr) {
        Serial.print(buffer);
        Serial.print(", ");
      }
      Serial.println();
    }
    //Serial.println("Line 1417 BEFORE CRASH");

#endif
    return true;
  }

  else {
    //Serial.println("no wifi connection");
    return false;
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG GetListOfRaces()_____________________________________"));
  Serial.println();
#endif
}

/*

function to build car list array (car number), and number of cars

*/
bool BuildCarList() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG BuildCarList()_____________________________________"));
#endif

  // example
  // const char *sendString = "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=37872";
  bool HTTPSuccess = false;
  char sendString[300];
  char numbuf[3];
  sprintf(sendString, "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=%lu", SelectedRaceID);
  const char *buffer = "                                    ";

  HTTPClient http;
  WiFiClientSecure client;

  http.setTimeout(HTTP_TIMEOUT);

  if (WiFi.status() == WL_CONNECTED) {

    HTTPSuccess = http.begin(sendString);

    if (!HTTPSuccess) {
#ifdef SHOW_DEBUG
      Serial.print("HTTPSuccess FAIL: ");
      Serial.println(HTTPSuccess);
#endif
      http.end();
      return false;
    }

    HTTPCodeRaceData = http.POST(sendString);
    TotalServerPings++;
    if (HTTPCodeRaceData == 400) {
      FoundInternet = false;
    }
#ifdef SHOW_DEBUG
    Serial.print("HTTPSuccess: ");
    Serial.println(HTTPSuccess);
    Serial.print("HTTP to get race data: ");
    Serial.println(sendString);
    Serial.print("HTTP Response code: ");
    Serial.println(HTTPCodeRaceData);
#endif

    JSONRaceData = http.getString();

    http.end();


#ifdef SHOW_DEBUG
    Serial.println(JSONRaceData);
#endif

    RaceDataError = deserializeJson(RaceDoc, JSONRaceData);

    NumberOfCars = RaceDoc["Session"]["Competitors"].size();

    if (NumberOfCars == 0) {
      FoundRace = false;
    }

#ifdef SHOW_DEBUG
    Serial.println(F("deserializeJson() return code: "));
    Serial.println(RaceDataError.f_str());
    Serial.print("Total cars in this race: ");
    Serial.println(NumberOfCars);
#endif

    if (NumberOfCars == 0) {
      return false;
    }

    i = 0;

    for (JsonPair kv : RaceDoc["Session"]["Competitors"].as<JsonObject>()) {

      buffer = kv.key().c_str();

      if (buffer != nullptr) {
        strcpy(numbuf, buffer);
        buffer = RaceDoc["Session"]["Competitors"][numbuf]["Number"];
        if (buffer != nullptr) {
          //strcpy(CarNumber[i], "\0");
          strcpy(CarNumber[i], buffer);
        } else {
          strcpy(CarNumber[i], "?");
        }
      } else {
        strcpy(CarNumber[i], "?");
      }

      i++;
    }

#ifdef SHOW_DEBUG
    for (i = 0; i < NumberOfCars; i++) {
      Serial.print(i);
      Serial.print(", ");
      Serial.print(CarNumber[i]);
      Serial.println();
    }
#endif
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG BuildCarList()_____________________________________"));
  Serial.println();
#endif

  return true;
}

/*

function to get race data once--mainly to get track name, track length
we could get this data each ping time when we get lap data, but chose to
get this data initially upon selection of a race

*/
void GetInitialRaceData() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG GetInitialRaceData()_____________________________________"));
#endif

  // example
  // const char *sendString = "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=37872";

  char sendString[300];
  bool HTTPSuccess = false;
  sprintf(sendString, "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=%lu", SelectedRaceID);
  const char *buffer = "                                    ";
  float TempTrackLength = 0.0f;

  HTTPClient http;
  WiFiClientSecure client;

  http.setTimeout(HTTP_TIMEOUT);

  if (WiFi.status() == WL_CONNECTED) {

    HTTPSuccess = http.begin(sendString);

    if (!HTTPSuccess) {
#ifdef SHOW_DEBUG
      Serial.print("1626 HTTPSuccess FAIL: ");
      Serial.println(HTTPSuccess);
#endif
      http.end();
      return;
    }

    HTTPCodeRaceData = http.POST(sendString);
    TotalServerPings++;
    if (HTTPCodeRaceData == 400) {
      FoundInternet = false;
    }
#ifdef SHOW_DEBUG
    Serial.print("HTTPSuccess: ");
    Serial.println(HTTPSuccess);
    Serial.print("HTTP to get race data: ");
    Serial.println(sendString);
    Serial.print("HTTP Response code: ");
    Serial.println(HTTPCodeRaceData);
#endif

    JSONRaceData = http.getString();
#ifdef SHOW_DEBUG
    Serial.println(JSONRaceData);
#endif

    http.end();
    RaceDataError = deserializeJson(RaceDoc, JSONRaceData);
    NumberOfCars = RaceDoc["Session"]["Competitors"].size();

    if (NumberOfCars == 0) {
      FoundRace = false;
    }

#ifdef SHOW_DEBUG
    Serial.println(F("deserializeJson() return code: "));
    Serial.println(RaceDataError.f_str());
    Serial.print("Total cars in this race: ");
    Serial.println(NumberOfCars);
#endif

    // get the session name
    buffer = RaceDoc["Session"]["SessionName"];

    if (buffer != nullptr) {
      strcpy(RDSessionName, buffer);
    } else {
      strcpy(RDSessionName, "Session Name ?");
    }
    // get the track name
    buffer = RaceDoc["Session"]["TrackName"];
    if (buffer != nullptr) {
      strcpy(RDTrackName, buffer);
    } else {
      strcpy(RDTrackName, "Track Name ?");
    }

    // get the track length
    buffer = RaceDoc["Session"]["TrackLength"];
    if (buffer != nullptr) {

      TempTrackLength = (float)atof(buffer);

    } else {
      TempTrackLength = 0.0f;
    }

    if (TempTrackLength > 0.01f) {
      TrackLength = TempTrackLength;
      EEPROM.put(1, TrackLength);
      delay(20);
      EEPROM.commit();
    }

    buffer = RaceDoc["Session"]["SessionTime"];
    if (buffer != nullptr) {
      strcpy(RDStartTime, buffer);
    } else {
      strcpy(RDStartTime, "StartTime ?");
    }

#ifdef SHOW_DEBUG
    Serial.print("SessionName: ");
    Serial.println(RDSessionName);
    Serial.print("Track Name: ");
    Serial.println(RDTrackName);
    Serial.print("Temp Track Length: ");
    Serial.println(TempTrackLength);
    Serial.print("Track Length: ");
    Serial.println(TrackLength);
    Serial.print("StartTime: ");
    Serial.println(RDStartTime);
#endif
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG GetInitialRaceData()_____________________________________"));
  Serial.println();
#endif
}

/*

function to ping race monitor and get car lap data
this is the heavy lifting function that gets live race data

*/
void GetCarLapTimes() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG GetCarLapTimes()_____________________________________"));
#endif

  int i = 0;
  bool HTTPSuccess = false;
  // example
  // const char *sendString = "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=37872";
  uint8_t TempLaps = 0;
  char sendString[300];
  sprintf(sendString, "https://api.race-monitor.com/v2/Live/GetSession?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=%lu", SelectedRaceID);
  const char *buffer = "                                        ";

  HTTPClient http;
  WiFiClientSecure client;

  http.setTimeout(HTTP_TIMEOUT);

  if (WiFi.status() == WL_CONNECTED) {

    HTTPSuccess = http.begin(sendString);

    if (!HTTPSuccess) {
#ifdef SHOW_DEBUG
      Serial.print("HTTPSuccess FAIL: ");
      Serial.println(HTTPSuccess);
#endif
      http.end();
      return;
    }

    HTTPCodeRaceData = http.POST(sendString);
    TotalServerPings++;
    if (HTTPCodeRaceData == 400) {
      FoundInternet = false;
    }
#ifdef SHOW_DEBUG
    Serial.print("HTTPSuccess: ");
    Serial.println(HTTPSuccess);
    Serial.print("HTTP to get race data: ");
    Serial.println(sendString);
    Serial.print("HTTP Response code: ");
    Serial.println(HTTPCodeRaceData);
#endif

    JSONRaceData = http.getString();

    http.end();
  } else {
    return;
  }

#ifdef SHOW_DEBUG
  Serial.println();
  Serial.println(JSONRaceData);
  Serial.println();
#endif

  RaceDataError = deserializeJson(RaceDoc, JSONRaceData);
  FoundRace = RaceDoc["Successful"];

#ifdef SHOW_DEBUG
  Serial.print(" FoundRace: ");
  Serial.println(FoundRace);
#endif

  if (RaceDoc["Successful"] == "true") {

    FoundRace = true;
  }

  buffer = RaceDoc["Session"]["SessionTime"];
  if (buffer != nullptr) {
    strcpy(CurrentRaceTime, buffer);
  } else {
    strcpy(CurrentRaceTime, "CurrentRaceTime ?");
  }

#ifdef SHOW_DEBUG
  Serial.println(F("deserializeJson() return code: "));
  Serial.println(RaceDataError.f_str());
  Serial.println(F("SessionTime: "));
  Serial.println(CurrentRaceTime);
#endif

  for (i = 0; i < NumberOfCars; i++) {

    buffer = RaceDoc["Session"]["Competitors"][CarNumber[i]]["Laps"];

    if (buffer != nullptr) {
      TempLaps = atoi(buffer);

      if (TempLaps > RaceMaxLaps) {
        RaceMaxLaps = TempLaps;
      }
    } else {
      TempLaps = 0;
    }

    if (TempLaps != NewLap[i]) {
      // now we have a new lap, so cache the lap and get the time
      OldLap[i] = NewLap[i];
      NewLap[i] = TempLaps;


      // now get lap time
      buffer = RaceDoc["Session"]["Competitors"][CarNumber[i]]["LastLapTime"];

      if (buffer != nullptr) {
        h = (((buffer[0] - 48) * 10) + (buffer[1] - 48));
        m = (((buffer[3] - 48) * 10) + (buffer[4] - 48));
        s = (((buffer[6] - 48) * 10) + (buffer[7] - 48));

        CarLapTime[i][TempLaps] = ((h * 3600) + (m * 60) + s);
#ifdef SHOW_DEBUG
        Serial.print("Index=");
        Serial.print(i);
        Serial.print(", number=");
        Serial.print(CarNumber[i]);
        Serial.print(", lap=");
        Serial.print(TempLaps);
        Serial.print(", Time=");
        Serial.println(CarLapTime[i][TempLaps]);
#endif

      } else {
        CarLapTime[i][TempLaps] = 0;
      }
    }
    //Serial.println();
  }



#ifdef SHOW_DEBUG
  int Cars = 0, Laps = 0;
  float CarSpeed = 0.0f;

  Serial.println("print header----------------------------------------------------------------------- ");
  // print header
  Serial.print("Lap ");
  for (Cars = 0; Cars < NumberOfCars; Cars++) {
    sprintf(buf, "Car %3s", CarNumber[Cars]);
    Serial.print(buf);
  }
  Serial.println();
  for (Laps = 0; Laps <= RaceMaxLaps; Laps++) {
    Serial.print(Laps);
    Serial.print("  ");
    for (Cars = 0; Cars < NumberOfCars; Cars++) {
      Serial.print("    ");
      CarSpeed = ComputeMPH(CarLapTime[Cars][Laps]);
      if (CarSpeed > 0) {
        Serial.print(CarSpeed, 1);
      } else {
        Serial.print("00.0");
      }
    }
    Serial.println();
  }
#endif

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG GetCarLapTimes()_____________________________________"));
  Serial.println();
#endif
}


/*

function to automatically connect to the internet based on the last SSID and password
stored in the EEPROM
it's used to keep GPUSA from logging into their Verison hot spot net every time

*/

bool FindMatchingSite() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG FindMatchingSite()_____________________________________"));

#endif
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
#ifdef SHOW_DEBUG
  Serial.println("scan start");
#endif

  // WiFi.scanNetworks will return the number of networks found
  NumberOfSites = WiFi.scanNetworks();
#ifdef SHOW_DEBUG
  Serial.println("scan done");
#endif
  if (NumberOfSites == 0) {
#ifdef SHOW_DEBUG
    Serial.println("no networks found");
#endif
    return false;
  } else {
#ifdef SHOW_DEBUG
    Serial.print(NumberOfSites);
    Serial.println(" networks found");
#endif

    for (int CurrentSite = 0; CurrentSite < NumberOfSites; ++CurrentSite) {
#ifdef SHOW_DEBUG
      Serial.print(SSID);
      Serial.print("-");
      Serial.println(WiFi.SSID(CurrentSite));
#endif
      if (SSID == WiFi.SSID(CurrentSite)) {
#ifdef SHOW_DEBUG
        Serial.print("Using site: ");
        Serial.print(CurrentSite);
        Serial.print("-");
        Serial.println(WiFi.SSID(CurrentSite));
#endif
        SiteToUse = CurrentSite;
        return true;
      }
      delay(50);
    }
  }

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG FindMatchingSite()_____________________________________"));
  Serial.println();
#endif

  return false;
}

/*

function to get all broadcasted SSID's selection and connection done in a different function

*/
void GetSites() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG GetSites()_____________________________________"));
#endif


  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
#ifdef SHOW_DEBUG
  Serial.println("scan start");
#endif
  DrawStatusBar(STATUS_DRAW, 2);
  // WiFi.scanNetworks will return the number of networks found
  NumberOfSites = WiFi.scanNetworks();
#ifdef SHOW_DEBUG
  Serial.println("scan done");
#endif
  if (NumberOfSites == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(NumberOfSites);
    Serial.println(" networks found");
    for (int CurrentSite = 0; CurrentSite < NumberOfSites; ++CurrentSite) {
      DrawStatusBar(STATUS_DRAW, ((CurrentSite * 100) / NumberOfSites));
// Print SSID and RSSI for each network found
#ifdef SHOW_DEBUG
      Serial.print(CurrentSite + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(CurrentSite));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(CurrentSite));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(CurrentSite) == WIFI_AUTH_OPEN) ? " " : "*");
#endif
      delay(50);
    }
  }

  // Wait a bit before returning
  delay(100);

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG GetSites()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw status indicator bar 

*/

void DrawStatusBar(uint8_t FirstDraw, int StatusPercent) {
  uint8_t w = 0;
  if (FirstDraw == STATUS_NEW) {
    Display.fillRoundRect(140, 200, 160, 40, 4, C_DKGREY);
  } else if (FirstDraw == STATUS_DRAW) {
    // compute width
    // draw status bar
    w = (StatusPercent * 160) / 100;
    if (w <= 8) {
      w = 8;
    }

    Display.fillRoundRect(140, 200, w, 40, 4, C_GREEN);

  } else {
    Display.fillRoundRect(140, 200, 160, 40, 4, C_DKGREY);
  }
}

/*

function to zero out data arrays

*/
void RebuildArrays() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG RebuildArrays()_____________________________________"));
#endif
  int j, i;
  for (i = 0; i < MAX_CARS; i++) {

    strcpy(CarNumber[i], "");
    NewLap[i] = 0;
    OldLap[i] = 0;
    for (j = 0; j < MAX_LAPS; j++) {
      CarLapTime[i][j] = 0;
    }
    CarStatus[i] = NO_VIOLATION;
    CarIsPrinted[i] = false;
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG RebuildArrays()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw header 

*/
void DrawConnectToInternetHeader() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawConnectToInternetHeader()_____________________________________"));
#endif

  Display.setFont(&FONT_HEADING);

  if (WiFi.status() != WL_CONNECTED) {
    Display.fillRect(0, 0, 320, 40, C_LTGREY);
    Display.setCursor(10, 30);
    ConnectStatus.setTextColor(C_MDRED, C_LTGREY);
    ConnectStatus.print("No Internet");
  } else {
    Display.fillRect(0, 0, 320, 40, C_LTGREY);
    Display.setCursor(10, 30);
    ConnectStatus.setTextColor(C_DKGREEN, C_LTGREY);
    ConnectStatus.print("Connected");
  }

  DoneBtn.draw();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawConnectToInternetHeader()_____________________________________"));
  Serial.println();
#endif
}

/*

function to print wifi status--debug tool

*/
void printWifiStatus() {

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("Open http://");
  Serial.println(ip);
}

/*

function to let user change settings like track distance and ping times

*/

void SettingsScreen() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG SettingsScreen()_____________________________________"));
#endif
  bool SettingsKeepIn = true;

  DrawSettingsScreen();

  while (SettingsKeepIn) {
    delay(50);

    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();
      RaceMonitorRefresh.press(BtnX, BtnY);
      if (PressIt(DoneBtn) == true) {
        SettingsKeepIn = false;
      }

      if (PressIt(TrackLengthBtn) == true) {
        GetTrackLength.value = TrackLength;
        GetTrackLength.getInput();
        TrackLength = GetTrackLength.value;
        MaxLapTime = (TrackLength * 3600.0f) / (15.0f);

        DrawSettingsScreen();
      }
    }
  }

  RefreshRateID = (uint8_t)RaceMonitorRefresh.option;

  GetRefreshRate();
#ifdef SHOW_DEBUG
  Serial.print(" RefreshRateID: ");
  Serial.println(RefreshRateID);
  Serial.print(" TrackLength: ");
  Serial.println(TrackLength);
#endif
  EEPROM.put(0, RefreshRateID);
  EEPROM.put(1, TrackLength);
  delay(20);
  EEPROM.commit();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG SettingsScreen()_____________________________________"));
  Serial.println();
#endif
}

/*

function to find ping time

*/
void GetRefreshRate() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG GetRefreshRate()_____________________________________"));
#endif
  if (RefreshRateID == 0) {
    RefreshRate = 15;
  } else if (RefreshRateID == 1) {
    RefreshRate = 30;
  } else if (RefreshRateID == 2) {
    RefreshRate = 60;
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG GetRefreshRate()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw settings screen

*/

void DrawSettingsScreen() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG DrawSettingsScreen()_____________________________________"));
#endif
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);
  Display.setFont(&FONT_HEADING);
  Display.setCursor(10, 30);
  Display.fillRect(0, 0, 320, 40, C_LTGREY);
  Display.setTextColor(C_BLACK, C_LTGREY);
  Display.print("Settings");

  Display.fillRect(200, 40, 110, 200, C_BLACK);

  Display.setFont(&FONT_ITEM);
  Display.setCursor(10, 75);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Server ping time [s]");

  Display.setCursor(10, 178);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Track length [mi]: ");

  RaceMonitorRefresh.draw(RefreshRateID);
  sprintf(buffer, "%0.3f", TrackLength);
  TrackLengthBtn.setText(buffer);
  TrackLengthBtn.draw();
  DoneBtn.draw();

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG DrawSettingsScreen()_____________________________________"));
  Serial.println();
#endif
}

/*

function to get parameters from EEPROM

new chips you will need to force putting data initially

*/

void GetParameters() {

  uint8_t temp = 0;

  EEPROM.get(0, RefreshRateID);
  EEPROM.get(1, TrackLength);
  EEPROM.get(5, SelectedRaceID);

  MaxLapTime = (TrackLength * 3600.0f) / (15.0f);

  GetRefreshRate();

  SSID = "";

  for (i = SSID_START; i < (SSID_START + SSID_LENGTH); i++) {
    EEPROM.get(i, temp);
    if (temp == 255) {
      break;
    }
    SSID = SSID + (char)temp;
  }

  PW = "";

  for (i = PW_START; i < (PW_START + PW_LENGTH); i++) {
    EEPROM.get(i, temp);
    if (temp == 255) {
      break;
    }
    PW = PW + (char)temp;
  }

#ifdef SHOW_DEBUG

  Serial.println(F("DEBUG GetParameters()_____________________________________"));

  Serial.print("RefreshRateID ");
  Serial.println(RefreshRateID);

  Serial.print("RefreshRate ");
  Serial.println(RefreshRate);

  Serial.print("Track Length ");
  Serial.println(TrackLength);

  Serial.print("Selected RaceID ");
  Serial.println(SelectedRaceID);

  Serial.print("getting SSID from EEPROM ");
  Serial.println(SSID);

  Serial.print("getting password from EEPROM ");
  Serial.println(PW);

  Serial.println(F("END DEBUG GetParameters()_____________________________________"));
  Serial.println();

#endif
}

/*

function to draw and let users interact with a main menu

*/
void MainMenu() {
#ifdef SHOW_DEBUG

  Serial.println(F("DEBUG MainMenu()_____________________________________"));
#endif
  // UI to draw screen and capture input
  bool MainMenuKeepIn = true;

  DrawMainMenu();

  while (MainMenuKeepIn) {
    delay(50);
    // if touch screen pressed handle it
    if (Touch.touched()) {

      ProcessTouch();

      if (PressIt(MonitorBtn) == true) {
        MainMenuKeepIn = false;
      }
      if (PressIt(DownloadBtn) == true) {
        DownloadLapTimes();
        DrawMainMenu();
      }
      if (PressIt(InternetConnectBtn) == true) {
        ConnectToInternet();
        DrawMainMenu();
      }
      if (PressIt(SettingsBtn) == true) {
        SettingsScreen();
        DrawMainMenu();
      }

      if (PressIt(SetRaceBtn) == true) {
        SelectRace();
        DrawMainMenu();
      }
    }
  }
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG MainMenu()_____________________________________"));
  Serial.println();
#endif
}

/*

function to draw fixed text in main menu

*/
void DrawMainMenu() {
#ifdef SHOW_DEBUG
  Serial.println(F("DrawMainMenu MainMenu()_____________________________________"));
#endif
  //nothing fancy, just a header and some buttons
  Display.fillScreen(C_BLACK);

  Display.setFont(&FONT_HEADING);
  Display.setCursor(10, 30);
  Display.fillRect(0, 0, 320, 40, C_LTGREY);
  Display.setTextColor(C_BLACK, C_LTGREY);
  Display.print("Main Menu");

  MonitorBtn.disable();
  SetRaceBtn.disable();
  DownloadBtn.disable();

  if (WiFi.status() == WL_CONNECTED) {
    SetRaceBtn.enable();
  }
  if ((FoundInternet) && (FoundRace)) {
    MonitorBtn.enable();
  }

  if (RaceMaxLaps > 0) {
    DownloadBtn.enable();
  }
  if (FoundInternet) {
    InternetConnectBtn.setColors(DISABLE_COLOR, C_GREEN, C_BLACK, C_BLACK, C_LTGREY, C_DKGREY);
  } else {
    InternetConnectBtn.setColors(DISABLE_COLOR, C_MDRED, C_BLACK, C_BLACK, C_LTGREY, C_DKGREY);
  }
  if (FoundRace) {
    SetRaceBtn.setColors(DISABLE_COLOR, C_GREEN, C_BLACK, C_BLACK, C_LTGREY, C_DKGREY);
  } else {
    SetRaceBtn.setColors(DISABLE_COLOR, C_MDRED, C_BLACK, C_BLACK, C_LTGREY, C_DKGREY);
  }

  MonitorBtn.draw();
  DownloadBtn.draw();
  InternetConnectBtn.draw();
  SettingsBtn.draw();
  SetRaceBtn.draw();

#ifdef SHOW_DEBUG
  Serial.println(F("END DrawMainMenu MainMenu()_____________________________________"));
  Serial.println();
#endif
}

/*

function to dump lap data from memory to and SD card
oped to do this once after a race as opposed to writing at each lap
mainly due to complexity of knowing when a race is still on (could use filename as race name)
but keeping it simple

*/
void DownloadLapTimes() {

#ifdef SHOW_DEBUG
  Serial.println(F("DownloadLapTimes MainMenu()_____________________________________"));
#endif

  int Cars = 0, Laps = 0;
  int next = 0;
  bool SDCardStatus = false;
  float CarSpeed = 0.0f;

  SdFat SDCARD;
  SdFile SDDataFile;

  SDCardStatus = SDCARD.begin(PIN_SDCS, SD_SCK_MHZ(SD_SPI_SPEED));  //SD

  if (!SDCardStatus) {
    ShowSDStatus(0);
    return;
  }

  FileName[6] = (int)((next / 100) % 10) + '0';
  FileName[7] = (int)((next / 10) % 10) + '0';
  FileName[8] = (int)(next % 10) + '0';

  while (SDCARD.exists(FileName)) {
    next++;

    FileName[6] = (int)((next / 100) % 10) + '0';
    FileName[7] = (int)((next / 10) % 10) + '0';
    FileName[8] = (int)(next % 10) + '0';

    if (next > 999) {
      ShowSDStatus(1);
      break;
    }
  }

  SDCardStatus = SDDataFile.open(FileName, O_WRITE | O_CREAT);

  if (!SDCardStatus) {
    return;
  }
  if (NumberOfCars == 0) {
    ShowSDStatus(3);  // no race data
  }

  if (RaceMaxLaps == 0) {
    ShowSDStatus(4);  // no race data
  }

  // print information
  SDDataFile.print(F("Series Name, "));
  SDDataFile.println(CRDSeriesName);
  SDDataFile.print(F("Race Name, "));
  SDDataFile.println(CRDName);
  SDDataFile.print(F("Track, "));
  SDDataFile.println(CRDTrack);
  SDDataFile.print(F("Race ID, "));
  SDDataFile.println(SelectedRaceID);
  SDDataFile.print(F("Session Name, "));
  SDDataFile.println(RDSessionName);
  SDDataFile.print(F("Track Name, "));
  SDDataFile.println(RDTrackName);
  SDDataFile.print(F("Start Time, "));
  SDDataFile.println(RDStartTime);
  SDDataFile.print(F("Track length [mi], "));
  SDDataFile.println(TrackLength);
  SDDataFile.print(F("Total cars, "));
  SDDataFile.println(NumberOfCars);
  SDDataFile.print(F("Server Pings, "));
  SDDataFile.println(TotalServerPings);

  // print header
  SDDataFile.print("Lap vs. Car Speed [MPH]");
  for (Cars = 0; Cars < NumberOfCars; Cars++) {
    SDDataFile.print(", Car: ");
    SDDataFile.print(CarNumber[Cars]);
  }
  SDDataFile.println();

  // print car number and lap car speed for each car. lap speeds are in MPH
  for (Laps = 0; Laps < RaceMaxLaps; Laps++) {
    SDDataFile.print(Laps);
    for (Cars = 0; Cars < NumberOfCars; Cars++) {
      SDDataFile.print(",");

      CarSpeed = ComputeMPH(CarLapTime[Cars][Laps]);

      // print nothing if speed = 0, that way Excel can ignore the val and connect adjacent points
      // 0 due to missing data from the server
      if (CarSpeed > 0) {
        SDDataFile.print(CarSpeed, 2);
      }
    }

    SDDataFile.println();
  }


  SDDataFile.close();

  ShowSDStatus(2);

#ifdef SHOW_DEBUG
  Serial.println(F("END DownloadLapTimes MainMenu()_____________________________________"));
  Serial.println();
#endif
}

/*

function to show status of SD card

*/
void ShowSDStatus(uint8_t Status) {
  bool KeepIN = true;
  Display.setFont(&FONT_DATA);
  Display.setTextColor(C_BLACK);
  Display.fillRect(52 - 2, 53 - 2, 208 + 4, 119 + 4, C_BLACK);

  if (Status == 0) {
    Display.fillRect(52, 53, 208, 119, C_MDRED);
    Display.setCursor(129, 71);
    Display.print(F("ERROR"));
    Display.setCursor(105, 95);
    Display.print(F("Missing or bad"));
    Display.setCursor(126, 115);
    Display.print(F("SD Card"));
  } else if (Status == 1) {
    Display.fillRect(52, 53, 208, 119, C_MDRED);
    Display.setCursor(129, 71);
    Display.print(F("ERROR"));
    Display.setCursor(115, 95);
    Display.print(F("Full or bad"));
    Display.setCursor(126, 115);
    Display.print(F("SD Card"));
  } else if (Status == 2) {
    Display.fillRect(52, 53, 208, 119, C_DKGREY);
    Display.setTextColor(C_WHITE);
    Display.setCursor(123, 71);
    Display.print(F("SUCCESS"));
    Display.setCursor(96, 95);
    Display.print(F("Data written to:"));
    Display.setCursor(90, 115);
    Display.print(FileName);
  } else if (Status == 3) {
    Display.fillRect(52, 53, 208, 119, C_MDRED);
    Display.setCursor(129, 71);
    Display.print(F("ERROR"));
    Display.setCursor(105, 95);
    Display.print(F("No race data"));
  } else if (Status == 4) {
    Display.fillRect(52, 53, 208, 119, C_MDRED);
    Display.setCursor(129, 71);
    Display.print(F("ERROR"));
    Display.setCursor(112, 95);
    Display.print(F("No lap data"));
  }

  SDStatusBTN.draw();

  while (KeepIN) {
    ProcessTouch();

    if (PressIt(SDStatusBTN) == true) {
      KeepIN = false;
    }
  }
}

/*

service function to handle button presses

*/
bool PressIt(Button TheButton) {

  if (TheButton.press(BtnX, BtnY)) {
    TheButton.draw(B_PRESSED);

    Click();

    while (Touch.touched()) {
      delay(50);
      if (TheButton.press(BtnX, BtnY)) {
        TheButton.draw(B_PRESSED);
      } else {
        TheButton.draw(B_RELEASED);
        return false;
      }
      ProcessTouch();
    }

    TheButton.draw(B_RELEASED);
    return true;
  }
  return false;
}

/*

service function to handle screen presses and convert press location to screen coordinate

*/
void ProcessTouch() {

  BtnX = -1;
  BtnY = -1;

  if (Touch.touched()) {

    TP = Touch.getPoint();
    BtnX = TP.x;
    BtnY = TP.y;
    BtnZ = TP.z;
    /*
#ifdef SHOW_DEBUG
    Serial.print("Touched coordinates : ");
    Serial.print(BtnX);
    Serial.print(", ");
    Serial.println(BtnY);
    Serial.print(", ");
    Serial.println(BtnZ);
#endif
*/
    //yellow headers
    BtnX = map(BtnX, ScreenLeft, ScreenRight, 320, 0);
    BtnY = map(BtnY, ScreenTop, ScreenBottom, 240, 0);

    // black headers
    // BtnX  = map(BtnX, 0, 3905, 320, 0);
    // BtnY  = map(BtnY, 0, 3970, 240, 0);
    // Display.fillCircle(BtnX, BtnY, 2, C_GREEN);
    /*
#ifdef SHOW_DEBUG
    //Serial.print("Mapped coordinates : ");
    //Serial.print(BtnX);
    //Serial.print(", ");
    //Serial.print(BtnY);
    //Serial.print(", ");
    //Serial.println(BtnZ);
#endif
*/
  }
}
/*

service function to init objects like buttons and graphs

*/
void CreateInterface() {

#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG CreateInterface MainMenu()_____________________________________"));
#endif

  // keeps button text from wrapping
  Display.setTextWrap(false);

  MonitorBtn.init(83, 92, 150, 91, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Begin", 0, 0, &FONT_BUTTON);
  DownloadBtn.init(83, 189, 148, 91, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Download", 0, 0, &FONT_BUTTON);
  InternetConnectBtn.init(241, 77, 146, 59, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Internet", 0, 0, &FONT_BUTTON);
  SetRaceBtn.init(241, 141, 146, 59, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Set Race", 0, 0, &FONT_BUTTON);
  SettingsBtn.init(241, 205, 146, 59, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Settings", 0, 0, &FONT_BUTTON);
  DoneBtn.init(260, 20, 100, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Done", 0, 0, &FONT_BUTTON);
  TrackLengthBtn.init(260, 170, 100, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Set", 0, 0, &FONT_BUTTON);
  RefreshBtn.init(70, 220, 120, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Refresh", 0, 0, &FONT_BUTTON);
  UpBtn.init(30, 120, 40, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "<", 0, 0, &FONT_BUTTON);
  DownBtn.init(280, 120, 40, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, ">", 0, 0, &FONT_BUTTON);
  RemoveBtn.init(75, 220, 100, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Remove", 0, 0, &FONT_BUTTON);
  SilenceBtn.init(245, 220, 100, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Silence", 0, 0, &FONT_BUTTON);
  SDStatusBTN.init(157, 145, 67, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "OK", 0, 0, &FONT_BUTTON);
  MenuBtn.init(255, 20, 120, 30, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Menu", 0, 0, &FONT_BUTTON);
  RaceIDBtn.init(70, 170, 120, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Race ID", 0, 0, &FONT_BUTTON);
  PasswordBtn.init(70, 170, 120, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Password", 0, 0, &FONT_BUTTON);
  ConnectBtn.init(220, 170, 160, 40, DISABLE_COLOR, C_GREY, C_BLACK, C_BLACK, "Connect", 0, 0, &FONT_BUTTON);

  // option buttons
  RaceMonitorRefresh.init(C_GREY, C_GREEN, C_DKGREY, C_WHITE, C_BLACK, 0, 0, &FONT_BUTTON);

  // and text
  RefreshID0 = RaceMonitorRefresh.add(140, 95, "15", 15);
  RefreshID1 = RaceMonitorRefresh.add(200, 95, "30", 30);
  RefreshID2 = RaceMonitorRefresh.add(260, 95, "60", 60);

  RaceMonitorRefresh.setClickPin(PIN_TONE);

  // keypad copntrols
  KeyboardInput.init(C_DKGREY, C_BLACK, C_LTGREY, C_LTGREY, DISABLE_COLOR, &FONT_BUTTON);
  KeyboardInput.setTouchLimits(ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
  KeyboardInput.setClickPin(PIN_TONE);
  GetTrackLength.init(C_DKGREY, C_BLACK, C_LTGREY, C_LTGREY, DISABLE_COLOR, &FONT_BUTTON);
  GetTrackLength.setTouchLimits(ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
  GetTrackLength.enableDecimal(true);
  GetTrackLength.enableNegative(false);
  GetTrackLength.setDecimals(3);
  GetTrackLength.setClickPin(PIN_TONE);

  // initialize the graph object
  Violator.init("Speed issue", "Lap", "Speed", C_WHITE, C_DKGREY, C_WHITE, C_BLACK, C_BLACK, FONT_DATA, FONT_DATA);

  // use the add method to create a plot for each data
  // PlotID = MyGraph.Add(data title, data color);
  //
  SpeedID = Violator.add("Speed", C_YELLOW);

  // optional
  // Violator.setMarkerSize(SpeedID, 4);
  Violator.setLineThickness(SpeedID, 2);
  Violator.showAxisLabels(false);
  Violator.showTitle(false);
  Violator.showLegend(false);

  // main screen

  MonitorBtn.setCornerRadius(C_RADIUS);
  DownloadBtn.setCornerRadius(C_RADIUS);
  InternetConnectBtn.setCornerRadius(C_RADIUS);
  SetRaceBtn.setCornerRadius(C_RADIUS);
  SettingsBtn.setCornerRadius(C_RADIUS);
  DoneBtn.setCornerRadius(C_RADIUS);
  TrackLengthBtn.setCornerRadius(C_RADIUS);
  RefreshBtn.setCornerRadius(C_RADIUS);
  UpBtn.setCornerRadius(C_RADIUS);
  DownBtn.setCornerRadius(C_RADIUS);
  RemoveBtn.setCornerRadius(C_RADIUS);
  SilenceBtn.setCornerRadius(C_RADIUS);
  SDStatusBTN.setCornerRadius(C_RADIUS);
  MenuBtn.setCornerRadius(C_RADIUS);
  RaceIDBtn.setCornerRadius(C_RADIUS);
  ConnectBtn.setCornerRadius(C_RADIUS);
  PasswordBtn.setCornerRadius(C_RADIUS);

  MonitorBtn.setBorderThickness(BORDER_THICKNESS);
  DownloadBtn.setBorderThickness(BORDER_THICKNESS);
  InternetConnectBtn.setBorderThickness(BORDER_THICKNESS);
  SetRaceBtn.setBorderThickness(BORDER_THICKNESS);
  SettingsBtn.setBorderThickness(BORDER_THICKNESS);
  DoneBtn.setBorderThickness(BORDER_THICKNESS);
  TrackLengthBtn.setBorderThickness(BORDER_THICKNESS);
  RefreshBtn.setBorderThickness(BORDER_THICKNESS);
  UpBtn.setBorderThickness(BORDER_THICKNESS);
  DownBtn.setBorderThickness(BORDER_THICKNESS);
  RemoveBtn.setBorderThickness(BORDER_THICKNESS);
  SilenceBtn.setBorderThickness(BORDER_THICKNESS);
  SDStatusBTN.setBorderThickness(BORDER_THICKNESS);
  MenuBtn.setBorderThickness(BORDER_THICKNESS);
  RaceIDBtn.setBorderThickness(BORDER_THICKNESS);
  ConnectBtn.setBorderThickness(BORDER_THICKNESS);
  PasswordBtn.setBorderThickness(BORDER_THICKNESS);

#ifdef SHOW_DEBUG
  Serial.println(F("CreateInterface MainMenu()_____________________________________"));
#endif
}

/*

service function mono icons
used to draw SSID status icons

*/
void drawBitmap(int16_t x, int16_t y, const unsigned char *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t BackColor) {

  int16_t ByteWidth = (w + 7) / 8, i, j;
  uint8_t sByte = 0;
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (i & 7) {
        sByte <<= 1;
      } else {
        sByte = pgm_read_byte(bitmap + j * ByteWidth + i / 8);
      }
      if (sByte & 0x80) {
        Display.drawPixel(x + i, y + j, color);
      } else {
        Display.drawPixel(x + i, y + j, BackColor);
      }
    }
  }
}

/*

service function to write STRING data to EEPROM
not used

*/
void SaveToEEPROM(uint16_t StartingByte, char *data) {
  EEPROM.writeString(StartingByte, data);
  EEPROM.commit();
}
/*

service function to read STRING data to EEPROM
not used

*/
void ReadEEPROM(uint16_t StartingByte, char *data) {
  String readData = EEPROM.readString(StartingByte);
  readData.toCharArray(data, readData.length() + 1);
}
/*

service function to draw GPUSA logo

*/
void draw565Bitmap(int16_t x, int16_t y, const uint16_t *bitmap, uint16_t w, uint16_t h) {

  uint16_t offset = 0;

  int j, i;

  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      Display.drawPixel(j + x, i + y, bitmap[offset]);
      offset++;
    }
  }
}

/*

service function to make click sound when button is pressed

*/

void Click() {

  analogWrite(PIN_TONE, 60);
  delay(5);
  analogWrite(PIN_TONE, 0);
}


/*

debug purposes only, a simple way to run throuhg an array of preset times and cars

*/

void SimulateRace() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG SimulateRace()_____________________________________"));
#endif
  uint8_t i = 0;
  uint8_t TempLap = 0;
  int32_t TotalTime = 0;
  bool bump = false;

  for (i = 0; i < NumberOfCars; i++) {

    Serial.print("OldLap: ");
    Serial.print(OldLap[i]);
    Serial.print(", NewLap: ");
    Serial.print(NewLap[i]);

    TempLap = OldLap[i];
    bump = false;
    OldLap[i] = NewLap[i];

    TotalTime = CarLapTime[i][NewLap[i]];
    Serial.print(", TotalTime: ");
    Serial.print(TotalTime);
    Serial.print(", target: ");
    Serial.print((SimulationIteration - OldLap[i]) * RefreshRate);

    while (TotalTime <= ((SimulationIteration - OldLap[i]) * RefreshRate)) {

      //Serial.print("614 TotalTime: ");
      //Serial.print(TotalTime);
      //Serial.print(", TempLap: ");
      //Serial.println(TempLap);

      TotalTime += CarLapTime[i][TempLap];
      TempLap++;
      bump = true;
    }

    if (bump) {
      NewLap[i] = TempLap + 1;
      bump = false;
    }


    Serial.print(", Car: ");
    Serial.print(CarNumber[i]);
    Serial.print(", MaxLapTime: ");
    Serial.print(MaxLapTime);
    Serial.print(", refresh rate: ");
    Serial.print(RefreshRate);
    Serial.print(", TempLap: ");
    Serial.print(TempLap);
    Serial.print(", TotalTime: ");
    Serial.print(TotalTime);
    Serial.print(", OldLap: ");
    Serial.print(OldLap[i]);
    Serial.print(", NewLap: ");
    Serial.println(NewLap[i]);
  }
  Serial.println();
#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG SimulateRace()_____________________________________"));
  Serial.println();
#endif
}

/*

DEBUG function to create a race for testing

*/
void CreateTestData() {
#ifdef SHOW_DEBUG
  Serial.println(F("DEBUG CreateTestData MainMenu()_____________________________________"));
#endif
  // car 0
  strcpy(CarNumber[0], "000");
  CarLapTime[0][0] = 200;
  CarLapTime[0][1] = 205;
  CarLapTime[0][2] = 218;
  CarLapTime[0][3] = 215;
  CarLapTime[0][4] = 215;
  CarLapTime[0][5] = 218;
  CarLapTime[0][6] = 220;
  CarLapTime[0][7] = 225;
  CarLapTime[0][8] = 239;
  CarLapTime[0][9] = 240;
  CarLapTime[0][10] = 240;
  CarLapTime[0][11] = 270;
  CarLapTime[0][12] = 280;
  CarLapTime[0][13] = 300;

  // car 1
  strcpy(CarNumber[1], "001");
  CarLapTime[1][0] = 60;
  CarLapTime[1][1] = 60;
  CarLapTime[1][2] = 60;
  CarLapTime[1][3] = 120;
  CarLapTime[1][4] = 120;
  CarLapTime[1][5] = 150;
  CarLapTime[1][6] = 200;
  CarLapTime[1][7] = 250;
  CarLapTime[1][8] = 290;
  CarLapTime[1][9] = 300;
  CarLapTime[1][10] = 300;
  CarLapTime[1][11] = 300;
  CarLapTime[1][12] = 300;

  //CarLap[1] = 13;

  // car 2
  strcpy(CarNumber[2], "002");
  CarLapTime[2][0] = 195;
  CarLapTime[2][1] = 195;
  CarLapTime[2][2] = 230;
  CarLapTime[2][3] = 170;
  CarLapTime[2][4] = 165;
  CarLapTime[2][5] = 205;
  CarLapTime[2][6] = 210;
  CarLapTime[2][7] = 215;
  CarLapTime[2][8] = 220;
  CarLapTime[2][9] = 230;
  CarLapTime[2][10] = 240;
  CarLapTime[2][11] = 280;
  CarLapTime[2][12] = 320;

  //CarLap[2] = 13;

  strcpy(CarNumber[3], "003");
  CarLapTime[3][0] = 20;
  CarLapTime[3][1] = 40;
  CarLapTime[3][2] = 30;
  CarLapTime[3][3] = 40;
  CarLapTime[3][4] = 60;
  CarLapTime[3][5] = 120;
  CarLapTime[3][6] = 211;
  CarLapTime[3][7] = 215;
  CarLapTime[3][8] = 221;
  CarLapTime[3][9] = 233;
  CarLapTime[3][10] = 245;
  CarLapTime[3][11] = 299;
  CarLapTime[3][12] = 332;

  strcpy(CarNumber[4], "004");
  CarLapTime[4][0] = 197;
  CarLapTime[4][1] = 197;
  CarLapTime[4][2] = 234;
  CarLapTime[4][3] = 198;
  CarLapTime[4][4] = 197;
  CarLapTime[4][5] = 207;
  CarLapTime[4][6] = 212;
  CarLapTime[4][7] = 215;
  CarLapTime[4][8] = 215;
  CarLapTime[4][9] = 230;
  CarLapTime[4][10] = 231;
  CarLapTime[4][11] = 232;
  CarLapTime[4][12] = 234;

  RaceMaxLaps = 14;

  NumberOfCars = 5;

#ifdef SHOW_DEBUG
  Serial.println(F("END DEBUG CreateTestData MainMenu()_____________________________________"));
  Serial.println();
#endif
}

/*

End of code

*/
/*

 enter CURL into a dos command window

curl examples -- for verifying server connection

curl https://api.race-monitor.com/v2/Live/GetSession --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=140330"
curl https://api.race-monitor.com/v2/Common/CurrentRaces --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6"
 
get live session
curl https://api.race-monitor.com/v2/Live/GetRacer --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=140330"
curl https://api.race-monitor.com/v2/Live/GetSession --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=140330"


replay formula car
 curl https://api.race-monitor.com/v2/Live/GetSession --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=37872"

replay 2015 US Open
 curl https://api.race-monitor.com/v2/Live/GetSession --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6&raceID=37820"



get live race feed

 const char *serverName = "https://api.race-monitor.com/v2/Common/CurrentRaces?apiToken=9f383b38-e448-405c-b3ab-babd695414f6&seriesID=50";
 curl https://api.race-monitor.com/v2/Account/PastRaces --data "apiToken=9f383b38-e448-405c-b3ab-babd695414f6"

 get races
 const char *serverName = "https://api.race-monitor.com/v2/Common/CurrentRaces?apiToken=9f383b38-e448-405c-b3ab-babd695414f6";



 working
 const char *serverName = "https://api.race-monitor.com/v2/Common/RaceTypes?apiToken=9f383b38-e448-405c-b3ab-babd695414f6";


 const char *token = "apiToken=9f383b38-e448-405c-b3ab-babd695414f6";
 const char *serverName = "https://api.race-monitor.com/v2/Common/PastRaces/";

*/



// end of code