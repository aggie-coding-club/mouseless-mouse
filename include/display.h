#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <TFT_eSPI.h>
#include <stack>

class Display {
    TFT_eSPI* tft;
    TFT_eSprite* bufferA;
    TFT_eSprite* bufferB;
    TFT_eSprite* activeBuffer;
    uint16_t fillColor;
    uint16_t strokeColor;
public:
    Display(TFT_eSPI* tft, TFT_eSprite* bufferA, TFT_eSprite* bufferB);
    ~Display();
    void begin();
    void dim(uint8_t brightness);
    void pushChanges();
    void clear();
    void setFill(uint16_t color);
    void setStroke(uint16_t color);
    void drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void drawString(String string, uint16_t xPos, uint16_t yPos);
    void textFormat(uint8_t size, uint16_t color);
    void drawStatusBar();
};

// Types of events that can be sent to the active page
enum class pageEvent_t : byte {ENTER, EXIT, NAV_UP, NAV_DOWN, NAV_SELECT, NAV_CANCEL};

class DisplayManager;

// DisplayPage class - base class for a full-screen display routine
class DisplayPage {
protected:
    Display* display;
    DisplayManager* displayManager;
    const char* pageName;
    uint32_t frameCounter;

public:
    DisplayPage(Display* display, DisplayManager* displayManager, const char* pageName);
    virtual ~DisplayPage();

    // Must be implemented by all pages
    virtual void draw() = 0;
    // Must be implemented by all pages
    virtual void onEvent(pageEvent_t event) = 0;
};



// MenuPage class - holds a collection of Page classes and facilitates recursive menus
class MenuPage : public DisplayPage {
    // Array of subpage classes - these are expected to implement certain methods, see below
    uint16_t numPages;
    DisplayPage** memberPages;

    // Subpage management
    byte subpageIdx;

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



class DisplayManager {
private:
    Display* display;

public:
    xQueueHandle eventQueue;
    std::stack<DisplayPage*> pageStack;

    DisplayManager(Display* display);
    void setHomepage(HomePage* homepage);
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
}

#endif