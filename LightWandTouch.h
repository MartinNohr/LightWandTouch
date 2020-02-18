#pragma once
#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even though we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <sdfat.h>
#include <FastLED.h>
#include <timer.h>

#define DATA_PIN 31
#define NUM_LEDS 144
int AuxButton = 35;                       // Aux Select Button Pin
int g = 0;                                // Variable for the Green Value
int b = 0;                                // Variable for the Blue Value
int r = 0;                                // Variable for the Red Value

// Define the array of leds
CRGB leds[NUM_LEDS];

// This is calibration data for the raw touch data to the screen coordinates
// since we rotated the screen these reverse x and y
#define TS_MINX 226
#define TS_MINY 166
#define TS_MAXX 3861
#define TS_MAXY 3790

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

#define SDcsPin 4                        // SD card CS pin

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
#define TFT_BRIGHT 3
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// wand settings
#define NEXT_FOLDER_CHAR '~'
#define PREVIOUS_FOLDER_CHAR '^'
String currentFolder = "/";
SdFat SD;
char signature[]{ "MLW" };                // set to make sure saved values are valid
int stripLength = 144;                    // Set the number of LEDs the LED Strip
int frameHold = 10;                       // default for the frame delay 
int lastMenuItem = -1;                    // check to see if we need to redraw menu
//int menuItem = mFirstMenu;                // Variable for current main menu selection
int startDelay = 0;                       // Variable for delay between button press and start of light sequence, in seconds
int repeat = 0;                           // Variable to select auto repeat (until select button is pressed again)
int repeatDelay = 0;                      // Variable for delay between repeats
int repeatCount = 1;                      // Variable to keep track of number of repeats
int nStripBrightness = 10;                // Variable and default for the Brightness of the strip
bool bGammaCorrection = true;             // set to use the gamma table
bool bAutoLoadSettings = false;           // set to automatically load saved settings
bool bScaleHeight = false;                // scale the Y values to fit the number of pixels
bool bCancelRun = false;                  // set to cancel a running job
bool bChainFiles = false;                 // set to run all the files from current to the last one in the current folder
SdFile dataFile;
int CurrentFileIndex = 0;
int NumberOfFiles = 0;
String FileNames[200];
bool bShowBuiltInTests = false;

// built-in "files"
struct BuiltInItem {
    char* text;
    void(*function)();
};
typedef BuiltInItem BuiltInItem;
void RunningDot();
void TestCylon();
void TestMeteor();
void TestBouncingBalls();
// adjustment values
int nBouncingBalls = 4;
BuiltInItem BuiltInFiles[] = {
    {"Running Dot",RunningDot},
    {"Cylon",TestCylon},
    {"Meteor",TestMeteor},
    {"Bouncing Balls",TestBouncingBalls},
};

// The menu structures
enum eDisplayOperation { eTerminate, eNoop, eClear, eTextInt, eBool, eText, eMenu, eExit };
struct MenuItem {
    enum eDisplayOperation op;
    int back_color;
    char* text;
    void(*function)(MenuItem*);
    void* value;
    int min, max;
    char* on;   // for boolean
    char* off;
};
typedef MenuItem MenuItem;
#define LINEHEIGHT 36

void GetIntegerValue(MenuItem*);
void ToggleBool(MenuItem*);
void EnterFileName(MenuItem*);
void ToggleFilesBuiltin(MenuItem*);

MenuItem RepeatMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Repeat Count: %d",GetIntegerValue,&repeatCount,1,1000},
    {eTextInt,ILI9341_BLACK,"Repeat Delay: %d",GetIntegerValue,&repeatDelay,0,1000},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem WandMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Frame Hold Time: %d mSec",GetIntegerValue,&frameHold,0,10000},
    {eTextInt,ILI9341_BLACK,"Wand Brightness: %d%%",GetIntegerValue,&nStripBrightness,1,100},
    {eTextInt,ILI9341_BLACK,"Start Delay (Sec): %d",GetIntegerValue,&startDelay,0,1000},
    {eBool,   ILI9341_BLACK,"Gamma Correction: %s",ToggleBool,&bGammaCorrection,0,0,"On","Off"},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem MainMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eText,   ILI9341_BLACK,"Choose File",EnterFileName},
    {eMenu,   ILI9341_BLACK,"Wand Settings",NULL,WandMenu},
    {eMenu,   ILI9341_BLACK,"Repeat Settings",NULL,RepeatMenu},
    {eBool,   ILI9341_BLACK,"Built-in Images (%s)",ToggleFilesBuiltin,&bShowBuiltInTests,0,0,"On","Off"},
    {eText,   ILI9341_BLACK,"Settings"},
    // make sure this one is last
    {eTerminate}
};
MenuItem* currentMenu = MainMenu;
// a stack for menus so we can find our way back
MenuItem* menustack[10];
int menuLevel = 0;

int nMaxBackLight = 75;                 // maximum backlight to use in %
int nBackLightSeconds = 10;             // how long to leave the backlight on before dimming
volatile bool bBackLightOn = false;     // used by backlight timer to indicate that backlight is on
volatile bool bTurnOnBacklight = true;  // set to turn the backlight on, safer than calling the BackLightControl code

// timers to run things
auto EventTimers = timer_create_default();
// set this to the delay time while we get the next frame
bool bStripWaiting = false;
// this gets called every second/TIMERSTEPS
