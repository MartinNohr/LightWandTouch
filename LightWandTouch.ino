/*
    This is the Light Wand code moved over to use a graphics touch screen
*/
// set this to calibrate the display
#define CALIBRATE 0

#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even tho we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <sdfat.h>

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
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// wand settings
#define OPEN_FOLDER_CHAR '\x7e'
#define OPEN_PARENT_FOLDER_CHAR '\x7f'
#define MAXFOLDERS 10
String folders[MAXFOLDERS];
int folderLevel = 0;
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
bool bChainFiles = false;            // set to run all the files from current to the last one in the current folder
SdFile dataFile;
String CurrentFilename = "";
int CurrentFileIndex = 0;
int NumberOfFiles = 0;
String FileNames[200];

// The menu structures
enum eDisplayOperation { eTerminate, eNoop, eClear, eText };
struct MenuItem {
    enum eDisplayOperation op;
    int back_color;
    int x, y;
    char* text;
    int* value;
} ;
typedef MenuItem MenuItem;
#define LINEHEIGHT 30
MenuItem MainMenu[] = {
    {eClear,ILI9341_BLACK},
    {eText,ILI9341_BLACK,0,0,"Files"},
    {eText,ILI9341_BLACK,0,1 * LINEHEIGHT,"Frame Hold Time: %d mSec",&frameHold},
    {eText,ILI9341_BLACK,0,2 * LINEHEIGHT,"Wand Brightness: %d%%",&nStripBrightness},
    {eText,ILI9341_BLACK,0,3 * LINEHEIGHT,"Repeat Count: %d",&repeatCount},
    // make sure this one is last
    {eTerminate}
};

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
    //Serial.println(tft.width());
    //Serial.println(tft.height());
    tft.setRotation(1);
    //Serial.println(tft.width());
    //Serial.println(tft.height());
#if !CALIBRATE
    folders[folderLevel = 0] = String("/");
    setupSDcard();
    tft.setTextSize(3);
    tft.println(" Light Wand Touch");
    tft.setTextSize(2);
    tft.println("   Version 0.9");
    delay(500);
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    for (int ix = 0; ix < NumberOfFiles; ++ix) {
        tft.println(FileNames[ix]);
    }
    while (!ts.touched()) {
        ;
    }
    //for (int ix = 0; ix < 319; ++ix) {
    //    int col;
    //    col = rand();
    //    int len = 319 - ix;
    //    len = constrain(len, 100, 240);
    //    //tft.fillRect(ix, 100, 10, 100, col);
    //    tft.drawLine(ix, 100, ix, len, col);
    //    delay(10);
    //    tft.drawLine(ix, 100, ix, len, ILI9341_BLACK);
    //}
    //tft.fillCircle(120, 120, 50, ILI9341_GREENYELLOW);
    //for (int ix = 50; ix; --ix) {
    //    tft.drawCircle(120, 120, ix, ILI9341_BLACK);
    //    delay(50);
    //}
    //delay(500);
    //pinMode(3, OUTPUT);
    //analogWrite(3, 10);
    //delay(2000);
    //analogWrite(3, 255);
    //delay(2000);
    ShowMenu(MainMenu);
#endif
}

