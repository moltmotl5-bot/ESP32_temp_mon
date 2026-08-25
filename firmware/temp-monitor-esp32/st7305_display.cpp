#include "st7305_display.h"

#include <Arduino.h>
#include <SPI.h>
#include <cstring>

#include <esp_heap_caps.h>

namespace {
constexpr uint8_t CASET_START = 0x12;
constexpr uint8_t CASET_END = 0x2A;
constexpr uint8_t RASET_START = 0x00;
constexpr uint8_t RASET_END = 0xC7;

void* allocDisplayMem(size_t size) {
  void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!ptr) ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return ptr;
}

void freeDisplayMem(void* ptr) {
  if (ptr) heap_caps_free(ptr);
}
}  // namespace

bool St7305Display::begin(uint8_t cs, uint8_t dc, uint8_t rst, uint8_t sck, uint8_t mosi) {
  csPin_ = cs;
  dcPin_ = dc;
  rstPin_ = rst;
  width_ = 400;
  height_ = 300;
  bufferSize_ = (width_ * height_) / 8;

  buffer_ = static_cast<uint8_t*>(allocDisplayMem(bufferSize_));
  if (!buffer_) return false;

  const uint32_t totalPixels = static_cast<uint32_t>(width_) * height_;
  pixelIndexLut_ = static_cast<uint16_t*>(allocDisplayMem(totalPixels * sizeof(uint16_t)));
  pixelBitLut_ = static_cast<uint8_t*>(allocDisplayMem(totalPixels));
  if (!pixelIndexLut_ || !pixelBitLut_) {
    freeDisplayMem(buffer_);
    buffer_ = nullptr;
    freeDisplayMem(pixelIndexLut_);
    pixelIndexLut_ = nullptr;
    freeDisplayMem(pixelBitLut_);
    pixelBitLut_ = nullptr;
    return false;
  }

  pinMode(csPin_, OUTPUT);
  pinMode(dcPin_, OUTPUT);
  pinMode(rstPin_, OUTPUT);
  digitalWrite(csPin_, HIGH);
  digitalWrite(dcPin_, HIGH);

  // Manual chip-select; do not let the SPI peripheral drive CS.
  SPI.begin(sck, -1, mosi, -1);
  SPI.beginTransaction(SPISettings(10000000, SPI_MSBFIRST, SPI_MODE0));

  clearWhite();
  initLandscapeLut();
  hardwareReset();
  initController();
  return true;
}

void St7305Display::hardwareReset() {
  digitalWrite(rstPin_, HIGH);
  delay(50);
  digitalWrite(rstPin_, LOW);
  delay(20);
  digitalWrite(rstPin_, HIGH);
  delay(50);
}

void St7305Display::sendCommand(uint8_t cmd) {
  digitalWrite(dcPin_, LOW);
  digitalWrite(csPin_, LOW);
  SPI.transfer(cmd);
  digitalWrite(csPin_, HIGH);
}

void St7305Display::sendData(uint8_t data) { sendData(&data, 1); }

void St7305Display::sendData(const uint8_t* data, size_t len) {
  digitalWrite(dcPin_, HIGH);
  digitalWrite(csPin_, LOW);
  for (size_t i = 0; i < len; ++i) SPI.transfer(data[i]);
  digitalWrite(csPin_, HIGH);
}

