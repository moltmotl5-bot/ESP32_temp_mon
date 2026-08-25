#pragma once

#include <stddef.h>
#include <stdint.h>

class St7305Display {
 public:
  bool begin(uint8_t cs, uint8_t dc, uint8_t rst, uint8_t sck, uint8_t mosi);
  void clearWhite();
  void setPixelBlack(int x, int y);
  void setPixel(int x, int y, bool black);
  void drawRect(int x, int y, int w, int h, bool black);
  void fillRect(int x, int y, int w, int h, bool black);
  void flush();
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  bool ready() const { return buffer_ != nullptr && pixelIndexLut_ != nullptr && pixelBitLut_ != nullptr; }

 private:
  uint8_t csPin_ = 0;
  uint8_t dcPin_ = 0;
  uint8_t rstPin_ = 0;
  uint16_t width_ = 400;
  uint16_t height_ = 300;
  size_t bufferSize_ = 0;
  uint8_t* buffer_ = nullptr;
  uint16_t* pixelIndexLut_ = nullptr;
  uint8_t* pixelBitLut_ = nullptr;

  void hardwareReset();
  void initController();
  void initLandscapeLut();
  void sendCommand(uint8_t cmd);
  void sendData(uint8_t data);
  void sendData(const uint8_t* data, size_t len);
};