void loop()
{
#if !CALIBRATE
    // See if there's any  touch data for us
    //if (ts.bufferEmpty()) {
    //    return;
    //}
    // You can also wait for a touch
    if (! ts.touched()) {
      return;
    }
    EnterBrightness();
    delay(500);
    EnterFrameHold();
    ShowMenu(MainMenu);
    // Retrieve a point  
//    TS_Point p = ReadTouch(false);
    //Serial.print("("); Serial.print(p.x);
    //Serial.print(", "); Serial.print(p.y);
    //Serial.println(")");
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

TS_Point ReadTouch(bool wait)
{
    if (wait) {
        while (!ts.touched())
            ;
    }
    delay(10);
    // Retrieve a point  
    TS_Point p = ts.getPoint();

    while (p.z < 20 || p.z > 250 || p.y < 100 || p.x < 100) {
        p = ts.getPoint();
    }
    Serial.print("X = "); Serial.print(p.x);
    Serial.print("\tY = "); Serial.print(p.y);
    Serial.print("\tPressure = "); Serial.println(p.z);
    // Scale from ~0->4000 to tft.width using the calibration #'s
    int x, y;
    x = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
    y = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());

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

void ShowMenu(struct MenuItem* menu)
{
    tft.setTextSize(2);
    // loop through the menu
    while (menu->op != eTerminate) {
        switch (menu->op) {
        case eClear:
            tft.fillScreen(menu->back_color);
            break;
        case eText:
            tft.setTextColor(ILI9341_WHITE, menu->back_color);
            tft.setCursor(menu->x, menu->y);
            if (menu->value) {
                char line[100];
                sprintf(line, menu->text, *menu->value);
                tft.print(line);
            }
            else {
                tft.print(menu->text);
            }
            break;
        }
        ++menu;
    }
}

// get a new brightness value
void EnterBrightness()
{
    ReadNumberPad(&nStripBrightness, 1, 100, "Brightness (%) ");
}

// get a new framehold value
void EnterFrameHold()
{
    ReadNumberPad(&frameHold, 0, 1000, "Frame Time (mSec) ");
}

// check if number is in range +/- dif
bool RangeTest(int num, int base, int diff)
{
    return num > base - diff && num < base + diff;
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
        {65,12,67,20},  // 0
        {25,12,112,20}, // 1
        {65,12,112,20}, // 2
        {128,12,112,20},// 3
        {25,12,158,20}, // 4
        {65,12,158,20}, // 5
        {128,12,158,20},// 6
        {25,12,198,20}, // 7
        {65,12,198,20}, // 8
        {128,12,198,20},// 9
    };
    bool firstdigit = true;
    bool done = false;
    int cursex;
    int cursey;
    // wait for no fingers
    while (ts.touched())
        ;
    tft.fillRect(0, 0, 319, 239, ILI9341_BLUE);
    tft.setCursor(5, 5);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.setTextSize(2);
    tft.print(text);
    cursex = tft.getCursorX();
    cursey = tft.getCursorY();
    tft.print(result);
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
    while (!done) {
        p = ReadTouch(false);
        // ok
        if (p.x < 50 && p.y < 32) {
            *pval = result.toInt();
            *pval = constrain(*pval, min, max);
            done = true;
        }
        // cancel
        if (p.x > 50 && p.y < 32) {
            done = true;
            status = false;
        }
        if (RangeTest(p.x, 128, 12) && RangeTest(p.y, 67, 20)) {
            // delete the last char
            result = result.substring(0, result.length() - 1);
            tft.setCursor(cursex, cursey);
            tft.print(result + String(" "));
        }
        for (int ix = 0; ix < 10; ++ix) {
            // check for match
            if (RangeTest(p.x, numranges[ix].x, numranges[ix].dx) && RangeTest(p.y, numranges[ix].y, numranges[ix].dy)) {
                Serial.println("GOT: " + String(ix));
                if (firstdigit) {
                    result = String("");
                    firstdigit = false;
                }
                if (result.length() < 3)
                    result += String(ix);
                tft.setTextSize(2);
                tft.setCursor(cursex, cursey);
                tft.print(result + String("  "));
                break;
            }
        }
    }
    return status;
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
    delay(1000);
//    folders[folderLevel = 0] = String("/");
//    lcd.clear();
//    lcd.print("Reading SD...   ");
    delay(500);
    GetFileNamesFromSD(folders[folderLevel]);
    CurrentFilename = FileNames[CurrentFileIndex];
    //DisplayCurrentFilename();
}

// read the files from the card
// look for start.lwc, and process it, but don't add it to the list
bool GetFileNamesFromSD(String dir) {
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
        FileNames[NumberOfFiles++] = String(OPEN_PARENT_FOLDER_CHAR) + folders[folderLevel - 1];
    }
    while (file.openNext(&root, O_RDONLY)) {
        if (!file.isHidden()) {
            char buf[100];
            file.getName(buf, sizeof buf);
            Serial.println("name: " + String(buf));
            if (file.isDir()) {
                FileNames[NumberOfFiles] = String(OPEN_FOLDER_CHAR) + buf;
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
