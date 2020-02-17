/*
    This is the Light Wand code moved over to use a graphics touch screen
*/
// set this to calibrate the touch screen
#define CALIBRATE 0

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even tho we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <sdfat.h>
#include <FastLED.h>

#define DATA_PIN 31
#define NUM_LEDS 144

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
int frameHold = 15;                       // default for the frame delay 
int lastMenuItem = -1;                    // check to see if we need to redraw menu
//int menuItem = mFirstMenu;                // Variable for current main menu selection
int startDelay = 0;                       // Variable for delay between button press and start of light sequence, in seconds
int repeat = 0;                           // Variable to select auto repeat (until select button is pressed again)
int repeatDelay = 0;                      // Variable for delay between repeats
int repeatCount = 1;                      // Variable to keep track of number of repeats
int nStripBrightness = 50;                // Variable and default for the Brightness of the strip
bool bGammaCorrection = true;             // set to use the gamma table
bool bAutoLoadSettings = false;           // set to automatically load saved settings
bool bScaleHeight = false;                // scale the Y values to fit the number of pixels
bool bCancelRun = false;                  // set to cancel a running job
bool bChainFiles = false;                 // set to run all the files from current to the last one in the current folder
SdFile dataFile;
String CurrentFilename = "";
int CurrentFileIndex = 0;
int NumberOfFiles = 0;
String FileNames[200];

