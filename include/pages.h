#ifndef DISPLAY_PAGES
#define DISPLAY_PAGES

#include "display.h"
#include "3ml_cleaner.h"
#include <Arduino.h>

// Define a blank placeholder page
class BlankPage : public DisplayPage {
public:
  BlankPage(Display *display, DisplayManager *displayManager, const char *pageName);
  void draw();
  void onEvent(pageEvent_t event);
};

// Allow user input for text fields
class KeyboardPage : public DisplayPage {
private:
  char *textBuffer;
  byte bufferIdx;
  char *field;
  int16_t specialIdx;
  int16_t letterIdx;
  int16_t numberIdx;
  int16_t colIdx;
  int16_t specialTlY;
  int16_t letterTlY;
  int16_t numberTlY;

public:
  KeyboardPage(Display *display, DisplayManager *displayManager, const char *pageName);
  KeyboardPage *operator()(char *field);
  void draw();
  void onEvent(pageEvent_t event);
};

// Generic page prompting users to confirm an action
class ConfirmationPage : public DisplayPage {
private:
  const char *message;
  void (*callback)();

public:
  ConfirmationPage(Display *display, DisplayManager *displayManager, const char *pageName);
  ConfirmationPage *operator()(const char *message, void (*callback)());
  void draw();
  void onEvent(pageEvent_t event);
};

// Page to visualize mouse inputs in real-time
class InputDisplay : public DisplayPage {
private:
  bool lmb, rmb, scrollU, scrollD, lock, calibrate;
  
public:
  InputDisplay(Display* display, DisplayManager* displayManager, const char* pageName);
  void onEvent(pageEvent_t event);
  void onMouseEvent(mouseEvent_t event);
  void draw();
};

// Amruth's sandbox
class DebugPage : public DisplayPage {
public:
  DebugPage(Display *display, DisplayManager *displayManager, const char *pageName);
  void draw();
  void onEvent(pageEvent_t event);
};

// Slider adjustment bar displayed inline on a menu page
typedef void (*changeCallback_t)(byte value);
class InlineSlider : public DisplayPage {
  uint8_t sliderValue;
  changeCallback_t onChange;
public:
  InlineSlider(Display *display, DisplayManager *displayManager, const char *pageName, changeCallback_t onChange);
  void draw();
  void onEvent(pageEvent_t event);
};

class DOMPage : public DisplayPage {
  struct SelectableNode {
    threeml::DOMNode *node;
    int16_t yPos;
  };
  struct Script {
    char *memory;
    struct js *engine;

    Script();
    ~Script();
  };

  const char *sourceFileName;
  threeml::DOM *dom;
  std::vector<Script*> scripts;
  SelectableNode nextSelectableNode;
  SelectableNode selectedNode;
  SelectableNode prevSelectableNode;
  int16_t scrollPos;
  int16_t scrollTlY;
  byte selectionIdx;
  
public:
  DOMPage(Display *display, DisplayManager *displayManager, const char *fileName);
  void draw();
  void onEvent(pageEvent_t event);
  void load();
    void loadDOM();
    void loadScript(threeml::DOMNode *script);
  void unload();
};

#endif
