/**
* CREDITS. Parts of this code is based on:
* Optimized ILI9341 screen driver library for Teensy 4/4.1, with vsync and differential updates: <https://github.com/vindar/ILI9341_T4>
* TGX - a tiny/teensy graphics library: <https://github.com/vindar/tgx>
* Example 'T3TRIS' from FrankBoesing: <https://github.com/FrankBoesing/T3TRIS>
*
* Used development software:
* Arduino IDE 1.8.15
* Teensyduino, Version 1.54
**/

// set the pins: here for SPI0 on Teensy 4.x
// ***  Recall that DC must be on a valid cs pin !!! ***
#define PIN_SCK               13  // (needed) SCK pin for SPI0 on Teensy 4.0
#define PIN_MISO              12  // (needed) MISO pin for SPI0 on Teensy 4.0
#define PIN_MOSI              11  // (needed) MOSI pin for SPI0 on Teensy 4.0
#define PIN_DC                10  // (needed) CS pin for SPI0 on Teensy 4.0
#define PIN_RESET              6  // (needed) any pin can be used 
#define PIN_CS                 9  // (needed) any pin can be used
#define PIN_BACKLIGHT          5  // only required if LED pin from screen is connected to Teensy 
#define PIN_TOUCH_IRQ        255  // 255 if touch not connected
#define PIN_TOUCH_CS         255  // 255 if touch not connected
#define SPI_SPEED       30000000  // SPI speed

#include "ILI9341Driver.h"
#include <tgx.h> 
#include "font_DSEG14Classic_Bold.h"
#include "tetris_bg.h"

// namespace for draw graphics primitives
using namespace tgx;

// framebuffers
DMAMEM uint16_t ib[240 * 320] = { 0 };  // used for internal buffering
DMAMEM uint16_t fb[240 * 320] = { 0 };  // paint in this buffer

