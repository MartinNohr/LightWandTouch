/*
    This is the Light Wand code moved over to use a graphics touch screen
*/
#include <SPI.h>				// must include this here (or else IDE can't find it)
#include <Adafruit_GFX.h>    // Core graphics library
#include <Wire.h>      // this is needed even though we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>
#include <sdfat.h>
#include <FastLED.h>
#include <timer.h>
#include <avr/eeprom.h>
#include "LightWandTouch.h"

//#include <PDQ_GFX.h>				// PDQ: Core graphics library
//#include "PDQ_ILI9341_config.h"			// PDQ: ILI9341 pins and other setup for this sketch
//#include <PDQ_ILI9341.h>			// PDQ: Hardware-specific driver library
//PDQ_ILI9341 tft;			// PDQ: create LCD object (using pins in "PDQ_ILI9341_config.h")

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

#define SDcsPin 4                        // SD card CS pin

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
#define TFT_BRIGHT 3
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define TIMERSTEPS 20
// this gets called every second/TIMERSTEPS
bool BackLightControl(void*)
{
    static int light;
    static int fade;
    static int timer;
    // don't do anything while writing the file out
    if (bStripWaiting) {
        return;
    }
    // change % to 0-255
    int abslight = 255L * nMaxBackLight / 100;
    if (bTurnOnBacklight) {
        timer = nBackLightSeconds * TIMERSTEPS;
        bBackLightOn = true;
        bTurnOnBacklight = false;
    }
    if (timer > 1) {
        light = abslight;
    }
    else if (timer == 1) {
        // start the fade timer
        fade = abslight / TIMERSTEPS;
    }
    if (bBackLightOn)
        analogWrite(TFT_BRIGHT, light);
    if (timer > 0)
        --timer;
    if (fade) {
        light -= fade;
        if (light < nMinBackLight) {
            light = nMinBackLight;
            bBackLightOn = false;
            analogWrite(TFT_BRIGHT, light);
        }
    }
    return true;    // repeat true
}

// counts the delay for the frame hold time
bool StripDelay(void*)
{
    bTurnOnBacklight = true;
    bStripWaiting = false;
    return false;
}

// counts down by seconds
bool SecondsTimer(void*)
{
    if (nTimerSeconds > 0) {
        --nTimerSeconds;
        return true;
    }
    return false;
}

void ShowWhiteBalance()
{
    //tft.fillRect(10, 10, 100, 200, ILI9341_GREEN);
    //delay(2000);

    // draw a gradient
    int x, y;
    uint16_t r, g, b, yr, yg, yb, buf[320];
    tft.startWrite();
    for (y = 0; y < tft.height(); ++y) {
        for (x = 0; x < tft.width(); ++x) {
            yr = map(y, 0, tft.height(), 255, 0);
            yg = map(y, 0, tft.height(), 0, 255);
            yb = map(y, 0, tft.height(), 255, 0);
            r = map(x, 0, tft.width(), 0, 255);
            g = map(x, 0, tft.width(), 0, 255);
            b = map(x, 0, tft.width(), 255, 0);
            buf[x] = tft.color565(max(r, yr), max(g, yg), max(b, yb));
            //tft.writePixel(x, y, tft.color565(max(r, yr), max(g, yg), max(b, yb)));
            //tft.writeFillRect(10, 10, 100, 200, ILI9341_RED);
        }
        tft.setAddrWindow(0, y, 320, y + 1);
        //tft.setCursor(x, y);
        tft.writePixels(buf, 320, false);
    }
    tft.endWrite();
}

void setup(void) {
    tft.begin();
    delay(50);
    Serial.begin(115200);
    Serial.println(F("Light Wand Touch"));
    tft.fillScreen(ILI9341_BLACK);
    tft.setRotation(1);
    if (!ts.begin()) {
        WriteMessage("Failed starting touch screen", true);
        //Serial.println("Couldn't start touchscreen controller");
        while (1);
    }
    //Serial.println("Touchscreen started");
    pinMode(AuxButton, INPUT_PULLUP);
    if (!SaveSettings(false, true, false) || digitalRead(AuxButton) == LOW) {
        Calibrate();
        SaveSettings(true, bAutoLoadSettings, true);
    }
    tft.setTextColor(ILI9341_BLUE);
    tft.setTextSize(3);
    tft.println("\n\n");
    tft.println(" Light Wand Touch");
    tft.setTextSize(2);
    tft.println("\n");
    tft.println("       Version 1.5");
    tft.println("       Martin Nohr");
    setupSDcard();
    WriteMessage("Testing LED Strip", false, 10);
    // control brightness of screen
    pinMode(TFT_BRIGHT, OUTPUT);
    analogWrite(TFT_BRIGHT, 255);
    digitalWrite(LED_BUILTIN, HIGH);
    EventTimers.every(1000 / TIMERSTEPS, BackLightControl);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, stripLength);
    //FastLED.setTemperature(CRGB(127, 127, 200));
    FastLED.setBrightness(map(nStripBrightness, 0, 100, 0, 255));
    // Turn the LED on, then pause
    leds[0] = leds[1] = CRGB::Red;
    leds[4] = leds[5] = CRGB::Green;
    leds[8] = leds[9] = CRGB::Blue;
    leds[12] = leds[13] = CRGB::White;
    for (int ix = 0; ix < 255; ix += 5) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    for (int ix = 255; ix >= 0; ix -= 5) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    // Now turn the LED off
    FastLED.clear(true);
    // run a white dot up the display and back
    FastLED.setBrightness(map(nStripBrightness, 0, 100, 0, 255));
    for (int ix = 0; ix < stripLength; ++ix) {
        leds[ix] = CRGB(255,255,255);
        if (ix)
            leds[ix - 1] = ILI9341_BLACK;
        FastLED.show();
        delay(1);
    }
    for (int ix = stripLength - 1; ix >= 0; --ix) {
        leds[ix] = CRGB(255, 255, 255);
        if (ix)
            leds[ix + 1] = ILI9341_BLACK;
        FastLED.show();
        delay(1);
    }
    FastLED.clear(true);
    menustack[menuLevel = 0] = MainMenu;
    //ShowWhiteBalance();
    //delay(5000);
}

