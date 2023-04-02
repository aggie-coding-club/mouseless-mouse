#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <SPIFFS.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <stack>
#include <mouse.h>

#define PWM_CHANNEL 0
#define BACKLIGHT_PIN 4

const uint8_t BRIGHT_BRIGHTNESS = 120;  // Default display brightness
const uint8_t DIM_BRIGHTNESS = 10;      // Brightness of the display after a period of inactivity
const uint16_t INACTIVITY_TIME = 900;   // Time (in display frames, 30/sec) until display dimming begins

const uint8_t SBAR_HEIGHT = 15;         // Height of the status bar in pixels
const uint16_t ACCENT_COLOR = 0x461F;   // TFT_eSPI::color565(64, 192, 255)
const uint16_t TEXT_COLOR = TFT_WHITE;  // Color of menu text
const uint16_t SEL_COLOR = ACCENT_COLOR >> 1 & ~0x0410; // Equivalent to lerp(ACCENT_COLOR, TFT_BLACK, 0.5)
const uint16_t BGND_COLOR = TFT_BLACK;  // Color of background

extern int16_t getBatteryPercentage();
class Button;   // Forward declaration of class Button, which is in io.h

extern TFT_eSPI* tft;

// Wrapper class for TFT_eSPI that handles double buffering
class Display {
    TFT_eSPI* tft;
    TFT_eSprite* bufferA;
    TFT_eSprite* bufferB;
    TFT_eSprite* activeBuffer;
    uint16_t fillColor;
    uint16_t strokeColor;

    uint16_t read16(fs::File &f);
    uint32_t read32(fs::File &f);

public:
    uint8_t brightness;

    Display(TFT_eSPI* tft, TFT_eSprite* bufferA, TFT_eSprite* bufferB);
    ~Display();
    void begin();
    void sleepMode();
    void dim(uint8_t brightness);
    void pushChanges();
    void clear();
    void flush();
    void setFill(uint16_t color);
    void setStroke(uint16_t color);
    int16_t getStringWidth(const char* string);
    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void drawArc(uint16_t x, uint16_t y, uint16_t r, uint16_t ir, uint16_t startAngle, uint16_t endAngle, uint16_t fg_color, uint16_t bg_color);
    void drawString(String string, uint16_t xPos, uint16_t yPos);
    void drawBitmapSPIFFS(const char* filename, uint16_t x, uint16_t y);
    void fillRect(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height);
    void textFormat(uint8_t size, uint16_t color);
    void drawStatusBar();
    void drawNavArrow(uint16_t x, uint16_t y, bool direction, float progress, uint16_t stroke_color, uint16_t bg_color);
    TFT_eSPI* directAccess();
};

// Types of events that can be sent to the active page
enum class pageEvent_t : byte {ENTER, EXIT, NAV_PRESS, NAV_UP, NAV_DOWN, NAV_SELECT, NAV_CANCEL};

class DisplayManager;

// DisplayPage class - base class for a full-screen display routine
class DisplayPage {
protected:
    Display* display;
    DisplayManager* displayManager;
    uint32_t frameCounter;

public:
    const char* pageName;

    DisplayPage(Display* display, DisplayManager* displayManager, const char* pageName);
    virtual ~DisplayPage();

    // Must be implemented by all pages
    virtual void draw() = 0;
    // Must be implemented by all pages
    virtual void onEvent(pageEvent_t event) = 0;

    virtual void onMouseEvent(mouseEvent_t event) = 0;
};



// MenuPage class - holds a collection of Page classes and facilitates recursive menus
class MenuPage : public DisplayPage {
    // Array of subpage classes - these are expected to implement certain methods, see below
    uint16_t numPages;
    DisplayPage** memberPages;

    // Subpage management
    byte subpageIdx;

    // Fun fun animation magic
    int16_t menuTlY;
    int16_t selectionTlY;

public:
    template <typename... Ts>
    MenuPage(Display* display, DisplayManager* displayManager, const char* pageName, Ts*... pages);
    ~MenuPage();

    void draw();
    void onEvent(pageEvent_t event);
};



// HomePage class - displays when no menus are open
class HomePage : public DisplayPage {
    MenuPage* mainMenu;

public:
    HomePage(Display* display, DisplayManager* displayManager, const char* pageName, MenuPage* mainMenu);

    void draw();
    void onEvent(pageEvent_t event);
};



// DisplayManager class - oversees navigation and relays page events to active pages
class DisplayManager {
private:
    Display* display;
    uint32_t frameCtr;
    uint32_t lastEventFrame;

public:
    xQueueHandle eventQueue;
    xQueueHandle mouseQueue;
    std::stack<DisplayPage*> pageStack;
    Button* upButton;
    Button* downButton;

    DisplayManager(Display* display);
    void setHomepage(HomePage* homepage);
    void attachButtons(Button* upButton, Button* downButton);
    void draw();
};

////////////////////////////////////////////////////////////////////////////
// Template functions
////////////////////////////////////////////////////////////////////////////

template <typename... Ts>
MenuPage::MenuPage(Display* display, DisplayManager* displayManager, const char* pageName, Ts*... pages) : DisplayPage(display, displayManager, pageName) {
    this->numPages = sizeof...(Ts);
    byte pageInit = 0;
    this->memberPages = (DisplayPage**)malloc(sizeof...(Ts) * sizeof(DisplayPage*));
    auto dummy = {(this->memberPages[pageInit++] = pages)...};
    this->subpageIdx = 0;
    this->menuTlY = 0;
    this->selectionTlY = 0;
}

#endif