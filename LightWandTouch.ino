/*
    This is the Light Wand code moved over to use a graphics touch screen
*/
// set this to calibrate the touch screen
#define CALIBRATE 0
#include "LightWandTouch.h"

#define TIMERSTEPS 10
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
    delay(1000);
    // control brightness of screen
    pinMode(TFT_BRIGHT, OUTPUT);
    //analogWrite(TFT_BRIGHT, 10);
    //delay(2000);
    analogWrite(TFT_BRIGHT, 255);
    //delay(2000);
    digitalWrite(LED_BUILTIN, HIGH);
    //SaveSettings(false, true);
    EventTimers.every(1000 / TIMERSTEPS, BackLightControl);
#endif
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(nStripBrightness * 255L / 100);
    //FastLED.setBrightness(255L / (101 - nStripBrightness));
    // Turn the LED on, then pause
    leds[0] = leds[1] = CRGB::Red;
    leds[4] = leds[5] = CRGB::Green;
    leds[8] = leds[9] = CRGB::Blue;
    leds[12] = leds[13] = CRGB::White;
    for (int ix = 0; ix < 255; ++ix) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    for (int ix = 255; ix; --ix) {
        FastLED.setBrightness(ix);
        FastLED.show();
    }
    // Now turn the LED off
    FastLED.clear(true);
}

bool bMenuChanged = true;