#define CALIBRATION_TRIES 2
#define CALIBRATION_POINTS 4
// calibrate routine
void Calibrate()
{
    calValues.tsMINX = 10000;
    calValues.tsMAXX = 0;
    calValues.tsMINY = 10000;
    calValues.tsMAXY = 0;
    tft.fillScreen(ILI9341_BLACK);
    for (int ix = 0; ix < CALIBRATION_POINTS * CALIBRATION_TRIES; ++ix) {
        switch (ix % CALIBRATION_POINTS) {
        case 0:
            tft.fillCircle(0, 0, 10, ILI9341_WHITE);
            break;
        case 1:
            tft.fillCircle(tft.width() - 1, 0, 10, ILI9341_WHITE);
            break;
        case 2:
            tft.fillCircle(0, tft.height() - 1, 10, ILI9341_WHITE);
            break;
        case 3:
            tft.fillCircle(tft.width() - 1, tft.height() - 1, 10, ILI9341_WHITE);
            break;
        }
        TS_Point pt;
        while (!ts.touched())
            ;
        do {
            pt = ts.getPoint();
        } while (pt.z < 35 || pt.z > 250);
        switch (ix % CALIBRATION_POINTS) {
        case 0:
            tft.fillCircle(0, 0, 10, ILI9341_BLACK);
            break;
        case 1:
            tft.fillCircle(tft.width() - 1, 0, 10, ILI9341_BLACK);
            break;
        case 2:
            tft.fillCircle(0, tft.height() - 1, 10, ILI9341_BLACK);
            break;
        case 3:
            tft.fillCircle(tft.width() - 1, tft.height() - 1, 10, ILI9341_BLACK);
            break;
        }
        //Serial.println(String(ix / CALIBRATION_POINTS) + String(ix % CALIBRATION_POINTS) + " x=" + String(pt.x) + " y=" + String(pt.y) + " z=" + String(pt.z));
        calValues.tsMINX = min(calValues.tsMINX, pt.x);
        calValues.tsMAXX = max(calValues.tsMAXX, pt.x);
        calValues.tsMINY = min(calValues.tsMINY, pt.y);
        calValues.tsMAXY = max(calValues.tsMAXY, pt.y);
        while (ts.touched())
            pt = ts.getPoint();
    }
    //Serial.println("xmin: " + String(calValues.tsMINX));
    //Serial.println("xmax: " + String(calValues.tsMAXX));
    //Serial.println("ymin: " + String(calValues.tsMINY));
    //Serial.println("ymax: " + String(calValues.tsMAXY));
    delay(20);
}

bool bMenuChanged = true;

void loop()
{
    EventTimers.tick();
    // See if there's any  touch data for us
    //if (ts.bufferEmpty()) {
    //    return;
    //}
    // wait for a touch
    //if (!ts.touched()) {
    //  return;
    //}
    if (bMenuChanged) {
        ShowMenu(menustack[menuLevel]);
        ShowGo();
        bMenuChanged = false;
    }

    TS_Point p = ReadTouch();
    // Retrieve a point  
    //Serial.print("("); Serial.print(p.x);
    //Serial.print(", "); Serial.print(p.y);
    //Serial.println(")");
    // see if one of the go buttons
    delay(100);
    if ((RangeTest(p.x, tft.width() - 40, 30) && RangeTest(p.y, tft.height() - 16, 25))) {
        //Serial.println("GO...");
        ProcessFileOrTest();
        bMenuChanged = true;
        return;
    }
    // see if we got a menu match
    bool skip = false;
    bool skipmatch = false;
    bool gotmatch = false;
    int skipper = 0;    // increment each time we have to skip one so we get the right height
    for (int ix = 0; !gotmatch && menustack[menuLevel][ix].op != eTerminate; ++ix) {
        // see if this is one to skip
        if (menustack[menuLevel][ix].op == eSkipFalse) {
            skipmatch = !*(bool*)menustack[menuLevel][ix].value;
            skip = true;
        }
        else if (menustack[menuLevel][ix].op == eSkipTrue) {
            skipmatch = *(bool*)menustack[menuLevel][ix].value;
            skip = true;
        }
        else if (menustack[menuLevel][ix].op == eClear) {
            skip = true;
        }
        // look for a match
        if (skip) {
            skip = false;
            ++skipper;
            continue;
        }
        if (skipmatch) {
            skipmatch = false;
            ++skipper;
            continue;
        }
        //Serial.println("rangetest: ix=" + String(ix) + " skipper=" + String(skipper));
        if (RangeTest(p.y, (ix + 1 - skipper) * LINEHEIGHT, LINEHEIGHT / 2) && RangeTest(p.x, 0, tft.width() - 50)) {
            gotmatch = true;
            //Serial.println("clicked on menu");
            // got one, service it
            switch (menustack[menuLevel][ix].op) {
            case eText:
            case eTextInt:
            case eTextCurrentFile:
            case eBool:
                if (menustack[menuLevel][ix].function) {
                    //Serial.println(ix);
                    (*menustack[menuLevel][ix].function)(&menustack[menuLevel][ix]);
                    bMenuChanged = true;
                }
                break;
            case eMenu:
                ++menuLevel;
                menustack[menuLevel] = (MenuItem*)(menustack[menuLevel - 1][ix].value);
                bMenuChanged = true;
                break;
            case eExit: // go back a level
                if (menuLevel) {
                    --menuLevel;
                    bMenuChanged = true;
                }
            }
            break;
        }
    }
    // if no match, and we are in a submenu, go back one level
    if (!bMenuChanged && menuLevel) {
        bMenuChanged = true;
        --menuLevel;
    }
}

// run file or built-in
void ProcessFileOrTest()
{
    char line[17];
    DisplayCurrentFile(false);
    if (startDelay) {
        // set a timer
        nTimerSeconds = startDelay;
        EventTimers.every(1000L, SecondsTimer);
        while (nTimerSeconds) {
            bTurnOnBacklight = true;
            //Serial.println("timer " + String(nTimerSeconds));
            tft.setCursor(0, 75);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
            tft.print("Seconds before start: " + String(nTimerSeconds));
            tft.print("   ");
            EventTimers.tick();
            if (CheckCancel())
                break;
            delay(10);
        }
        tft.setCursor(0, 75);
        tft.print("                         ");
    }
    int chainCount = bChainFiles ? FileCountOnly() - CurrentFileIndex : 1;
    int chainRepeatCount = bChainFiles ? nChainRepeats : 1;
    int lastFileIndex = CurrentFileIndex;
    if (bShowBuiltInTests) {
        chainCount = 1;
        chainRepeatCount = 1;
    }
    FastLED.setBrightness(map(nStripBrightness, 0, 100, 0, 255));
    while (chainRepeatCount-- > 0) {
        while (chainCount-- > 0) {
            DisplayCurrentFile(false);
            if (bChainFiles && !bShowBuiltInTests) {
                tft.setCursor(0, 60);
                tft.print("Files: " + String(chainCount) + "  ");
                if (chainRepeatCount > 0) {
                    tft.print("Repeats: " + String(chainRepeatCount) + "  ");
                }
                else {
                    tft.print("            ");
                }
            }
            // process the repeats and waits for each file in the list
            for (int counter = repeatCount; counter > 0; counter--) {
                // fill the progress bar
                ShowProgressBar(0);
                if (repeatCount > 1) {
                    tft.drawRoundRect(0, 95, 240, 24, 10, ILI9341_BLUE);
                    tft.setCursor(7, 100);
                    tft.setTextSize(2);
                    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
                    tft.print("Repeats Left: " + String(counter) + "   ");
                }
                if (bShowBuiltInTests) {
                    // run the test
                    (*BuiltInFiles[CurrentFileIndex].function)();
                }
                else {
                    // output the file
                    SendFile(FileNames[CurrentFileIndex]);
                }
                if (bCancelRun) {
                    break;
                }
                ShowProgressBar(0);
                if (counter > 1) {
                    if (repeatDelay) {
                        FastLED.clear(true);
                        // start timer
                        nTimerSeconds = repeatDelay;
                        EventTimers.every(1000L, SecondsTimer);
                        while (nTimerSeconds) {
                            bTurnOnBacklight = true;
                            tft.setCursor(0, 75);
                            tft.setTextSize(2);
                            tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
                            tft.print("Repeat Seconds Left: " + String(nTimerSeconds));
                            tft.print("   ");
                            delay(1000);
                            EventTimers.tick();
                            if (CheckCancel())
                                break;
                            delay(10);
                        }
                        tft.setCursor(0, 75);
                        tft.print("                           ");
                    }
                }
            }
            if (bCancelRun) {
                chainCount = 0;
                break;
            }
            if (bShowBuiltInTests)
                break;
            // see if we are chaining, if so, get the next file, if a folder we're done
            if (bChainFiles) {
                // grab the next file
                if (CurrentFileIndex < NumberOfFiles - 1)
                    ++CurrentFileIndex;
                if (IsFolder(CurrentFileIndex))
                    break;
            }
            FastLED.clear(true);
        }
        if (bCancelRun) {
            chainCount = 0;
            chainRepeatCount = 0;
            bCancelRun = false;
            break;
        }
        // start again
        CurrentFileIndex = lastFileIndex;
        chainCount = bChainFiles ? FileCountOnly() - CurrentFileIndex : 1;
    }
    if (bChainFiles)
        CurrentFileIndex = lastFileIndex;
    FastLED.clear(true);
}

