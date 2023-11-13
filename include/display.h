#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <FS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <stack>
#include <mouse.h>

// Constants
#define PWM_CHANNEL 0
#define BACKLIGHT_PIN 4

const uint8_t BRIGHT_BRIGHTNESS = 120; // Default display brightness
const uint8_t DIM_BRIGHTNESS = 10;     // Brightness of the display after a period of inactivity
const uint16_t INACTIVITY_TIME = 900;  // Time (in display frames, 30/sec) until display dimming begins

const uint8_t SBAR_HEIGHT = 15;                         // Height of the status bar in pixels
extern uint16_t ACCENT_COLOR;
extern uint16_t TEXT_COLOR;
extern uint16_t SEL_COLOR;
extern uint16_t BGND_COLOR;

// Allow using externally defined functions/variables/objects
extern int16_t getBatteryPercentage();
class Button; // Forward declaration of class Button, which is in io.h

extern TFT_eSPI* tft;

// Wrapper class for TFT_eSPI that handles double buffering
class Display {
  TFT_eSPI *tft;
  uint16_t fillColor;
  uint16_t strokeColor;

  uint16_t read16(fs::File &f);
  uint32_t read32(fs::File &f);

public:
  uint8_t rotation;
  uint8_t brightness;
  TFT_eSprite *buffer;

  Display(TFT_eSPI* tft, TFT_eSprite* buffer);
  ~Display();
  void begin();
  void swapRotation();
  void sleepMode();
  void dim(uint8_t brightness);
  void pushChanges();
  void clear();
  void flush();
  void drawBitmapSPIFFS(const char* filename, uint16_t x, uint16_t y);
  void textFormat(uint8_t size, uint16_t color);
  void drawStatusBar();
  void drawNavArrow(uint16_t x, uint16_t y, bool direction, float progress, uint16_t stroke_color, uint16_t bg_color);
  TFT_eSPI* directAccess();
};

// Types of events that can be sent to the active page
enum class pageEvent_t : byte { ENTER, EXIT, NAV_PRESS, NAV_UP, NAV_DOWN, NAV_SELECT, NAV_CANCEL };

// Forward declaration of DisplayManager class
class DisplayManager;

// DisplayPage class - base class for a full-screen display routine
class DisplayPage {
protected:
  Display *display;
  DisplayManager *displayManager;
  uint32_t frameCounter;

public:
  const char *pageName;

  DisplayPage(Display *display, DisplayManager *displayManager, const char *pageName);
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
  DisplayPage **memberPages;

  // Subpage management
  byte subpageIdx;

  // Fun fun animation magic
  int16_t menuTlY;
  int16_t selectionTlY;

public:
  int16_t selectionY;

  template <typename... Ts>
  MenuPage(Display *display, DisplayManager *displayManager, const char *pageName, Ts *...pages);
  ~MenuPage();

  void draw();
  void onEvent(pageEvent_t event);
};

// HomePage class - displays when no menus are open
class HomePage : public DisplayPage {
  MenuPage *mainMenu;

public:
  HomePage(Display *display, DisplayManager *displayManager, const char *pageName, MenuPage *mainMenu);

  void draw();
  void onEvent(pageEvent_t event);
};

// DisplayManager class - oversees navigation and relays page events to active pages
class DisplayManager {
private:
  Display *display;
  uint32_t frameCtr;
  uint32_t lastEventFrame;

public:
    xQueueHandle eventQueue;
    xQueueHandle mouseQueue;
    std::stack<DisplayPage*> pageStack;
    Button* upButton;
    Button* downButton;

  DisplayManager(Display *display);
  void setHomepage(HomePage *homepage);
  void attachButtons(Button *upButton, Button *downButton);
  void draw();
};

////////////////////////////////////////////////////////////////////////////
// Template functions
////////////////////////////////////////////////////////////////////////////

// Build a menu page from any number of subpages
template <typename... Ts>
MenuPage::MenuPage(Display *display, DisplayManager *displayManager, const char *pageName, Ts *...pages)
    : DisplayPage(display, displayManager, pageName)
    , numPages(sizeof...(Ts))
    , subpageIdx(0)
    , menuTlY(0)
    , selectionTlY(0)
    , selectionY(0)
{
  byte pageInit = 0;
  this->memberPages = (DisplayPage **)malloc(sizeof...(Ts) * sizeof(DisplayPage *));
  auto dummy = {(this->memberPages[pageInit++] = pages)...};
  (void) dummy; // Fix unused variable warning
}

#endif