// the screen driver object
ILI9341_T4::ILI9341Driver tft(PIN_CS, PIN_DC, PIN_SCK, PIN_MOSI, PIN_MISO, PIN_RESET, PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// two diff buffers
ILI9341_T4::DiffBuffStatic<6000> diff1;
ILI9341_T4::DiffBuffStatic<6000> diff2;

// images that encapsulates framebuffer
Image<RGB565> im(fb, 240, 320);
Image<RGB565> bg(tetris_bg, 240, 320);

#include "blocks.h"

#define SPEED_START 500
#define SPEED_MAX   200

RGB32 color_gamma[3][NUMCOLORS];
uint8_t field[FIELD_WIDTH][FIELD_HIGHT];
uint16_t aSpeed = 0, score = 0, highscore = 0;
int8_t nBlock = 0, nColor = 0, nRotation = 0; // next Block
int8_t aBlock = 0, aColor = 0, aX = 0, aY = 0, aRotation = 0; // active Block
int dX = 0;

/// <summary>
/// setup
/// </summary>
void setup() {
    for (unsigned i = 1; i < NUMCOLORS; i++) {
        color_gamma[0][i] = colgamma(color[i], 30);
        color_gamma[1][i] = colgamma(color[i], -70);
        color_gamma[2][i] = colgamma(color[i], -35);
    }

    // display
    while (!tft.begin(SPI_SPEED)) delay(1000);

    tft.setScroll(0);
    tft.setRotation(0);
    tft.setFramebuffers(ib);             // set 1 internal framebuffer -> activate float buffering
    tft.setDiffBuffers(&diff1, &diff2);  // set the 2 diff buffers => activate differential updates 
    tft.setDiffGap(4);                   // use a small gap for the diff buffers
    tft.setRefreshRate(90);              // around 90hz for the display refresh rate 
    tft.setVSyncSpacing(0);

    // make sure backlight is on
    if (PIN_BACKLIGHT != 255) {
        pinMode(PIN_BACKLIGHT, OUTPUT);
        digitalWrite(PIN_BACKLIGHT, HIGH);
    }

    highscore = 0;
    nextBlock();

    im.fillScreen(RGB565_Black);
    im.copyFrom(bg);
    tft.update(fb, true);
    delay(2000);

    initGame();
    printScore();
    printHighScore();
}

/// <summary>
/// init game
/// </summary>
void initGame() {
    score = 0;
    aSpeed = SPEED_START;
    initField();
}

/// <summary>
/// init field
/// </summary>
void initField() {
    memset(field, 0, sizeof(field));
    im.fillRect(FIELD_X, FIELD_Y, FIELD_WIDTH * PIX, FIELD_HIGHT * PIX, color[0]);
}

/// <summary>
/// center text horizontaly
/// </summary>
/// <param name="img"></param>
/// <param name="text"></param>
/// <param name="y"></param>
/// <param name="w"></param>
/// <param name="color"></param>
/// <param name="font"></param>
void drawTextCenterX(Image<RGB565> img, const char* text, int y, int w, RGB565 color, const ILI9341_t3_font_t& font) {
    auto b = im.measureText(text, { 0,0 }, font, false);
    img.drawText(text, iVec2((w / 2) - b.lx() / 2, y), color, font, false);
}

/// <summary>
/// draw a long integer
/// </summary>
/// <param name="img"></param>
/// <param name="long_num"></param>
/// <param name="poX"></param>
/// <param name="poY"></param>
/// <param name="bg"></param>
/// <param name="color"></param>
/// <param name="fnt"></param>
void printNum(Image<RGB565> img, long long_num, int poX, int poY, bool bg, RGB565 color, const ILI9341_t3_font_t& font) {
    char str[14];
    ltoa(long_num, str, 10);
    if (bg) {
        iBox2 b = im.measureText(str, { poX, poY }, font, false);
        im.fillRectVGradient(iBox2(poX - 3, poX + 85, b.minY - 3, b.maxY + 3), RGB565_Gray, RGB565_Black);
    }
    img.drawText(str, iVec2(poX, poY), color, font, false);
}

/// <summary>
/// print score
/// </summary>
void printScore() {
    printNum(im, score, 50, 26, true, RGB565_Lime, font_DSEG14Classic_Bold_18);
}

/// <summary>
/// print highscore
/// </summary>
void printHighScore() {
    printNum(im, highscore, 145, 26, true, RGB565_Yellow, font_DSEG14Classic_Bold_18);
}

/// <summary>
/// print game over
/// </summary>
void printGameOver() {
    drawTextCenterX(im, "GAME", 150, 240, RGB565_Lime, font_DSEG14Classic_Bold_36);
    drawTextCenterX(im, "OVER", 190, 240, RGB565_Lime, font_DSEG14Classic_Bold_36);
    tft.update(fb, false);
    delay(3000);
}

/// <summary>
/// print start game
/// </summary>
void printStartGame() {
    char str[14];
    for (int i = 3; i > 0; i--) {
        im.fillRect(50, 120, 140, 70, RGB565_Black);
        ltoa(i, str, 10);
        drawTextCenterX(im, str, 170, 240, RGB565_Lime, font_DSEG14Classic_Bold_36);
        tft.update(fb, false);
        delay(500);
    }

    im.fillRect(50, 120, 140, 70, RGB565_Black);
    drawTextCenterX(im, "GO", 170, 240, RGB565_Lime, font_DSEG14Classic_Bold_36);
    tft.update(fb, false);
    delay(500);
    initField();
}

/// <summary>
/// main loop
/// </summary>
/// <param name=""></param>
void loop(void) {
    bool r = false;
    int c = 0;
    int c1 = 0;

    while (!r) {
        if (++c == 8) {
            eff();
            c = 0;
        }
        if (++c1 == 50) {
            c1 = 0;
        }
        r = game(true);
    }
    game(false);
}

/// <summary>
/// game loop
/// </summary>
/// <param name="demoMode"></param>
/// <returns></returns>
bool game(bool demoMode) {
    bool gameOver = false;
    static bool pause = false;
    int tk = 0;

    initGame();
    if (!demoMode) printStartGame();

    nextBlock();
    drawBlockSmall(true);
    drawBlock(aBlock, aX, aY, aRotation, aColor);

    do {
        yield();
        int t = millis();

        if (!demoMode) do {  // process controls
            if (millis() - tk > aSpeed / 9) {
                char ch = controls();
                if (ch != '\0') tk = millis();

                switch (ch) {
                case 'r': // reset
                    score = 0;
                    printScore();
                    return true;

                case 'p': // pause
                    pause = !pause;
                    break;

                case 's': // down
                    t = 0;
                    break;

                case '+': // rotate
                    if (checkMoveBlock(0, 0, 1)) {
                        int oldaRotation = aRotation;
                        aRotation = (aRotation + 1) & 0x03;
                        drawBlockEx(aBlock, aX, aY, aRotation, aColor, aX, aY, oldaRotation);
                    }
                    break;

                case 'a': // left
                case 'd': // right
                    int dX = (ch == 'd') ? 1 : -1;
                    if (checkMoveBlock(dX, 0, 0)) {
                        drawBlockEx(aBlock, aX + dX, aY, aRotation, aColor, aX, aY, aRotation);
                        aX += dX;
                    }
                    break;
                }
            }
            yield();
        } while (millis() - t < aSpeed);
        else { // demoMode
            delay(5);
            char ch = controls();
            if (ch != '\0')  return true;
        }

        if (!pause) {
            // move the block down
            bool movePossible = checkMoveBlock(0, 1, 0);
            if (movePossible) {
                drawBlockEx(aBlock, aX, aY + 1, aRotation, aColor, aX, aY, aRotation);
                aY++;
                score++;
            } else { // block stopped moving down
                // store location
                setBlock();
                checkLines();
                // get new block and draw it
                score += 10;
                nextBlock();
                drawBlockSmall(true);
                drawBlock(aBlock, aX, aY, aRotation, aColor);
                if (!checkMoveBlock(0, 0, 0)) {
                    // game over
                    initField();
                    gameOver = true;
                }
            }
        }

        if (!demoMode) printScore();

    } while (!gameOver);

    if (!demoMode) {
        if (score > highscore) {
            highscore = score;
            printHighScore();
        }
        printGameOver();
    }
    drawBlockSmall(false);
    return false;
}

/// <summary>
///  set block
/// </summary>
void setBlock() {
    int bH = BLOCKHIGHT(aBlock, aRotation);
    int bW = BLOCKWIDTH(aBlock, aRotation);

    for (int y = 0; y < bH; y++) {
        for (int x = 0; x < bW; x++) {
            if ((block[aRotation][aBlock][y * 4 + x + 2] > 0)) {
                field[x + aX][y + aY] = aColor;
            }
        }
    }
}

/// <summary>
/// check lines
/// </summary>
void checkLines() {
    int x, y, c, i;
    for (y = 0; y < FIELD_HIGHT; y++) {
        c = 0;
        for (x = 0; x < FIELD_WIDTH; x++) {
            if (field[x][y] > 0) c++;
        }

        // complete line
        if (c >= FIELD_WIDTH) {
            // line-effect
            for (i = NUMCOLORS - 1; i >= 0; i--) {
                for (x = 0; x < FIELD_WIDTH; x++) {
                    drawBlockPix(FIELD_X + x * PIX, FIELD_Y + y * PIX, i);
                }
                delay(60);
            }

            // move entire field above complete line down and redraw
            for (i = y; i > 0; i--)
                for (x = 0; x < FIELD_WIDTH; x++)
                    field[x][i] = field[x][i - 1];
            for (x = 0; x < FIELD_WIDTH; x++)
                field[x][0] = 0;

            drawField();
            if (aSpeed > SPEED_MAX) aSpeed -= 5;
        }
    }
}

/// <summary>
/// check moved block
/// </summary>
/// <param name="deltaX"></param>
/// <param name="deltaY"></param>
/// <param name="deltaRotation"></param>
/// <returns></returns>
bool checkMoveBlock(int deltaX, int deltaY, int deltaRotation) {
    int rot = (aRotation + deltaRotation) & 0x03;
    int bH = BLOCKHIGHT(aBlock, rot);
    int dY = aY + deltaY;

    if (dY + bH > FIELD_HIGHT)  // lower border
        return false;

    int bW = BLOCKWIDTH(aBlock, rot);
    int dX = aX + deltaX;

    if (dX < 0 || dX + bW > FIELD_WIDTH) { // left/right border
        return false;
    }

    for (int y = bH - 1; y >= 0; y--) {
        for (int x = 0; x < bW; x++) {
            if ((field[x + dX][y + dY] > 0) && (block[rot][aBlock][y * 4 + x + 2] > 0)) {
                return false;
            }
        }
    }

    return true;
}

/// <summary>
/// next block
/// </summary>
void nextBlock() {
    aColor = nColor;
    aBlock = nBlock;
    aRotation = nRotation;
    aY = 0;
    aX = random(FIELD_WIDTH - BLOCKWIDTH(aBlock, aRotation) + 1);

    nColor = random(1, NUMCOLORS);
    nBlock = random(NUMBLOCKS);
    nRotation = random(4);
}

/// <summary>
/// effect
/// </summary>
void eff() {
    int t = millis();
    do {
        nextBlock();
        drawBlock(aBlock, aX, random(FIELD_HIGHT), aRotation, aColor);
    } while (millis() - t < 1000);
}

/// <summary>
/// set gamma
/// </summary>
/// <param name="color"></param>
/// <param name="gamma"></param>
/// <returns></returns>
RGB32 colgamma(RGB32 color, int16_t gamma) {
    return  RGB32(
        constrain(((color.R) & 0xf8) + gamma, 0, 255),  //r
        constrain(((color.G) & 0xfc) + gamma, 0, 255),  //g
        constrain(((color.B) & 0xf8) + gamma, 0, 255)); //b
}

/// <summary>
///  draw block pix
/// </summary>
/// <param name="px"></param>
/// <param name="py"></param>
/// <param name="col"></param>
void drawBlockPix(int px, int py, int col) {
    if (px >= FIELD_XW) return;
    if (px < FIELD_X) return;
    if (py >= FIELD_YW) return;
    if (py < FIELD_Y) return;

    if (col == 0) {
        //remove Pix, draw backgroundcolor
        im.fillRect(px, py, PIX, PIX, color[col]);
        tft.update(fb, false);
        return;
    }

    const int w = 4;

    im.fillRect(px + w, py + w, PIX - w * 2 + 1, PIX - w * 2 + 1, color[col]);
    for (int i = 0; i < w; i++) {
        im.drawFastHLine(px + i, py + i, PIX - 2 * i, color_gamma[0][col]);
        im.drawFastHLine(px + i, PIX + py - i - 1, PIX - 2 * i, color_gamma[1][col]);
        im.drawFastVLine(px + i, py + i, PIX - 2 * i, color_gamma[2][col]);
        im.drawFastVLine(px + PIX - i - 1, py + i, PIX - 2 * i, color_gamma[2][col]);
    }

    tft.update(fb, false);
}

/// <summary>
/// draw block pix small
/// </summary>
/// <param name="px"></param>
/// <param name="py"></param>
/// <param name="col"></param>
inline void drawBlockPixSmall(int px, int py, int col) {
    const int w = 2;

    im.fillRect(px + w, py + w, PIXSMALL - w * 2 + 1, PIXSMALL - w * 2 + 1, color[col]);
    for (int i = 0; i < w; i++) {
        im.drawFastHLine(px + i, py + i, PIXSMALL - 2 * i, color_gamma[0][col]);
        im.drawFastHLine(px + i, PIXSMALL + py - i - 1, PIXSMALL - 2 * i, color_gamma[1][col]);
        im.drawFastVLine(px + i, py + i, PIXSMALL - 2 * i, color_gamma[2][col]);
        im.drawFastVLine(px + PIXSMALL - i - 1, py + i, PIXSMALL - 2 * i, color_gamma[2][col]);
    }

    tft.update(fb, false);
}

/// <summary>
/// draw block
/// </summary>
/// <param name="blocknum"></param>
/// <param name="px"></param>
/// <param name="py"></param>
/// <param name="rotation"></param>
/// <param name="col"></param>
void drawBlock(int blocknum, int px, int py, int rotation, int col) {
    int w = BLOCKWIDTH(blocknum, rotation);
    int h = BLOCKHIGHT(blocknum, rotation);

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            if (block[rotation][blocknum][y * 4 + x + 2])
                drawBlockPix(FIELD_X + px * PIX + x * PIX, FIELD_Y + py * PIX + y * PIX, col);
        }
    }
}