// show progress bar
void ShowProgressBar(int percent)
{
    static int lastpercent;
    if (lastpercent && (lastpercent == percent))
        return;
    if (percent == 0) {
        tft.fillRect(0, 0, tft.width() - 1, 15, ILI9341_LIGHTGREY);
    }
    else if (percent == 100) {
        tft.fillRect(0, 0, tft.width() - 1, 15, ILI9341_DARKGREY);
    }
    else {
        tft.fillRect(0, 0, (tft.width()) * percent / 100, 15, ILI9341_DARKGREY);
    }
    lastpercent = percent;
}

// save or restore all the settings that are relevant
// this is used when reading the LWC associated with a file
bool SettingsSaveRestore(bool save)
{
    static void* memptr = NULL;
    if (save) {
        // get some memory and save the values
        if (memptr)
            free(memptr);
        memptr = malloc(sizeof saveValueList);
        if (!memptr)
            return false;
    }
    void* blockptr = memptr;
    for (int ix = 0; ix < (sizeof saveValueList / sizeof * saveValueList); ++ix) {
        if (save) {
            memcpy(blockptr, saveValueList[ix].val, saveValueList[ix].size);
        }
        else {
            memcpy(saveValueList[ix].val, blockptr, saveValueList[ix].size);
        }
        blockptr = (void*)((byte*)blockptr + saveValueList[ix].size);
    }
    if (!save) {
        // if it was saved, restore it and free the memory
        if (memptr) {
            free(memptr);
            memptr = NULL;
        }
    }
    return true;
}

void SendFile(String Filename) {
    char temp[14];
    Filename.toCharArray(temp, 14);
    // see if there is an associated config file
    String cfFile = temp;
    cfFile = MakeLWCFilename(cfFile, true);
    SettingsSaveRestore(true);
    ProcessConfigFile(cfFile);
    String fn = currentFolder + temp;
    dataFile.open(fn.c_str(), O_READ);
    // if the file is available send it to the LED's
    if (dataFile.available()) {
        for (int cnt = 0; cnt < (bMirrorPlayImage ? 2 : 1); ++cnt) {
            ReadAndDisplayFile(cnt == 0);
            bReverseImage = !bReverseImage; // note this will be restored by SettingsSaveRestore
            dataFile.rewind();
            if (CheckCancel())
                break;
        }
        dataFile.close();
    }
    else {
        //lcd.clear();
        //lcd.print("* Error reading ");
        //lcd.setCursor(0, 1);
        //lcd.print(CurrentFilename);
        //bBackLightOn = true;
        //delay(1000);
        //lcd.clear();
        //setupSDcard();
        return;
    }
    ShowProgressBar(100);
    SettingsSaveRestore(false);
}

