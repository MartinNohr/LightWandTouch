#pragma once
#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even though we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <sdfat.h>
#include <FastLED.h>
#include <timer.h>
#include <avr/eeprom.h>

#define DATA_PIN 31
#define NUM_LEDS 144
int AuxButton = 35;                       // Aux Select Button Pin
int g = 0;                                // Variable for the Green Value
int b = 0;                                // Variable for the Blue Value
int r = 0;                                // Variable for the Red Value

// functions
bool CheckCancel();

// Define the array of leds
CRGB leds[NUM_LEDS * 2];

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
bool bAutoLoadSettings = false;           // set to automatically load saved settings from eeprom
//bool bAutoLoadStart = true;               // set to automatically load the start.lwc file
bool bScaleHeight = false;                // scale the Y values to fit the number of pixels
bool bCancelRun = false;                  // set to cancel a running job
bool bChainFiles = false;                 // set to run all the files from current to the last one in the current folder
SdFile dataFile;
int nMaxBackLight = 75;                 // maximum backlight to use in %
int nMinBackLight = 25;                 // dimmest setting
int nBackLightSeconds = 10;             // how long to leave the backlight on before dimming
volatile bool bBackLightOn = false;     // used by backlight timer to indicate that backlight is on
volatile bool bTurnOnBacklight = true;  // set to turn the backlight on, safer than calling the BackLightControl code
int CurrentFileIndex = 0;
int NumberOfFiles = 0;
#define MAX_FILES 25
String FileNames[MAX_FILES];
bool bShowBuiltInTests = false;

struct saveValues {
    void* val;
    int size;
};
const saveValues saveValueList[] = {
    {&signature, sizeof(signature)},
    {&bAutoLoadSettings, sizeof(bAutoLoadSettings)},
    {&nStripBrightness, sizeof(nStripBrightness)},
    {&frameHold, sizeof(frameHold)},
    {&startDelay, sizeof(startDelay)},
    {&repeat, sizeof(repeat)},
    {&repeatCount, sizeof(repeatCount)},
    {&repeatDelay, sizeof(repeatDelay)},
    {&bGammaCorrection, sizeof(bGammaCorrection)},
    {&stripLength, sizeof(stripLength)},
    {&nBackLightSeconds, sizeof(nBackLightSeconds)},
    {&nMaxBackLight, sizeof(nMaxBackLight)},
    {&CurrentFileIndex,sizeof(CurrentFileIndex)},
    {&bShowBuiltInTests,sizeof(bShowBuiltInTests)},
    {&bScaleHeight,sizeof(bScaleHeight)},
    {&bChainFiles,sizeof(bChainFiles)},
};

// The menu structures
enum eDisplayOperation {
    eClear,     // set screen background
    eText,      // handle text with optional %s value
    eTextInt,   // handle text with optional %d value
    eBool,      // handle bool using %s and on/off values
    eMenu,      // load another menu
    eExit,      // closes this menu
    eSkipTrue,  // skip the next menu item if boolean is true
    eSkipFalse, // skip the next menu item if boolean is false
    eTerminate, // must be last in a menu
};
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
void EraseStartFile(MenuItem*);
void SaveStartFile(MenuItem*);
void LoadStartFile(MenuItem*);
void WriteMessage(String, bool error = false, int wait = 2000);
void SaveEepromSettings(MenuItem*);
void LoadEepromSettings(MenuItem*);

void RunningDot();
void TestCylon();
void TestMeteor();
void TestBouncingBalls();
void BarberPole();
void OppositeRunningDots();

// adjustment values
int nBouncingBallsCount = 4;
int nBouncingBallsDecay = 1000;
int nBouncingBallsRuntime = 20; // in seconds