// The menu structures
enum eDisplayOperation { eTerminate, eNoop, eClear, eTextInt, eBool, eText, eMenu, eExit };
struct MenuItem {
    enum eDisplayOperation op;
    int back_color;
    char* text;
    void(*function)(MenuItem*);
    void* value;
    int min, max;
} ;
typedef MenuItem MenuItem;
#define LINEHEIGHT 36
MenuItem RepeatMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Repeat Count: %d",GetIntegerValue,&repeatCount,1,1000},
    {eTextInt,ILI9341_BLACK,"Repeat Delay: %d",GetIntegerValue,&repeatDelay,0,1000},
    {eExit,   ILI9341_BLACK,"Main Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem WandMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eTextInt,ILI9341_BLACK,"Frame Hold Time: %d mSec",GetIntegerValue,&frameHold,0,10000},
    {eTextInt,ILI9341_BLACK,"Wand Brightness: %d%%",GetIntegerValue,&nStripBrightness,1,100},
    {eTextInt,ILI9341_BLACK,"Start Delay (Sec): %d",GetIntegerValue,&startDelay,0,1000},
    {eExit,   ILI9341_BLACK,"Main Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem MainMenu[] = {
    {eClear,  ILI9341_BLACK},
    {eText,   ILI9341_BLACK,"File Chooser",EnterFileName},
    {eMenu,   ILI9341_BLACK,"Wand Settings",NULL,WandMenu},
    {eMenu,   ILI9341_BLACK,"Repeat Settings",NULL,RepeatMenu},
    {eText,   ILI9341_BLACK,"Built-in Images",TestCylon},
    {eText,   ILI9341_BLACK,"Settings"},
    // make sure this one is last
    {eTerminate}
};
MenuItem* currentMenu = MainMenu;
// a stack for menus
MenuItem* menustack[10];
int menuLevel = 0;

void setup(void) {
    tft.begin();
    delay(50);
    Serial.begin(115200);
    Serial.println(F("Light Wand Touch"));

    if (!ts.begin()) {
        Serial.println("Couldn't start touchscreen controller");
        while (1);
    }
    Serial.println("Touchscreen started");
    tft.fillScreen(ILI9341_BLACK);
    tft.setRotation(1);
#if !CALIBRATE
    setupSDcard();
    tft.setTextColor(ILI9341_BLUE);
    tft.setTextSize(3);
    tft.println("\n\n");
    tft.println(" Light Wand Touch");
    tft.setTextSize(2);
    tft.println("\n");
    tft.println("       Version 0.9");
    tft.println("       Martin Nohr");
    delay(1500);
    // control brightness of screen
    //pinMode(TFT_BRIGHT, OUTPUT);
    //analogWrite(3, 10);
    //delay(2000);
    //analogWrite(3, 255);
    //delay(2000);
#endif
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    // Turn the LED on, then pause
    leds[0] = leds[1] = CRGB::Red;
    leds[4] = leds[5] = CRGB::Green;
    leds[8] = leds[9] = CRGB::Blue;
    FastLED.setBrightness(nStripBrightness * 255 / 100);
    for (int ix = 0; ix < 255; ++ix) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    for (int ix = 255; ix; --ix) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    //FastLED.show();
    delay(500);
    // Now turn the LED off, then pause
    FastLED.clear(true);
    //leds[0] = CRGB::Black;
    //FastLED.show();
    delay(500);
}

bool bMenuChanged = true;

void loop()
{
#if !CALIBRATE
    // See if there's any  touch data for us
    //if (ts.bufferEmpty()) {
    //    return;
    //}
    // wait for a touch
    //if (!ts.touched()) {
    //  return;
    //}
    if (bMenuChanged) {
        ShowMenu(currentMenu);
        ShowGo();
        bMenuChanged = false;
    }

    // Retrieve a point  
    TS_Point p = ReadTouch(false);
    Serial.print("("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");
    // see if the go button
    if (RangeTest(p.x, tft.width() - 40, 25) && RangeTest(p.y, tft.height() - 16, 20)) {
        Serial.println("GO...");
        tft.fillScreen(ILI9341_BLUE);
        tft.fillRect(0, 0, tft.width() - 1, 10, ILI9341_LIGHTGREY);
        tft.setCursor(0, 12);
        tft.print(currentFolder + CurrentFilename);
        for (int x = 0; x <= 100; ++x) {
            int wide = map(x, 0, 100, 0, tft.width() - 1);
            tft.fillRect(0, 0, wide, 10, ILI9341_DARKGREY);
            if (ts.touched()) {
                ReadTouch(false);
                break;
            }
            delay(100);
        }
        delay(500);
        bMenuChanged = true;
        return;
    }
    // see if we got a menu match
    for (int ix = 0; currentMenu[ix].op != eTerminate; ++ix) {
        // look for a match
        if (RangeTest(p.y, ix * LINEHEIGHT, LINEHEIGHT / 2)) {
            Serial.println("clicked on menu");
            // got one, service it
            switch (currentMenu[ix].op) {
            case eText:
            case eTextInt:
                if (currentMenu[ix].function) {
                    Serial.println(ix);
                    (*currentMenu[ix].function)(&currentMenu[ix]);
                    bMenuChanged = true;
                }
                break;
            case eMenu:
                menustack[menuLevel++] = currentMenu;
                currentMenu = (MenuItem*)(menustack[menuLevel - 1][ix].value);
                bMenuChanged = true;
                break;
            case eExit: // go back a level
                if (menuLevel) {
                    currentMenu = menustack[--menuLevel];
                    bMenuChanged = true;
                }
            }
            break;
        }
    }
    
#else
    tft.fillCircle(5, 5, 5, ILI9341_WHITE);
    tft.fillCircle(tft.width() - 1 - 5, 5, 5, ILI9341_WHITE);
    tft.fillCircle(tft.width() - 1 - 5, tft.height() - 1 - 5, 5, ILI9341_WHITE);
    tft.fillCircle(5, tft.height() - 1 - 5, 5, ILI9341_WHITE);
    if (!ts.touched()) {
        return;
    }
    ReadTouch(true);
#endif
}

void ShowGo()
{
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.print(currentFolder + CurrentFilename.substring(0, CurrentFilename.lastIndexOf(".")) + " " + String(CurrentFileIndex + 1) + "/" + String(NumberOfFiles));
    tft.fillRoundRect(tft.width() - 50, tft.height() - 50, 45, 45, 10, ILI9341_DARKGREEN);
    tft.setCursor(tft.width() - 40, tft.height() - 34);
    tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREEN);
    tft.print("GO");
}

// Top left is origin, down is y, right is x
TS_Point ReadTouch(bool wait)
{
    if (wait) {
        while (!ts.touched())
            ;
    }
    delay(10);
    // Retrieve a point  
    TS_Point p = ts.getPoint();
    // eat the ones with not enough pressure
    while (p.z < 35 || p.z > 250) {
        //while (p.z < 35 || p.z > 250 || p.y < 100 || p.x < 100) {
        p = ts.getPoint();
    }
    Serial.print("X = "); Serial.print(p.x);
    Serial.print("\tY = "); Serial.print(p.y);
    Serial.print("\tPressure = "); Serial.println(p.z);
    // Scale from ~0->4000 to tft.width using the calibration #'s
    int x, y;
    x = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
    y = map(p.x, TS_MINX, TS_MAXX, tft.height(), 0);

    Serial.print("("); Serial.print(x);
    Serial.print(", "); Serial.print(y);
    Serial.println(")");
    while (ts.touched())
        ;
    ts.getPoint();
    p.x = x;
    p.y = y;
    return p;
}

// display the menu
void ShowMenu(struct MenuItem* menu)
{
    int y = 0;
    int x = 0;
    tft.setTextSize(2);
    // loop through the menu
    while (menu->op != eTerminate) {
        switch (menu->op) {
        case eClear:
            tft.fillScreen(menu->back_color);
            break;
        case eTextInt:
        case eText:
        case eBool:
            // increment displayable lines
            y += LINEHEIGHT;
            tft.setTextColor(ILI9341_WHITE, menu->back_color);
            tft.setCursor(x, y);
            if (menu->value) {
                char line[100];
                sprintf(line, menu->text, *(int*)menu->value);
                tft.print(line);
            }
            else {
                tft.print(menu->text);
            }
            break;
        case eMenu:
        case eExit:
            y += LINEHEIGHT;
            tft.setTextColor(ILI9341_WHITE, menu->back_color);
            tft.setCursor(x, y);
            tft.print(menu->text);
            break;
        }
        ++menu;
    }
}

// get integer values
void GetIntegerValue(MenuItem* menu)
{
    ReadNumberPad((int*)menu->value, menu->min, menu->max, menu->text);
}

// select the filename
void EnterFileName(MenuItem* menu)
{
    bool done = false;
    int startindex = 0;
    tft.setTextSize(3);
    tft.fillScreen(ILI9341_BLACK);
    TS_Point p;
    while (!done) {
        tft.setCursor(0, 0);
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        for (int ix = startindex; ix < NumberOfFiles; ++ix) {
            tft.println();
            // pad out to 16
            String nm = FileNames[ix];
            while (nm.length() < 15)
                nm += " ";
            tft.println(nm);
        }
        tft.fillRoundRect(tft.width() - 50, 6, 45, 45, 10, ILI9341_WHITE);
        tft.fillRoundRect(tft.width() - 50, tft.height() - 50, 45, 45, 10, ILI9341_WHITE);
        tft.fillRoundRect(tft.width() - 50, tft.height() / 2 - 22, 45, 45, 10, ILI9341_WHITE);
        tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
        tft.setCursor(tft.width() - 35, 18);
        tft.print("-");
        tft.setCursor(tft.width() - 35, tft.height() - 38);
        tft.print("+");
        tft.setCursor(tft.width() - 35, tft.height() / 2 - 10);
        tft.print("X");
        while (!ts.touched()) {
            ;
        }
        p = ReadTouch(true);
        if (RangeTest(p.x, tft.width() - 40, 25) && RangeTest(p.y, 25, 20)) {
            startindex -= 5;
        }
        else if (RangeTest(p.x, tft.width() - 40, 25) && RangeTest(p.y, tft.height() - 20, 20)) {
            startindex += 5;
        }
        // see if a file
        else if (p.x < tft.width() - 80) {
            // get the row
            int row = (p.y - 20);
            row = constrain(row, 0, tft.height() - 1);
            row /= 50;
            row += startindex;
            String tmp = FileNames[row];
            if (tmp[0] == NEXT_FOLDER_CHAR) {
                tmp = tmp.substring(1);
                // change folder, reload files
                currentFolder += tmp + "/";
                GetFileNamesFromSD(currentFolder);
                EnterFileName(menu);
                return;
            }
            else if (tmp[0] == PREVIOUS_FOLDER_CHAR) {
                tmp = tmp.substring(1);
                tmp = tmp.substring(0, tmp.length() - 1);
                if (tmp.length() == 0)
                    tmp = "/";
                // change folder, reload files
                currentFolder = tmp;
                GetFileNamesFromSD(currentFolder);
                EnterFileName(menu);
                return;
            }
            else {
                // just a file, set the current value
                CurrentFileIndex = row;
                CurrentFilename = FileNames[CurrentFileIndex];
                done = true;
            }
        }
        else {
            done = true;
        }
        startindex = constrain(startindex, 0, NumberOfFiles - 5);
    }
}

// check if number is in range +/- dif
bool RangeTest(int num, int base, int diff)
{
    int min = base - diff;
    if (min < 0)
        min = 0;
    int max = base + diff;
    return num > min && num < max;
}

// return a number using the onscreen keypad, false if cancelled
bool ReadNumberPad(int* pval, int min, int max, char* text)
{
    bool status = true;
    String result = String(*pval);
    struct NumRange {
        int x;
        int dx;
        int y;
        int dy;
    };
    struct NumRange numranges[10] = {
        {65,12,170,20}, // 0
        {15,12,128,20}, // 1
        {65,12,128,20}, // 2
        {128,12,128,20},// 3
        {12,12,85,20},  // 4
        {65,12,85,20},  // 5
        {128,12,85,20}, // 6
        {12,12,45,20},  // 7
        {65,12,45,20},  // 8
        {125,12,45,20}, // 9
    };
    bool firstdigit = true;
    bool done = false;
    // wait for no fingers
    while (ts.touched())
        ;
    tft.fillRect(0, 0, 319, 239, ILI9341_BLUE);
    DisplayValueLine(text, result.toInt(), 0);
    tft.setTextSize(5);
    tft.setCursor(0, 30);
    tft.println("7 8 9");
    tft.println("4 5 6");
    tft.println("1 2 3");
    tft.println("  0 <");
    tft.setTextSize(4);
    tft.setCursor(5, 200);
    tft.print("OK CANCEL");
    TS_Point p;
    int delchars = 0;   // to wipe out digits when the number is shorter on the line
    while (!done) {
        p = ReadTouch(false);
        // ok
        if (p.x < 50 && p.y > tft.height() - 40) {
            *pval = result.toInt();
            *pval = constrain(*pval, min, max);
            done = true;
        }
        // cancel
        if (p.x > 50 && p.y > tft.height() - 40) {
            done = true;
            status = false;
        }
        if (RangeTest(p.x, 128, 12) && RangeTest(p.y, 158, 20)) {
            // delete the last char
            result = result.substring(0, result.length() - 1);
            delchars = 1;
        }
        for (int ix = 0; ix < 10; ++ix) {
            // check for match
            if (RangeTest(p.x, numranges[ix].x, numranges[ix].dx) && RangeTest(p.y, numranges[ix].y, numranges[ix].dy)) {
                if (firstdigit) {
                    delchars = result.length();
                    result = String("");
                    firstdigit = false;
                }
                if (result.toInt() < max)
                    result += String(ix);
                Serial.println("result: " + result);
                DisplayValueLine(text, result.toInt(), delchars);
                delchars = 0;
                break;
            }
        }
    }
    return status;
}
void DisplayValueLine(char* text, int val, int blanks)
{
    tft.setCursor(5, 5);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.setTextSize(2);
    char line[100];
    sprintf(line, text, val);
    tft.print(line);
    while (blanks) {
        tft.print(" ");
        --blanks;
    }
}

void setupSDcard() {
    pinMode(SDcsPin, OUTPUT);

    while (!SD.begin(SDcsPin)) {
        Serial.println("failed to init sd");
//        bBackLightOn = true;
//        lcd.print("SD init failed! ");
        delay(1000);
//        lcd.clear();
        delay(500);
    }
//    lcd.clear();
//    lcd.print("SD init done    ");
    //delay(1000);
//    folders[folderLevel = 0] = String("/");
//    lcd.clear();
//    lcd.print("Reading SD...   ");
    //delay(500);
    GetFileNamesFromSD(currentFolder);
    CurrentFilename = FileNames[CurrentFileIndex];
    //DisplayCurrentFilename();
}

// read the files from the card
// look for start.lwc, and process it, but don't add it to the list
bool GetFileNamesFromSD(String dir) {
    Serial.println("getting files from: " + dir);
    String startfile;
    // Directory file.
    SdFile root;
    // Use for files
    SdFile file;
    // start over
    NumberOfFiles = 0;
    CurrentFileIndex = 0;
    String CurrentFilename = "";

    if (!root.open(dir.c_str())) {
        Serial.println("open failed: " + dir);
        return false;
    }
    if (dir != "/") {
        // add an arrow to go back
        String sdir = currentFolder.substring(0, currentFolder.length() - 1);
        sdir = sdir.substring(0, sdir.lastIndexOf("/"));
        if (sdir.length() == 0)
            sdir = "/";
        FileNames[NumberOfFiles++] = String(PREVIOUS_FOLDER_CHAR) + sdir;
    }
    while (file.openNext(&root, O_RDONLY)) {
        if (!file.isHidden()) {
            char buf[100];
            file.getName(buf, sizeof buf);
            //Serial.println("name: " + String(buf));
            if (file.isDir()) {
                FileNames[NumberOfFiles] = String(NEXT_FOLDER_CHAR) + buf;
                NumberOfFiles++;
            }
            else if (file.isFile()) {
                CurrentFilename = String(buf);
                String uppername = CurrentFilename;
                uppername.toUpperCase();
                if (uppername.endsWith(".BMP")) { //find files with our extension only
                    FileNames[NumberOfFiles] = CurrentFilename;
                    NumberOfFiles++;
                }
                else if (uppername == "START.LWC") {
                    startfile = CurrentFilename;
                }
            }
        }
        file.close();
    }
    root.close();
    delay(500);
    isort(FileNames, NumberOfFiles);
    // see if we need to process the auto start file
    //if (startfile.length())
    //    ProcessConfigFile(startfile);
    return true;
}

// Sort the filenames in alphabetical order
void isort(String* filenames, int n) {
    for (int i = 1; i < n; ++i) {
        String istring = filenames[i];
        int k;
        for (k = i - 1; (k >= 0) && (CompareStrings(istring, filenames[k]) < 0); k--) {
            filenames[k + 1] = filenames[k];
        }
        filenames[k + 1] = istring;
    }
}

// compare two strings, one-two, case insensitive
int CompareStrings(String one, String two)
{
    one.toUpperCase();
    two.toUpperCase();
    return one.compareTo(two);
}

void TestCylon(MenuItem* pmenu)
{
    CylonBounce(255 * nStripBrightness / 100, 255, 0, 0, 4, frameHold, 50);
}
void CylonBounce(byte bright, byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay)
{
    FastLED.setBrightness(bright);
    for (int i = 0; i < stripLength - EyeSize - 2; i++) {
        //if (CheckCancel())
        //    return;
        FastLED.clear();
        leds[i]=CRGB(red / 10, green / 10, blue / 10);
        for (int j = 1; j <= EyeSize; j++) {
            leds[i+j]=CRGB(red, green, blue);
        }
        leds[i+EyeSize+1]=CRGB( red / 10, green / 10, blue / 10);
        FastLED.show();
        delay(SpeedDelay);
    }

    delay(ReturnDelay);

    for (int i = stripLength - EyeSize - 2; i > 0; i--) {
        //if (CheckCancel())
        //    return;
        FastLED.clear();
        leds[i]=CRGB(red / 10, green / 10, blue / 10);
        for (int j = 1; j <= EyeSize; j++) {
            leds[i+j]=CRGB(red, green, blue);
        }
        leds[i+EyeSize+1]=CRGB(red / 10, green / 10, blue / 10);
        FastLED.show();
        delay(SpeedDelay);
    }
    delay(ReturnDelay);
    FastLED.clear(true);
}