void ReadAndDisplayFile(bool doingFirstHalf) {
    static int totalSeconds;
    if (doingFirstHalf)
        totalSeconds = -1;
#define MYBMP_BF_TYPE           0x4D42
#define MYBMP_BF_OFF_BITS       54
#define MYBMP_BI_SIZE           40
#define MYBMP_BI_RGB            0L
#define MYBMP_BI_RLE8           1L
#define MYBMP_BI_RLE4           2L
#define MYBMP_BI_BITFIELDS      3L

    uint16_t bmpType = readInt();
    uint32_t bmpSize = readLong();
    uint16_t bmpReserved1 = readInt();
    uint16_t bmpReserved2 = readInt();
    uint32_t bmpOffBits = readLong();
    bmpOffBits = 54;

    /* Check file header */
    if (bmpType != MYBMP_BF_TYPE || bmpOffBits != MYBMP_BF_OFF_BITS) {
        WriteMessage("Invalid BMP:\n" + currentFolder + FileNames[CurrentFileIndex], true);
        return;
    }

    /* Read info header */
    uint32_t imgSize = readLong();
    uint32_t imgWidth = readLong();
    uint32_t imgHeight = readLong();
    uint16_t imgPlanes = readInt();
    uint16_t imgBitCount = readInt();
    uint32_t imgCompression = readLong();
    uint32_t imgSizeImage = readLong();
    uint32_t imgXPelsPerMeter = readLong();
    uint32_t imgYPelsPerMeter = readLong();
    uint32_t imgClrUsed = readLong();
    uint32_t imgClrImportant = readLong();

    /* Check info header */
    if (imgSize != MYBMP_BI_SIZE || imgWidth <= 0 ||
        imgHeight <= 0 || imgPlanes != 1 ||
        imgBitCount != 24 || imgCompression != MYBMP_BI_RGB ||
        imgSizeImage == 0)
    {
        WriteMessage("Unsupported, must be 24bpp:\n" + currentFolder + FileNames[CurrentFileIndex], true);
        return;
    }

    int displayWidth = imgWidth;
    if (imgWidth > stripLength) {
        displayWidth = stripLength;           //only display the number of led's we have
    }

    /* compute the line length */
    uint32_t lineLength = imgWidth * 3;
    // fix for padding to 4 byte words
    if ((lineLength % 4) != 0)
        lineLength = (lineLength / 4 + 1) * 4;

    // Note:  
    // The x,r,b,g sequence below might need to be changed if your strip is displaying
    // incorrect colors.  Some strips use an x,r,b,g sequence and some use x,r,g,b
    // Change the order if needed to make the colors correct.

    long secondsLeft = 0, lastSeconds = 0;
    char num[50];
    int percent;
    // note that y is 0 based and x is 0 based in the following code, the original code had y 1 based
    for (int y = bReverseImage ? 0 : imgHeight - 1; bReverseImage ? y < imgHeight : y >= 0; bReverseImage ? ++y : --y) {
        // approximate time left
        if (bReverseImage)
            secondsLeft = ((long)(imgHeight - y) * frameHold / 1000L) + 1;
        else
            secondsLeft = ((long)y * frameHold / 1000L) + 1;
        if (bMirrorPlayImage) {
            if (totalSeconds == -1)
                totalSeconds = secondsLeft;
            if (doingFirstHalf) {
                secondsLeft += totalSeconds;
            }
        }
        if (secondsLeft != lastSeconds) {
            tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
            tft.setTextSize(2);
            tft.setCursor(0, 38);
            lastSeconds = secondsLeft;
            sprintf(num, "%3d seconds", secondsLeft);
            tft.print(num);
        }
        percent = map(bReverseImage ? y : imgHeight - y, 0, imgHeight, 0, 100);
        if (bMirrorPlayImage) {
            percent /= 2;
            if (!doingFirstHalf) {
                percent += 50;
            }
        }
        if (((percent % 5) == 0) || percent > 90) {
            ShowProgressBar(percent);
            //tft.setCursor(0, 50);
            //sprintf(num, "%4d/%5d", y, imgHeight);
            //tft.print(num);
        }
        int bufpos = 0;
        //uint32_t offset = (MYBMP_BF_OFF_BITS + (y * lineLength));
        //dataFile.seekSet(offset);
        for (int x = 0; x < displayWidth; x++) {
            // moved this back here because it might make it possible to reverse scan in the future
            FileSeek((uint32_t)MYBMP_BF_OFF_BITS + ((y * lineLength) + (x * 3)));
            //dataFile.seekSet((uint32_t)MYBMP_BF_OFF_BITS + ((y * lineLength) + (x * 3)));
            getRGBwithGamma();
            // see if we want this one
            if (bScaleHeight && (x * displayWidth) % imgWidth) {
                continue;
            }
            leds[x] = CRGB(r, g, b);
            if (bShowImageDuringOutput) {
                static bool doit = true;
                //static int writercount = 0;
                // output to tft
                int yval = imgWidth - x + tft.height() - stripLength;
                int xval = (imgHeight - y) % tft.width();
                //if (writercount == 0) {
                //    tft.startWrite();
                //    writercount = 5000;
                //}
                // skip every other line if image too tall
                if (imgWidth > tft.height()) {
                    yval = (imgWidth - x) / 2 + tft.height() - stripLength / 2;
                    xval = ((imgHeight - y) / 2) % tft.width();
                    doit = !doit;
                }
                if (doit)
                    tft.drawPixel(xval, yval, tft.color565(r, g, b));
                //if (doit)
                //    tft.writePixel(xval, yval, tft.color565(r, g, b));
                //if (--writercount == 0) {
                //    tft.endWrite();
                //}
            }
        }
        // wait for timer to expire before we show the next frame
        while (bStripWaiting)
            EventTimers.tick();
        // set a timer so we can go ahead and load the next frame
        bStripWaiting = true;
        EventTimers.in(frameHold, StripDelay);
        // now show the lights
        FastLED.show();
        // check keys
        if (CheckCancel())
            break;
    }
    // all done
    readByte(true);
}

uint32_t readLong() {
    uint32_t retValue;
    byte incomingbyte;

    incomingbyte = readByte(false);
    retValue = (uint32_t)((byte)incomingbyte);

    incomingbyte = readByte(false);
    retValue += (uint32_t)((byte)incomingbyte) << 8;

    incomingbyte = readByte(false);
    retValue += (uint32_t)((byte)incomingbyte) << 16;

    incomingbyte = readByte(false);
    retValue += (uint32_t)((byte)incomingbyte) << 24;

    return retValue;
}

uint16_t readInt() {
    byte incomingbyte;
    uint16_t retValue;

    incomingbyte = readByte(false);
    retValue += (uint16_t)((byte)incomingbyte);

    incomingbyte = readByte(false);
    retValue += (uint16_t)((byte)incomingbyte) << 8;

    return retValue;
}

byte filebuf[512];
int fileindex = 0;
int filebufsize = 0;
uint32_t filePosition = 0;

int readByte(bool clear) {
    //int retbyte = -1;
    if (clear) {
        filebufsize = 0;
        fileindex = 0;
        return 0;
    }
    // TODO: this needs to align with 512 byte boundaries
    if (filebufsize == 0 || fileindex >= sizeof(filebuf)) {
        filePosition = dataFile.curPosition();
        //// if not on 512 boundary yet, just return a byte
        //if ((filePosition % 512) && filebufsize == 0) {
        //    //Serial.println("not on 512");
        //    return dataFile.read();
        //}
        // read a block
//        Serial.println("block read");
        do {
            filebufsize = dataFile.read(filebuf, sizeof(filebuf));
        } while (filebufsize < 0);
        fileindex = 0;
    }
    return filebuf[fileindex++];
    //while (retbyte < 0) 
    //    retbyte = dataFile.read();
    //return retbyte;
}

// make sure we are the right place
uint32_t FileSeek(uint32_t place)
{
    if (place < filePosition || place >= filePosition + filebufsize) {
        // we need to read some more
        filebufsize = 0;
        dataFile.seekSet(place);
    }
}

void getRGBwithGamma() {
    if (bGammaCorrection) {
        b = pgm_read_byte(&gammaB[readByte(false)]);
        g = pgm_read_byte(&gammaG[readByte(false)]);
        r = pgm_read_byte(&gammaR[readByte(false)]);
    }
    else {
        b = readByte(false);
        g = readByte(false);
        r = readByte(false);
    }
}

void fixRGBwithGamma(byte* rp, byte* gp, byte* bp) {
    if (bGammaCorrection) {
        *gp = pgm_read_byte(&gammaG[*gp]);
        *bp = pgm_read_byte(&gammaB[*bp]);
        *rp = pgm_read_byte(&gammaR[*rp]);
    }
}

// create the associated LWC name
String MakeLWCFilename(String filename, bool addext)
{
    String cfFile = filename;
    cfFile = cfFile.substring(0, cfFile.lastIndexOf('.'));
    if(addext)
        cfFile += ".LWC";
    return cfFile;
}

void EraseStartFile(MenuItem* menu)
{
    WriteOrDeleteConfigFile("", true, true);
}

void SaveStartFile(MenuItem* menu)
{
    WriteOrDeleteConfigFile("", false, true);
}

void EraseAssociatedFile(MenuItem* menu)
{
    WriteOrDeleteConfigFile(FileNames[CurrentFileIndex].c_str(), true, false);
}

void SaveAssociatedFile(MenuItem* menu)
{
    WriteOrDeleteConfigFile(FileNames[CurrentFileIndex].c_str(), false, false);
}

void LoadAssociatedFile(MenuItem* menu)
{
    String name = FileNames[CurrentFileIndex];
    name = MakeLWCFilename(name, true);
    if (ProcessConfigFile(name)) {
        WriteMessage("Processed:\n" + name);
    }
    else {
        WriteMessage("Failed reading:\n" + name, true);
    }
}