void loop()
{
    EventTimers.tick();
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
    TS_Point p = ReadTouch();
    Serial.print("("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");
    // see if the go button
    if (RangeTest(p.x, tft.width() - 40, 25) && RangeTest(p.y, tft.height() - 16, 20)) {
        //Serial.println("GO...");
        tft.fillScreen(ILI9341_BLUE);
        tft.fillRect(0, 0, tft.width() - 1, 10, ILI9341_LIGHTGREY);
        tft.setCursor(0, 12);
        tft.print(currentFolder + FileNames[CurrentFileIndex]);
        //for (int x = 0; x <= 100; ++x) {
        //    int wide = map(x, 0, 100, 0, tft.width() - 1);
        //    tft.fillRect(0, 0, wide, 10, ILI9341_DARKGREY);
        //    if (ts.touched()) {
        //        ReadTouch(false);
        //        break;
        //    }
        //    delay(100);
        //}
        //delay(500);
        ProcessFileOrTest(0);
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
            case eBool:
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
    // if no match, and we are in a submenu, go back one level
    if (!bMenuChanged && menuLevel) {
        bMenuChanged = true;
        currentMenu = menustack[--menuLevel];
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

// run file or built-in
void ProcessFileOrTest(int chainnumber)
{
    char line[17];
    if (startDelay) {
        for (int seconds = startDelay; seconds; --seconds) {
            //lcd.setCursor(0, 0);
            //sprintf(line, "Wait: %d", seconds);
            //lcd.print(line);
            delay(1000);
        }
    }
    FastLED.setBrightness(255L * nStripBrightness / 100);
    for (int counter = repeatCount; counter > 0; counter--) {
        //if (chainnumber) {
        //    lcd.setCursor(13, 1);
        //    char line[10];
        //    sprintf(line, "%2d", chainnumber);
        //    lcd.print(line);
        //}
        //lcd.setCursor(0, 0);
        // only display if a file
        char first = FileNames[CurrentFileIndex][0];
        if (bShowBuiltInTests) {
            // run the test
            (*BuiltInFiles[CurrentFileIndex].function)();
        }
        else {
            // output the file
            SendFile(FileNames[CurrentFileIndex]);
        }
        if (bCancelRun) {
            bCancelRun = false;
            break;
        }
        if (counter > 1) {
            //lcd.clear();
            //lcd.setCursor(0, 0);
            //lcd.print("Repeat delay...");
            //if (repeatDelay) {
            //    FastLED.clear();
            //    FastLED.show();
            //    delay(repeatDelay);
            //}
        }
    }
    FastLED.clear(true);
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

// save some settings in the eeprom
// if autoload is true, check the first flag, and load the rest if it is true
void SaveSettings(bool save, bool autoload)
{
    void* blockpointer = (void*)NULL;
    for (int ix = 0; ix < (sizeof saveValueList / sizeof * saveValueList); ++ix) {
        if (save) {
            eeprom_write_block(saveValueList[ix].val, blockpointer, saveValueList[ix].size);
        }
        else {  // load
            // check signature
            char svalue[sizeof signature];
            eeprom_read_block(svalue, (void*)NULL, sizeof svalue);
            if (strncmp(svalue, signature, sizeof signature)) {
                //lcd.clear();
                //lcd.setCursor(0, 0);
                //lcd.print("bad signature");
                //lcd.setCursor(0, 1);
                //lcd.print(svalue);
                //delay(1000);
                return;
            }
            eeprom_read_block(saveValueList[ix].val, blockpointer, saveValueList[ix].size);
            // if autoload, exit if the save value is not true
            if (autoload && ix == 0) {
                if (!bAutoLoadSettings) {
                    return;
                }
            }
        }
        blockpointer = (void*)((byte*)blockpointer + saveValueList[ix].size);
    }
    if (!save) {
        int savedFileIndex = CurrentFileIndex;
        //// we don't know the folder path, so just reset the folder level
        //folderLevel = 0;
        setupSDcard();
        CurrentFileIndex = savedFileIndex;
        // make sure file index isn't too big
        if (CurrentFileIndex >= NumberOfFiles) {
            CurrentFileIndex = 0;
        }
        //// check test number also
        //if (nTestNumber >= MAXTEST) {
        //    nTestNumber = 0;
        //}
    }
    //lcd.clear();
    //lcd.setCursor(0, 0);
    //lcd.print(save ? "Settings Saved" : "Settings Loaded");
    delay(1000);
}

void SendFile(String Filename) {
    char temp[14];
    Filename.toCharArray(temp, 14);
    // see if there is an associated config file
    String cfFile = temp;
    cfFile = MakeLWCFilename(cfFile);
    SettingsSaveRestore(true);
    ProcessConfigFile(cfFile);
    String fn = currentFolder + temp;
    dataFile.open(fn.c_str(), O_READ);
    // if the file is available send it to the LED's
    if (dataFile.available()) {
        ReadAndDisplayFile();
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
    SettingsSaveRestore(false);
}

void ReadAndDisplayFile() {
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
        //lcd.setCursor(0, 0);
        //lcd.print("not a bitmap");
        //delay(1000);
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
        //lcd.setCursor(0, 0);
        //lcd.print("Unsupported");
        //lcd.setCursor(0, 1);
        //lcd.print("Bitmap Use 24bpp");
        //delay(1000);
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
    char num[8];
    for (int y = imgHeight; y > 0; y--) {
        // approximate time left
        secondsLeft = ((long)y * frameHold / 1000L) + 1;
        if (secondsLeft != lastSeconds) {
            //lcd.setCursor(11, 0);
            //lastSeconds = secondsLeft;
            //sprintf(num, "%3d S", secondsLeft);
            //lcd.print(num);
        }
        if ((y % 10) == 0) {
            //lcd.setCursor(12, 1);
            //sprintf(num, "%4d", y);
            //lcd.print(num);
        }
        int bufpos = 0;
        //uint32_t offset = (MYBMP_BF_OFF_BITS + ((y - 1) * lineLength));
        //dataFile.seekSet(offset);
        for (int x = 0; x < displayWidth; x++) {
            // moved this back here because it might make it possible to reverse scan in the future
            FileSeek((uint32_t)MYBMP_BF_OFF_BITS + (((y - 1) * lineLength) + (x * 3)));
            //dataFile.seekSet((uint32_t)MYBMP_BF_OFF_BITS + (((y - 1) * lineLength) + (x * 3)));
            getRGBwithGamma();
            // see if we want this one
            if (bScaleHeight && (x * displayWidth) % imgWidth) {
                continue;
            }
            leds[x] = CRGB(r, g, b);
        }
        // wait for timer to expire before we show the next frame
        while (bStripWaiting)
            EventTimers.tick();
        FastLED.show();
        delay(frameHold);
        bStripWaiting = true;
        // set a timer so we can go ahead and load the next frame
        EventTimers.in(frameHold, StripDelay);
        // check keys
        //if (CheckCancel())
        //    break;
    }
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
        filebufsize = dataFile.read(filebuf, sizeof(filebuf));
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
        b = gammaB[readByte(false)];
        g = gammaG[readByte(false)];
        r = gammaR[readByte(false)];
    }
    else {
        b = readByte(false);
        g = readByte(false);
        r = readByte(false);
    }
}

void fixRGBwithGamma(byte* rp, byte* gp, byte* bp) {
    if (bGammaCorrection) {
        *gp = gammaG[*gp];
        *bp = gammaB[*bp];
        *rp = gammaR[*rp];
    }
}

// create the associated LWC name
String MakeLWCFilename(String filename)
{
    String cfFile = filename;
    cfFile = cfFile.substring(0, cfFile.lastIndexOf('.') + 1);
    cfFile += "LWC";
    return cfFile;
}

// process the lines in the config file
bool ProcessConfigFile(String filename)
{
    bool retval = true;
    String filepath = currentFolder + filename;
    SdFile rdfile(filepath.c_str(), O_RDONLY);
    if (rdfile.available()) {
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
                    //FastLED.updateLength(stripLength);
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
            }
        }
    }
    else
        retval = false;
    return retval;
}

// create the config file, or remove it
bool WriteOrDeleteConfigFile(String filename, bool remove)
{
    bool retval = true;
    String name = MakeLWCFilename(filename);
    String filepath = currentFolder + name;
    if (remove) {
        SD.remove(filepath.c_str());
    }
    else {
        SdFile file(filepath.c_str(), O_READ | O_WRITE | O_CREAT | O_TRUNC);
        String line;
        if (file.availableForWrite()) {
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
            file.close();
        }
        else
            retval = false;
    }
    return retval;
}

void ShowGo()
{
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
    tft.print((bShowBuiltInTests ? String("*") : currentFolder) + FileNames[CurrentFileIndex].substring(0, FileNames[CurrentFileIndex].lastIndexOf(".")) + " " + String(CurrentFileIndex + 1) + "/" + String(NumberOfFiles));
    tft.fillRoundRect(tft.width() - 50, tft.height() - 50, 45, 45, 10, ILI9341_DARKGREEN);
    tft.setCursor(tft.width() - 40, tft.height() - 34);
    tft.setTextColor(ILI9341_WHITE, ILI9341_DARKGREEN);
    tft.print("GO");
}

// Top left is origin, down is y, right is x
TS_Point ReadTouch()
{
    // Retrieve a point  
    TS_Point p = ts.getPoint();
    // eat the ones with not enough pressure
    while (p.z < 35 || p.z > 250) {
        //while (p.z < 35 || p.z > 250 || p.y < 100 || p.x < 100) {
        p = ts.getPoint();
        EventTimers.tick();
    }
    bTurnOnBacklight = true;
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
        EventTimers.tick();
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
            // increment displayable lines
            y += LINEHEIGHT;
            tft.setTextColor(ILI9341_WHITE, menu->back_color);
            tft.setCursor(x, y);
            if (menu->value) {
                char line[100];
                if (menu->op == eText)
                    sprintf(line, menu->text, (char*)menu->value);
                else
                    sprintf(line, menu->text, *(int*)menu->value);
                tft.print(line);
            }
            else {
                tft.print(menu->text);
            }
            break;
        case eBool:
            // increment displayable lines
            y += LINEHEIGHT;
            tft.setTextColor(ILI9341_WHITE, menu->back_color);
            tft.setCursor(x, y);
            if (menu->value) {
                char line[100];
                sprintf(line, menu->text, *(bool*)menu->value?menu->on:menu->off);
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
            currentMenu = MainMenuInternal;
        }
        else {
            // read the SD
            GetFileNamesFromSD(currentFolder);
            currentMenu = MainMenu;
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
                Serial.println("previous" + tmp);
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
            file.getName(buf, sizeof(buf));
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

void TestCylon()
{
    CylonBounce(255, 0, 0, 10, frameHold, 50);
}
void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay)
{
    for (int i = 0; i < stripLength - EyeSize - 2; i++) {
        //if (CheckCancel())
        //    return;
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
        //if (CheckCancel())
        //    return;
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
        //if (CheckCancel())
        //    return;
        // fade brightness all LEDs one step
        for (int j = 0; j < stripLength; j++) {
            //if (CheckCancel())
            //    return;
            if ((!meteorRandomDecay) || (random(10) > 5)) {
                fadeToBlack(j, meteorTrailDecay);
            }
        }

        // draw meteor
        for (int j = 0; j < meteorSize; j++) {
            //if (CheckCancel())
            //    return;
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
        //if (CheckCancel())
        //    return;
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
            //if (CheckCancel())
            //    return;
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
        //if (CheckCancel())
        //    return;
        for (int i = 0; i < balls; i++) {
            //if (CheckCancel())
            //    return;
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
            //if (CheckCancel())
                //return;
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
