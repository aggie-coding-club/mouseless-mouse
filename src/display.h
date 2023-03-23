#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

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
};

class DisplayPage {
    void (*drawHandler)();
public:
    DisplayPage(void (*drawHandler)());
    inline void draw();
};

class MenuPage {
    byte numPages;
    DisplayPage** pages;
public:
    template <typename... T>
    MenuPage(const T*... displayPages);
};

#endif