void LoadStartFile(MenuItem* menu)
{
    String name = "START.LWC";
    if (ProcessConfigFile(name)) {
        WriteMessage("Processed:\n" + name);
    }
    else {
        WriteMessage("Failed reading:\n" + name, true);
    }
}

// process the lines in the config file
bool ProcessConfigFile(String filename)
{
    bool retval = true;
    String filepath = currentFolder + filename;
    SdFile rdfile(filepath.c_str(), O_RDONLY);
    if (rdfile.available()) {
        //Serial.println("Processing: " + filepath);
        String line, command, args;
        char buf[100];
        while (rdfile.fgets(buf, sizeof(buf), "\n")) {
            line = String(buf);
            // read the lines and do what they say
            int ix = line.indexOf('=', 0);
            if (ix > 0) {
                command = line.substring(0, ix);
                command.trim();
                command.toUpperCase();
                args = line.substring(ix + 1);
                if (!command.compareTo("PIXELS")) {
                    stripLength = args.toInt();
                    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, stripLength);
                }
                else if (command == "BRIGHTNESS") {
                    nStripBrightness = args.toInt();
                    if (nStripBrightness < 1)
                        nStripBrightness = 1;
                    else if (nStripBrightness > 100)
                        nStripBrightness = 100;
                }
                else if (command == "REPEAT COUNT") {
                    repeatCount = args.toInt();
                }
                else if (command == "REPEAT DELAY") {
                    repeatDelay = args.toInt();
                }
                else if (command == "FRAME TIME") {
                    frameHold = args.toInt();
                }
                else if (command == "START DELAY") {
                    startDelay = args.toInt();
                }
                else if (command == "REVERSE IMAGE") {
                    args.toUpperCase();
                    bReverseImage = args[0] == 'T';
                }
            }
        }
        rdfile.close();
    }
    else
        retval = false;
    return retval;
}

// create the config file, or remove it
// startfile true makes it use the start.lwc file, else it handles the associated name file
bool WriteOrDeleteConfigFile(String filename, bool remove, bool startfile)
{
    bool retval = true;
    String filepath;
    if (startfile) {
        filepath = currentFolder + "START.LWC";
    }
    else {
        filepath = currentFolder + MakeLWCFilename(filename, true);
    }
    if (remove) {
        if (!SD.exists(filepath.c_str()))
            WriteMessage("Not Found:\n" + filepath);
        else if (SD.remove(filepath.c_str())) {
            WriteMessage("Erased:\n" + filepath);
        }
        else {
            WriteMessage("Failed to erase:\n" + filepath, true);
        }
    }
    else {
        SdFile file;
        String line;
        if (file.open(filepath.c_str(), O_WRITE | O_CREAT | O_TRUNC)) {
            line = "PIXELS=" + String(stripLength);
            file.println(line);
            line = "BRIGHTNESS=" + String(nStripBrightness);
            file.println(line);
            line = "REPEAT COUNT=" + String(repeatCount);
            file.println(line);
            line = "REPEAT DELAY=" + String(repeatDelay);
            file.println(line);
            line = "FRAME TIME=" + String(frameHold);
            file.println(line);
            line = "START DELAY=" + String(startDelay);
            file.println(line);
            line = "REVERSE IMAGE=" + String(bReverseImage ? "TRUE" : "FALSE");
            file.println(line);
            file.close();
            WriteMessage("Saved:\n" + filepath);
        }
        else {
            retval = false;
            WriteMessage("Failed to write:\n" + filepath, true);
        }
    }
    return retval;
}

// display message on first line
void WriteMessage(String txt, bool error = false, int wait = 2000)
{
    tft.setCursor(0, 1);
    tft.setTextSize(2);
    tft.fillRect(0, 0, tft.width() - 1, 34, error ? ILI9341_RED : ILI9341_GREEN);
    tft.setTextColor(error ? ILI9341_WHITE : ILI9341_BLACK, error ? ILI9341_RED : ILI9341_GREEN);
    //tft.print(" "); // looks better
    tft.print(txt);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    delay(wait);
}

void ShowGo()
{
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.print((bShowBuiltInTests ? String("*") : currentFolder) + FileNames[CurrentFileIndex].substring(0, FileNames[CurrentFileIndex].lastIndexOf(".")) + " " + String(CurrentFileIndex + 1) + "/" + String(NumberOfFiles));
    tft.fillRoundRect(tft.width() - 52, tft.height() - 52, 49, 49, 10, ILI9341_BLUE);
    tft.fillRoundRect(tft.width() - 50, tft.height() - 50, 45, 45, 10, ILI9341_DARKGREEN);
    tft.setCursor(tft.width() - 40, tft.height() - 34);
    tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREEN);
    tft.print("GO");
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
}

// Top left is origin, down is y, right is x
TS_Point ReadTouch()
{
    // Retrieve a point  
    TS_Point p = ts.getPoint();
    // eat the ones with not enough pressure
    while (p.z < 35 || p.z > 250) {
        p = ts.getPoint();
        EventTimers.tick();
        // return if aux button
        if (digitalRead(AuxButton) == LOW) {
            delay(50);  // debounce
            if (digitalRead(AuxButton) == LOW) {
                // pretend this is the go button
                p.x = calValues.tsMINY + 428;
                p.y = calValues.tsMAXX - 490;
                p.z = 100;
                break;
            }
        }
    }
    bTurnOnBacklight = true;
    //Serial.print("X = "); Serial.print(p.x);
    //Serial.print("\tY = "); Serial.print(p.y);
    //Serial.print("\tPressure = "); Serial.println(p.z);
    // Scale from ~0->4000 to tft.width using the calibration #'s
    int x, y;
    x = map(p.y, calValues.tsMINY, calValues.tsMAXY, 0, tft.width());
    y = map(p.x, calValues.tsMINX, calValues.tsMAXX, tft.height(), 0);

    //Serial.print("("); Serial.print(x);
    //Serial.print(", "); Serial.print(y);
    //Serial.println(")");
    while (ts.touched() || digitalRead(AuxButton) == LOW) {
        EventTimers.tick();
    }
    ts.getPoint();
    p.x = x;
    p.y = y;
    return p;
}

// see if they want to cancel
bool CheckCancel()
{
    static long waitForIt;
    static bool bReadyToCancel = false;
    static bool bCancelPending = false;
    bool retflag = false;
    if (bCancelPending && !bReadyToCancel) {
        bReadyToCancel = true;
        waitForIt = millis();
        return false;
    }
    if (bReadyToCancel && millis() > waitForIt + 5000) {
        bReadyToCancel = false;
        bCancelPending = false;
        tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
        tft.setCursor(5, tft.height() / 2);
        tft.setTextSize(4);
        tft.println("       ");
        tft.println("             ");
    }
    if (ts.bufferEmpty() && digitalRead(AuxButton) == HIGH)
        return false;
    TS_Point point;
    if (digitalRead(AuxButton) == HIGH)
        point = ReadTouch();
    if (!bCancelPending) {
        tft.setCursor(5, tft.height() / 2);
        tft.setTextColor(ILI9341_RED, ILI9341_BLACK);
        tft.setTextSize(4);
        tft.println("Cancel?");
        tft.print("   Yes    No");
        bCancelPending = true;
        bReadyToCancel = false;
        return false;
    }
    if (bReadyToCancel) {
        bCancelPending = false;
        if (digitalRead(AuxButton) == LOW || (RangeTest(point.x, 105, 20) && RangeTest(point.y, 178, 20))) {
            bCancelRun = true;
            retflag = true;
        }
        else {
            tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
            tft.setCursor(5, tft.height() / 2);
            tft.setTextSize(4);
            tft.println("       ");
            tft.println("             ");
        }
    }
    return retflag;
}