MenuItem RepeatMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Repeat Count: %d",GetIntegerValue,&repeatCount,1,1000},
    {eTextInt,ILI9341_BLACK,"Repeat Delay: %d",GetIntegerValue,&repeatDelay,0,1000},
    {eBool,   ILI9341_BLACK,"Chain Files: %s",ToggleBool,&bChainFiles,0,0,"On","Off"},
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
    {eTextInt,ILI9341_BLACK,"Pixel Count: %d",GetIntegerValue,&stripLength,1,288},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem BouncingBallsMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eText,   ILI9341_BLACK,"Bouncing Balls"},
    {eTextInt,ILI9341_BLACK,"Ball Count (1-8): %d",GetIntegerValue,&nBouncingBallsCount,1,8},
    {eTextInt,ILI9341_BLACK,"Decay (500-10000): %d",GetIntegerValue,&nBouncingBallsDecay,500,10000},
    {eTextInt,ILI9341_BLACK,"Runtime (seconds): %d",GetIntegerValue,&nBouncingBallsRuntime,1,10000},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem EepromMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eBool,   ILI9341_BLACK,"Autoload Defaults: %s",ToggleBool,&bAutoLoadSettings,0,0,"On","Off"},
    {eText,   ILI9341_BLACK,"Save Default Settings",SaveEepromSettings},
    {eText,   ILI9341_BLACK,"Load Default Settings",LoadEepromSettings},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem OtherSettingsMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Max Display Bright: %d%%",GetIntegerValue,&nMaxBackLight,1,100},
    {eTextInt,ILI9341_BLACK,"Min Display Bright: %d%%",GetIntegerValue,&nMinBackLight,5,100},
    {eTextInt,ILI9341_BLACK,"Backlight Timeout: %d (S)",GetIntegerValue,&nBackLightSeconds,1,1000},
    {eMenu,   ILI9341_BLACK,"Default Settings",NULL,EepromMenu},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem StartFileMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eText,   ILI9341_BLACK,"Erase START.LWC",EraseStartFile},
    {eText,   ILI9341_BLACK,"Save START.LWC",SaveStartFile},
    {eText,   ILI9341_BLACK,"Load START.LWC",LoadStartFile},
    {eExit,   ILI9341_BLACK,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem MainMenu[] = {
    {eClear,    ILI9341_BLACK},
    {eText,     ILI9341_BLACK,"Choose File",EnterFileName},
    {eMenu,     ILI9341_BLACK,"Wand Settings",NULL,WandMenu},
    {eMenu,     ILI9341_BLACK,"Repeat Settings",NULL,RepeatMenu},
    {eBool,     ILI9341_BLACK,"Built-in Images (%s)",ToggleFilesBuiltin,&bShowBuiltInTests,0,0,"On","Off"},
    {eSkipTrue, ILI9341_BLACK,"",NULL,&bShowBuiltInTests},
    {eMenu,     ILI9341_BLACK,"START.LWC Operations",NULL,StartFileMenu},
    {eSkipFalse,ILI9341_BLACK,"",NULL,&bShowBuiltInTests},
    {eMenu,     ILI9341_BLACK,"Internal File Settings",NULL,BouncingBallsMenu},
    {eMenu,     ILI9341_BLACK,"Other Settings",NULL,OtherSettingsMenu},
    // make sure this one is last
    {eTerminate}
};

// a stack for menus so we can find our way back
MenuItem* menustack[10];
int menuLevel = 0;

// built-in "files"
struct BuiltInItem {
    char* text;
    void(*function)();
    MenuItem* menu;     // not used yet, but might be used to set the correct menu
};
typedef BuiltInItem BuiltInItem;
BuiltInItem BuiltInFiles[] = {
    {"Running Dot",RunningDot},
    {"2 Running Dots",OppositeRunningDots},
    {"Barber Pole",BarberPole},
    {"Bouncing Balls",TestBouncingBalls,BouncingBallsMenu},
    {"Cylon",TestCylon},
    {"Meteor",TestMeteor},
};

// timers to run things
auto EventTimers = timer_create_default();
// use for countdowns in seconds
volatile int nTimerSeconds;
// set this to the delay time while we get the next frame
volatile bool bStripWaiting = false;

// Gramma Correction (Defalt Gamma = 2.8)
const uint8_t PROGMEM gammaR[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,
    2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,
    5,  5,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,
    9,  9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14,
   15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
   23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33,
   33, 34, 35, 36, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46,
   46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
   62, 63, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 78, 79, 80,
   81, 83, 84, 85, 87, 88, 89, 91, 92, 94, 95, 97, 98, 99,101,102,
  104,105,107,109,110,112,113,115,116,118,120,121,123,125,127,128,
  130,132,134,135,137,139,141,143,145,146,148,150,152,154,156,158,
  160,162,164,166,168,170,172,174,177,179,181,183,185,187,190,192,
  194,196,199,201,203,206,208,210,213,215,218,220,223,225,227,230 };

const uint8_t PROGMEM gammaG[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };


const uint8_t PROGMEM gammaB[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
    4,  4,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  7,  7,  7,  8,
    8,  8,  8,  9,  9,  9, 10, 10, 10, 10, 11, 11, 12, 12, 12, 13,
   13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 19,
   20, 20, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28,
   29, 30, 30, 31, 32, 32, 33, 34, 34, 35, 36, 37, 37, 38, 39, 40,
   40, 41, 42, 43, 44, 44, 45, 46, 47, 48, 49, 50, 51, 51, 52, 53,
   54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 69, 70,
   71, 72, 73, 74, 75, 77, 78, 79, 80, 81, 83, 84, 85, 86, 88, 89,
   90, 92, 93, 94, 96, 97, 98,100,101,103,104,106,107,109,110,112,
  113,115,116,118,119,121,122,124,126,127,129,131,132,134,136,137,
  139,141,143,144,146,148,150,152,153,155,157,159,161,163,165,167,
  169,171,173,175,177,179,181,183,185,187,189,191,193,196,198,200 };