/// <summary>
/// draw block small
/// </summary>
/// <param name="draw"></param>
void drawBlockSmall(bool draw) {
    const int px = 6;
    const int py = 6;

    im.fillRect(px, py, PIXSMALL * 4, PIXSMALL * 4, RGB565_Black);

    if (draw) {
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                if (block[nRotation][nBlock][y * 4 + x + 2])
                    drawBlockPixSmall(px + x * PIXSMALL, py + y * PIXSMALL, nColor);
            }
        }
    }
}

/// <summary>
/// draw block extended
/// </summary>
/// <param name="blocknum"></param>
/// <param name="px"></param>
/// <param name="py"></param>
/// <param name="rotation"></param>
/// <param name="col"></param>
/// <param name="oldx"></param>
/// <param name="oldy"></param>
/// <param name="oldrotation"></param>
static uint8_t dbuf[FIELD_WIDTH][FIELD_HIGHT] = { 0 };
void drawBlockEx(int blocknum, int px, int py, int rotation, int col, int oldx, int oldy, int oldrotation) {
    int x, y;
    int w = BLOCKWIDTH(blocknum, oldrotation);
    int h = BLOCKHIGHT(blocknum, oldrotation);

    for (x = 0; x < w; x++)
        for (y = 0; y < h; y++)
            if (block[oldrotation][blocknum][y * 4 + x + 2] > 0) dbuf[x + oldx][y + oldy] = 2;

    w = BLOCKWIDTH(blocknum, rotation);
    h = BLOCKHIGHT(blocknum, rotation);
    for (x = 0; x < w; x++)
        for (y = 0; y < h; y++)
            if (block[rotation][blocknum][y * 4 + x + 2] > 0) dbuf[x + px][y + py] = 1;

    for (y = FIELD_HIGHT - 1; y >= oldy; y--)
        for (x = 0; x < FIELD_WIDTH; x++)
            switch (dbuf[x][y]) {
            case 1:  drawBlockPix(FIELD_X + x * PIX, FIELD_Y + y * PIX, col); dbuf[x][y] = 0; break;
            case 2:  im.fillRect(FIELD_X + x * PIX, FIELD_Y + y * PIX, PIX, PIX, color[0]); dbuf[x][y] = 0; tft.update(fb, false); break;
            }
}