// display the menu
void ShowMenu(struct MenuItem* menu)
{
    int y = 0;
    int x = 0;
    char line[100];
    tft.setTextSize(2);
    bool skip = false;
    bool sepline = false;
    // loop through the menu
    while (menu->op != eTerminate) {
        sepline = false;
        if (skip) {
            skip = false;
        }
        else
        {
            switch (menu->op) {
            case eSkipTrue:
                // skip the next one if true
                if (*(bool*)menu->value)
                    skip = true;
                break;
            case eSkipFalse:
                // skip the next one if false
                if (!*(bool*)menu->value)
                    skip = true;
                break;
            case eClear:
                tft.fillScreen(menu->back_color);
                break;
            case eTextInt:
            case eText:
            case eTextCurrentFile:
                // increment displayable lines
                y += LINEHEIGHT;
                tft.setTextColor(ILI9341_WHITE, menu->back_color);
                tft.setCursor(x, y);
                if (menu->value) {
                    if (menu->op == eText)
                        sprintf(line, menu->text, (char*)menu->value);
                    else if (menu->op == eTextInt)
                        sprintf(line, menu->text, *(int*)menu->value);
                    tft.print(line);
                }
                else {
                    if (menu->op == eTextCurrentFile) {
                        sprintf(line, menu->text, MakeLWCFilename(FileNames[CurrentFileIndex], false).c_str());
                        tft.print(line);
                    }
                    else
                        tft.print(menu->text);
                }
                sepline = true;
                break;
            case eBool:
                // increment displayable lines
                y += LINEHEIGHT;
                tft.setTextColor(ILI9341_WHITE, menu->back_color);
                tft.setCursor(x, y);
                if (menu->value) {
                    char line[100];
                    sprintf(line, menu->text, *(bool*)menu->value ? menu->on : menu->off);
                    tft.print(line);
                }
                else {
                    tft.print(menu->text);
                }
                sepline = true;
                break;
            case eMenu:
            case eExit:
                y += LINEHEIGHT;
                tft.setTextColor(ILI9341_WHITE, menu->back_color);
                tft.setCursor(x, y);
                tft.print(menu->text);
                sepline = true;
                break;
            }
        }
        if (sepline)
            tft.drawLine(0, y + 23, tft.width() - 58, y + 23, ILI9341_BLUE);
        ++menu;
    }
}

// switch between SD and built-ins
void ToggleFilesBuiltin(MenuItem* menu)
{
    bool lastval = bShowBuiltInTests;
    ToggleBool(menu);
    if (lastval != bShowBuiltInTests) {
        if (bShowBuiltInTests) {
            CurrentFileIndex = 0;
            NumberOfFiles = 0;
            for (NumberOfFiles = 0; NumberOfFiles < sizeof(BuiltInFiles) / sizeof(*BuiltInFiles); ++NumberOfFiles) {
                // add each one
                FileNames[NumberOfFiles] = String(BuiltInFiles[NumberOfFiles].text);
            }
            currentFolder = "";
        }
        else {
            // read the SD
            currentFolder = "/";
            GetFileNamesFromSD(currentFolder);
        }
    }
}

// toggle a boolean value
void ToggleBool(MenuItem* menu)
{
    bool* pb = (bool*)menu->value;
    *pb = !*pb;
}
// get integer values
void GetIntegerValue(MenuItem* menu)
{
    // some integers needs special processing when changed, handle it here
    int lastlength = stripLength;
    ReadNumberPad((int*)menu->value, menu->min, menu->max, menu->text);
    if (lastlength != stripLength) {
        FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, stripLength);
    }
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
            EventTimers.tick();
        }
        p = ReadTouch();
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
                //Serial.println("previous" + tmp);
                GetFileNamesFromSD(currentFolder);
                EnterFileName(menu);
                return;
            }
            else {
                // just a file, set the current value
                CurrentFileIndex = row;
                done = true;
            }
        }
        else {
            done = true;
        }
        startindex = constrain(startindex, 0, NumberOfFiles > 5 ? NumberOfFiles - 5 : 0);
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
        EventTimers.tick();
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
        p = ReadTouch();
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
            DisplayValueLine(text, result.toInt(), delchars);
        }
        for (int ix = 0; ix < 10; ++ix) {
            // check for match
            if (RangeTest(p.x, numranges[ix].x, numranges[ix].dx) && RangeTest(p.y, numranges[ix].y, numranges[ix].dy)) {
                if (firstdigit) {
                    delchars = result.length();
                    result = String("");
                    firstdigit = false;
                }
                result += String(ix);
                //Serial.println("result: " + result);
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
        //Serial.println("failed to init sd");
        bBackLightOn = true;
        WriteMessage("SD Init failed", true, 5000);
    }
    WriteMessage("Reading SD...", false, 100);
    GetFileNamesFromSD(currentFolder);
}