void St7305Display::initController() {
  sendCommand(0xD6);
  sendData((const uint8_t[]){0x17, 0x02}, 2);
  sendCommand(0xD1);
  sendData(0x01);

  sendCommand(0xC0);
  sendData((const uint8_t[]){0x11, 0x04}, 2);
  sendCommand(0xC1);
  sendData((const uint8_t[]){0x69, 0x69, 0x69, 0x69}, 4);
  sendCommand(0xC2);
  sendData((const uint8_t[]){0x19, 0x19, 0x19, 0x19}, 4);
  sendCommand(0xC4);
  sendData((const uint8_t[]){0x4B, 0x4B, 0x4B, 0x4B}, 4);
  sendCommand(0xC5);
  sendData((const uint8_t[]){0x19, 0x19, 0x19, 0x19}, 4);
  sendCommand(0xD8);
  sendData((const uint8_t[]){0x80, 0xE9}, 2);
  sendCommand(0xB2);
  sendData(0x02);
  sendCommand(0xB3);
  sendData((const uint8_t[]){0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45}, 10);
  sendCommand(0xB4);
  sendData((const uint8_t[]){0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45}, 8);
  sendCommand(0x62);
  sendData((const uint8_t[]){0x32, 0x03, 0x1F}, 3);
  sendCommand(0xB7);
  sendData(0x13);
  sendCommand(0xB0);
  sendData(0x64);

  sendCommand(0x11);
  delay(200);

  sendCommand(0xC9);
  sendData(0x00);
  sendCommand(0x36);
  sendData(0x48);
  sendCommand(0x3A);
  sendData(0x11);
  sendCommand(0xB9);
  sendData(0x20);
  sendCommand(0xB8);
  sendData(0x29);
  sendCommand(0x21);

  sendCommand(0x2A);
  sendData((const uint8_t[]){CASET_START, CASET_END}, 2);
  sendCommand(0x2B);
  sendData((const uint8_t[]){RASET_START, RASET_END}, 2);
  sendCommand(0x35);
  sendData(0x00);
  sendCommand(0xD0);
  sendData(0xFF);
  sendCommand(0x38);
  sendCommand(0x29);
}

void St7305Display::initLandscapeLut() {
  const uint16_t h4 = height_ >> 2;
  for (uint16_t y = 0; y < height_; ++y) {
    const uint16_t invY = height_ - 1 - y;
    const uint16_t blockY = invY >> 2;
    const uint8_t localY = invY & 3;

    for (uint16_t x = 0; x < width_; ++x) {
      const uint16_t byteX = x >> 1;
      const uint8_t localX = x & 1;
      const uint32_t bufferIdx = static_cast<uint32_t>(byteX) * h4 + blockY;
      const uint8_t bit = static_cast<uint8_t>(7 - ((localY << 1) | localX));
      const uint32_t pixelIdx = static_cast<uint32_t>(x) * height_ + y;
      pixelIndexLut_[pixelIdx] = static_cast<uint16_t>(bufferIdx);
      pixelBitLut_[pixelIdx] = static_cast<uint8_t>(1 << bit);
    }
  }
}

void St7305Display::clearWhite() {
  if (buffer_) memset(buffer_, 0xFF, bufferSize_);
}

void St7305Display::setPixelBlack(int x, int y) { setPixel(x, y, true); }

void St7305Display::setPixel(int x, int y, bool black) {
  if (!ready() || x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_)) {
    return;
  }
  const uint32_t pixelIdx = static_cast<uint32_t>(x) * height_ + y;
  const uint16_t bufferIdx = pixelIndexLut_[pixelIdx];
  const uint8_t bitMask = pixelBitLut_[pixelIdx];
  if (black) {
    buffer_[bufferIdx] &= static_cast<uint8_t>(~bitMask);
  } else {
    buffer_[bufferIdx] |= bitMask;
  }
}

void St7305Display::drawRect(int x, int y, int w, int h, bool black) {
  if (w <= 0 || h <= 0) return;
  for (int ix = x; ix < x + w; ++ix) {
    setPixel(ix, y, black);
    setPixel(ix, y + h - 1, black);
  }
  for (int iy = y; iy < y + h; ++iy) {
    setPixel(x, iy, black);
    setPixel(x + w - 1, iy, black);
  }
}

void St7305Display::fillRect(int x, int y, int w, int h, bool black) {
  if (w <= 0 || h <= 0) return;
  for (int iy = y; iy < y + h; ++iy) {
    for (int ix = x; ix < x + w; ++ix) {
      setPixel(ix, iy, black);
    }
  }
}

void St7305Display::flush() {
  if (!buffer_) return;

  sendCommand(0x38);
  sendCommand(0x29);
  sendCommand(0x2A);
  sendData((const uint8_t[]){CASET_START, CASET_END}, 2);
  sendCommand(0x2B);
  sendData((const uint8_t[]){RASET_START, RASET_END}, 2);

  digitalWrite(dcPin_, LOW);
  digitalWrite(csPin_, LOW);
  SPI.transfer(0x2C);
  digitalWrite(dcPin_, HIGH);
  for (size_t i = 0; i < bufferSize_; ++i) SPI.transfer(buffer_[i]);
  digitalWrite(csPin_, HIGH);
}
