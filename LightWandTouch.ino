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

 // This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 380
#define TS_MINY 310
#define TS_MAXX 3500
#define TS_MAXY 3700

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// wand settings
int repeatCount = 1;
int brightness = 50;
int frameHoldTime = 25;

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
    {eText,ILI9341_BLACK,0,1 * LINEHEIGHT,"Frame Hold Time: %d mSec",&frameHoldTime},
    {eText,ILI9341_BLACK,0,2 * LINEHEIGHT,"Wand Brightness: %d%%",&brightness},
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
    tft.setRotation(1);
    tft.setTextSize(3);
    tft.println(" Light Wand Touch");
    tft.setTextSize(2);
    tft.println("   Version 0.9");
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
    delay(500);
    ShowMenu(MainMenu);
}


void loop()
{
#if (!CALIBRATE)
    // See if there's any  touch data for us
    //if (ts.bufferEmpty()) {
    //    return;
    //}
    // You can also wait for a touch
    if (! ts.touched()) {
      return;
    }
    EnterBrightness();
    ShowMenu(MainMenu);
    // Retrieve a point  
    TS_Point p = ts.getPoint();
    if (p.z > 10) {

        //Serial.print("X = "); Serial.print(p.x);
        //Serial.print("\tY = "); Serial.print(p.y);
        //Serial.print("\tPressure = "); Serial.println(p.z);

        // Scale from ~0->4000 to tft.width using the calibration #'s
        p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
        p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

        Serial.print("("); Serial.print(p.x);
        Serial.print(", "); Serial.print(p.y);
        Serial.println(")");
    }
#else
    // You can also wait for a touch
    if (!ts.touched()) {
        return;
    }
    // Retrieve a point  
    TS_Point p = ts.getPoint();

    Serial.print("X = "); Serial.print(p.x);
    Serial.print("\tY = "); Serial.print(p.y);
    Serial.print("\tPressure = "); Serial.println(p.z);
    // Scale from ~0->4000 to tft.width using the calibration #'s
    p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

    Serial.print("("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");
    while (ts.touched())
        ;
#endif
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

void EnterBrightness()
{
    int newbright = brightness;
    bool done = false;
    int x, y;
    int cursex;
    int cursey;
    // wait for no fingers
    while (ts.touched())
        ;
    tft.fillRect(0, 0, 300, 239, ILI9341_BLUE);
    tft.setCursor(5, 5);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.setTextSize(2);
    tft.print("Enter Brightness (%) ");
    cursex = tft.getCursorX();
    cursey = tft.getCursorY();
    tft.print(newbright);
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
        while (!ts.touched())
            ;
        while (ts.touched()) {
            p = ts.getPoint();
            if (p.z > 10 && p.z < 255) {
                y = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
                x = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

                Serial.print("("); Serial.print(x);
                Serial.print(", "); Serial.print(y);
                Serial.print(", "); Serial.print(p.z);
                Serial.println(")");
            }
        }
        Serial.print(x);
        Serial.print(" ");
        Serial.println(y);
        // wait for finger removal
        while (ts.touched())
            ;
        if (x > 30 && y > 30) {
            ++newbright;
            tft.setTextSize(2);
            tft.setCursor(cursex, cursey);
            tft.print(newbright);
        }
        if (x < 30 && y > 30) {
            --newbright;
            tft.setTextSize(2);
            tft.setCursor(cursex, cursey);
            tft.print(newbright);
        }
        if (x < 30 && y < 30) {
            Serial.println("leaving now");
            brightness = constrain(newbright, 1, 100);
            done = true;
        }
        if (x > 30 && y < 30) {
            done = true;
        }
    }
}