// read the files from the card
// look for start.lwc, and process it, but don't add it to the list
bool GetFileNamesFromSD(String dir) {
    //Serial.println("getting files from: " + dir);
    String startfile;
    // Directory file.
    SdFile root;
    // Use for files
    SdFile file;
    // start over
    // first empty the current file names
    for (int ix = 0; ix < NumberOfFiles; ++ix) {
        FileNames[ix] = "";
    }
    NumberOfFiles = 0;
    CurrentFileIndex = 0;
    String CurrentFilename = "";

    if (!root.open(dir.c_str())) {
        //Serial.println("open failed: " + dir);
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
    static char buf[20];
    while (file.openNext(&root, O_RDONLY)) {
        if (NumberOfFiles >= MAX_FILES) {
            String str = "Max " + String(MAX_FILES) + String(" files allowed");
            WriteMessage(str, true);
            break;
        }
        if (!file.isHidden() && file.getName(buf, sizeof(buf))) {
            CurrentFilename = String(buf);
            //Serial.println("name: " + CurrentFilename + " len: " + String(CurrentFilename.length()));
            if (file.isDir()) {
                FileNames[NumberOfFiles] = String(NEXT_FOLDER_CHAR) + buf;
                NumberOfFiles++;
            }
            else if (file.isFile()) {
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
    if (startfile.length())
        ProcessConfigFile(startfile);
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

void TestCylon()
{
    CylonBounce(255, 0, 0, 10, frameHold, 50);
}
void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay)
{
    for (int i = 0; i < stripLength - EyeSize - 2; i++) {
        if (CheckCancel())
            return;
        FastLED.clear();
        leds[i] = CRGB(red / 10, green / 10, blue / 10);
        for (int j = 1; j <= EyeSize; j++) {
            leds[i + j] = CRGB(red, green, blue);
        }
        leds[i + EyeSize + 1] = CRGB(red / 10, green / 10, blue / 10);
        FastLED.show();
        delay(SpeedDelay);
    }

    delay(ReturnDelay);

    for (int i = stripLength - EyeSize - 2; i > 0; i--) {
        if (CheckCancel())
            return;
        FastLED.clear();
        leds[i] = CRGB(red / 10, green / 10, blue / 10);
        for (int j = 1; j <= EyeSize; j++) {
            leds[i + j] = CRGB(red, green, blue);
        }
        leds[i + EyeSize + 1] = CRGB(red / 10, green / 10, blue / 10);
        FastLED.show();
        delay(SpeedDelay);
    }
    delay(ReturnDelay);
    FastLED.clear(true);
}

void TestMeteor() {
    meteorRain(255, 255, 255, 10, 64, true, 30);
}

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {
    FastLED.clear(true);

    for (int i = 0; i < stripLength + stripLength; i++) {
        if (CheckCancel())
            return;
        // fade brightness all LEDs one step
        for (int j = 0; j < stripLength; j++) {
            if (CheckCancel())
                return;
            if ((!meteorRandomDecay) || (random(10) > 5)) {
                fadeToBlack(j, meteorTrailDecay);
            }
        }

        // draw meteor
        for (int j = 0; j < meteorSize; j++) {
            if (CheckCancel())
                return;
            if ((i - j < stripLength) && (i - j >= 0)) {
                leds[i - j] = CRGB(red, green, blue);
            }
        }
        FastLED.show();
        delay(SpeedDelay);
    }
}

void fadeToBlack(int ledNo, byte fadeValue) {
    // FastLED
    leds[ledNo].fadeToBlackBy(fadeValue);
}

// running bits
void RunningDot()
{
    for (int colorvalue = 0; colorvalue <= 3; ++colorvalue) {
        if (CheckCancel())
            return;
        // RGBW
        byte r, g, b;
        switch (colorvalue) {
        case 0: // red
            r = 255;
            g = 0;
            b = 0;
            break;
        case 1: // green
            r = 0;
            g = 255;
            b = 0;
            break;
        case 2: // blue
            r = 0;
            g = 0;
            b = 255;
            break;
        case 3: // white
            r = 255;
            g = 255;
            b = 255;
            break;
        }
        //fixRGBwithGamma(&r, &g, &b);
        char line[10];
        for (int ix = 0; ix < stripLength; ++ix) {
            if (CheckCancel())
                return;
            //lcd.setCursor(11, 0);
            //sprintf(line, "%3d", ix);
            //lcd.print(line);
            if (ix > 0) {
                leds[ix - 1] = 0;
            }
            leds[ix] = CRGB(r, g, b);
            FastLED.show();
            delay(frameHold);
        }
        // remember the last one, turn it off
        leds[stripLength - 1] = 0;
        FastLED.show();
    }
}

/*
int* array = calloc(m*n, sizof(int));

array[i*n + j]
*/
// up to 8 bouncing balls
void TestBouncingBalls() {
    byte colors[][3] = {
        {255, 0, 0},
        {255, 255, 255},
        {0, 0, 255},
        {0, 255, 0},
        {255,255,0},
        {0,255,255},
        {255,0,255},
        {128,128,128},
    };

    BouncingColoredBalls(nBouncingBallsCount, colors);
}

void BouncingColoredBalls(int balls, byte colors[][3]) {
    float Gravity = -9.81;
    int StartHeight = 1;

    float* Height = (float*)calloc(balls, sizeof(float));
    float* ImpactVelocity = (float*)calloc(balls, sizeof(float));
    float* TimeSinceLastBounce = (float*)calloc(balls, sizeof(float));
    int* Position = (int*)calloc(balls, sizeof(int));
    long* ClockTimeSinceLastBounce = (long*)calloc(balls, sizeof(long));
    float* Dampening = (float*)calloc(balls, sizeof(float));
    float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);

    for (int i = 0; i < balls; i++) {
        ClockTimeSinceLastBounce[i] = millis();
        Height[i] = StartHeight;
        Position[i] = 0;
        ImpactVelocity[i] = ImpactVelocityStart;
        TimeSinceLastBounce[i] = 0;
        Dampening[i] = 0.90 - float(i) / pow(balls, 2);
    }

    // run for 30 seconds
    long start = millis();
    while (millis() < start + ((long)nBouncingBallsRuntime * 1000)) {
        if (CheckCancel())
            return;
        for (int i = 0; i < balls; i++) {
            if (CheckCancel())
                return;
            TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
            Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / nBouncingBallsDecay, 2.0) + ImpactVelocity[i] * TimeSinceLastBounce[i] / nBouncingBallsDecay;

            if (Height[i] < 0) {
                Height[i] = 0;
                ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
                ClockTimeSinceLastBounce[i] = millis();

                if (ImpactVelocity[i] < 0.01) {
                    ImpactVelocity[i] = ImpactVelocityStart;
                }
            }
            Position[i] = round(Height[i] * (stripLength - 1) / StartHeight);
        }

        for (int i = 0; i < balls; i++) {
            if (CheckCancel())
                return;
            leds[Position[i]] = CRGB(colors[i][0], colors[i][1], colors[i][2]);
        }
        FastLED.show();
        FastLED.clear();
    }
    free(Height);
    free(ImpactVelocity);
    free(TimeSinceLastBounce);
    free(Position);
    free(ClockTimeSinceLastBounce);
    free(Dampening);
}

void OppositeRunningDots()
{
    for (int mode = 0; mode <= 3; ++mode) {
        if (CheckCancel())
            return;
        // RGBW
        byte r, g, b;
        switch (mode) {
        case 0: // red
            r = 255;
            g = 0;
            b = 0;
            break;
        case 1: // green
            r = 0;
            g = 255;
            b = 0;
            break;
        case 2: // blue
            r = 0;
            g = 0;
            b = 255;
            break;
        case 3: // white
            r = 255;
            g = 255;
            b = 255;
            break;
        }
        fixRGBwithGamma(&r, &g, &b);
        for (int ix = 0; ix < stripLength; ++ix) {
            if (CheckCancel())
                return;
            if (ix > 0) {
                leds[ix - 1] = ILI9341_BLACK;
                leds[stripLength - ix + 1] = ILI9341_BLACK;
            }
            leds[stripLength - ix] = CRGB(r, g, b);
            leds[ix] = CRGB(r, g, b);
            FastLED.show();
            delay(frameHold);
        }
        // remember the last one, turn it off
        leds[stripLength - 1] = ILI9341_BLACK;
        FastLED.show();
    }
}

// checkerboard
void CheckerBoard()
{
    byte r, g, b;
    int size = sqrt(stripLength);
    for (int x = 0; x < size * 2; ++x) {
        if (CheckCancel())
            return;
        // one row with BW, and the next WB
        // write pixels alternating white and black
        for (int y = 0; y < stripLength; ++y) {
            if (CheckCancel())
                return;
            r = g = b = ((((y / size) % 2) ^ (x % 2)) & 1) ? 0 : 255;
            fixRGBwithGamma(&r, &g, &b);
            leds[y] = CRGB(r, g, b);
        }
        FastLED.show();
        delay(frameHold);
    }
}

// show random bars of lights with blacks between, 50 times
void RandomBars()
{
    byte r, g, b;
    srand(millis());
    char line[] = "                ";
    //    lcd.setCursor(0, 1);
    //    lcd.write(line, 16);
    for (int pass = 0; pass < 50; ++pass) {
        if (CheckCancel())
            return;
        sprintf(line, "%2d/50", pass + 1);
        //lcd.setCursor(10, 0);
        //lcd.print(line);
        if (pass % 2) {
            // odd numbers, clear
            FastLED.clear();
        }
        else {
            // even numbers, show bar
            r = random(0, 255);
            g = random(0, 255);
            b = random(0, 255);
            fixRGBwithGamma(&r, &g, &b);
            // fill the strip color
            //FastLED.fill(strip.Color(r, g, b), 0, stripLength);
            for (int ix = 0; ix < stripLength; ++ix) {
                leds[ix] = CRGB(r, g, b);
            }
        }
        FastLED.show();
        delay(frameHold);
    }
}

// show random bars of lights, 50 times
void RandomColors()
{
    byte r, g, b;
    srand(millis());
    char line[] = "                ";
    //    lcd.setCursor(0, 1);
    //    lcd.write(line, 16);
    for (int pass = 0; pass < 50; ++pass) {
        if (CheckCancel())
            return;
        sprintf(line, "%2d/50", pass + 1);
        //lcd.setCursor(10, 0);
        //lcd.print(line);
        r = random(0, 255);
        g = random(0, 255);
        b = random(0, 255);
        fixRGBwithGamma(&r, &g, &b);
        // fill the strip color
        //strip.fill(strip.Color(r, g, b), 0, stripLength);
        for (int ix = 0; ix < stripLength; ++ix) {
            leds[ix] = CRGB(r, g, b);
        }
        FastLED.show();
        delay(frameHold);
    }
}

#define BARBERSIZE 10
#define BARBERCOUNT 40
void BarberPole()
{
    CRGB:CRGB color, red, white, blue;
    byte r, g, b;
    r = 255, g = 0, b = 0;
    fixRGBwithGamma(&r, &g, &b);
    red = CRGB(r, g, b);
    r = 255, g = 255, b = 255;
    fixRGBwithGamma(&r, &g, &b);
    white = CRGB(r, g, b);
    r = 0, g = 0, b = 255;
    fixRGBwithGamma(&r, &g, &b);
    blue = CRGB(r, g, b);
    //ShowProgressBar(0);
    for (int loop = 0; loop < (4 * BARBERCOUNT); ++loop) {
        //Serial.println("barber:" + String(loop));
        //ShowProgressBar(loop * 100 / 4 * BARBERCOUNT);
        if (CheckCancel())
            return;
        for (int ledIx = 0; ledIx < stripLength; ++ledIx) {
            if (CheckCancel())
                return;
            // figure out what color
            switch (((ledIx + loop) % BARBERCOUNT) / BARBERSIZE) {
            case 0: // red
                color = red;
                break;
            case 1: // white
            case 3:
                color = white;
                break;
            case 2: // blue
                color = blue;
                break;
            }
            leds[ledIx] = color;
        }
        FastLED.show();
        delay(frameHold);
    }
    //ShowProgressBar(100);
}

void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne) {
    FastLED.clear();
    byte brightness = (255 * nStripBrightness) / 100;

    for (int i = 0; i < Count; i++) {
        if (CheckCancel())
            return;
        leds[random(stripLength)] = CRGB(random(0, brightness), random(0, brightness), random(0, brightness));
        FastLED.show();
        delay(SpeedDelay);
        if (OnlyOne) {
            FastLED.clear();
        }
    }
    delay(SpeedDelay);
}

void TestTwinkle() {
    TwinkleRandom(200, frameHold, false);
}

// save some settings in the eeprom
// if autoload is true, check the first flag, and load the rest if it is true
// return true if valid, false if failed
// note that the calvalues must always be read
bool SaveSettings(bool save, bool autoload, bool onlyCalvalues)
{
    void* blockpointer = (void*)NULL;
    for (int ix = 0; ix < (sizeof saveValueList / sizeof * saveValueList); ++ix) {
        //Serial.println("savesettings ix:" + String(ix));
        if (save) {
            eeprom_write_block(saveValueList[ix].val, blockpointer, saveValueList[ix].size);
            //Serial.println("xmin:" + String(calValues.tsMINX) + " xmax:" + String(calValues.tsMAXX) + " ymin:" + String(calValues.tsMINY) + " ymax:" + String(calValues.tsMINY));
            if (ix == 2 && onlyCalvalues) {  // the calvalues
                //Serial.println("saving only calvalues");
                return true;
            }
        }
        else {  // load
            // check signature
            char svalue[sizeof signature];
            eeprom_read_block(svalue, (void*)NULL, sizeof svalue);
            if (strncmp(svalue, signature, sizeof signature)) {
                WriteMessage("bad eeprom signature\nSave Default to fix", true);
                return false;
            }
            eeprom_read_block(saveValueList[ix].val, blockpointer, saveValueList[ix].size);
            if (ix == 2 && onlyCalvalues) {   // the calvalues
                return true;
            }
            // if autoload, exit if the save value is not true
            if (autoload && ix == 2) {  // we use 2 here so that the signature and the calvalues are read
                if (!bAutoLoadSettings) {
                    return true;
                }
            }
        }
        blockpointer = (void*)((byte*)blockpointer + saveValueList[ix].size);
    }
    if (!save) {
        int savedFileIndex = CurrentFileIndex;
        // we don't know the folder path, so just reset the folder level
        currentFolder = "/";
        setupSDcard();
        CurrentFileIndex = savedFileIndex;
        // make sure file index isn't too big
        if (CurrentFileIndex >= NumberOfFiles) {
            CurrentFileIndex = 0;
        }
    }
    WriteMessage(save ? "Settings Saved" : "Settings Loaded");
    return true;
}

// save the eeprom settings
void SaveEepromSettings(MenuItem* menu)
{
    SaveSettings(true, false, false);
}

// load eeprom settings
void LoadEepromSettings(MenuItem* menu)
{
    SaveSettings(false, true, false);
}

// count the actual files
int FileCountOnly()
{
    int count = 0;
    // ignore folders, at the end
    for (int files = 0; files < NumberOfFiles; ++files) {
        if (!IsFolder(count))
            ++count;
    }
    return count;
}

// return true if current file is folder
bool IsFolder(int index)
{
    return FileNames[index][0] == NEXT_FOLDER_CHAR
        || FileNames[index][0] == PREVIOUS_FOLDER_CHAR;
}

// show the current file
void DisplayCurrentFile(bool top)
{
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, top ? 0 : 15);
    tft.print(currentFolder + FileNames[CurrentFileIndex]);
}