/// <summary>
/// draw field
/// </summary>
void drawField() {
    int x, y;
    for (y = FIELD_HIGHT - 1; y >= 0; y--)
        for (x = 0; x < FIELD_WIDTH; x++)
            drawBlockPix(FIELD_X + x * PIX, FIELD_Y + y * PIX, field[x][y]);
}

/// <summary>
/// get data from serial input
/// input text in the form "123 456\n"/// </summary>
/// <returns></returns>
char controls() {
    static int past_val1 = 99;
    static int past_val2 = 99;

    // room for 19 characters plus terminating null
    static char instring[20];
    // location in the array
    static int spos = 0;
    int val1 = 0;
    int val2 = 0;

    if (Serial.available()) {
        char* bita, * bitb;
        int c = Serial.read();
        switch (c) {
        case '\r': break; // ignore
        case '\n':
            spos = 0;
            bita = strtok(instring, " ");
            bitb = strtok(NULL, " ");
            if (bita && bitb) {
                val1 = atoi(bita);
                val2 = atoi(bitb);
            }
            break;
        default:
            if (spos < 18) {
                instring[spos++] = c;
                instring[spos] = 0;
            }
        }

        if (val1 != past_val1 || val2 != past_val2) {
            past_val1 = val1;
            past_val2 = val2;

            switch (val1) {
            case 101: // reset
                return ('r');
            case 102: // pause
                return ('p');
            case 103: // A - rotate
            case 104: // B - rotate
                return ('+');
            case 106: // down
                return ('s');
            case 107: // left
                return ('a');
            case 108: // right
                return ('d');
            }
        }
    }

    return ('\0');
}