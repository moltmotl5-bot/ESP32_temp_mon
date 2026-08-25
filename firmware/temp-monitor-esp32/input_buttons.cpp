#include "input_buttons.h"

#include <Arduino.h>

#include "config.h"

namespace {
struct ButtonState {
  uint8_t pin;
  bool stablePressed;
  bool lastReading;
  uint32_t changedAt;
  uint32_t pressedAt;
};

ButtonState bootBtn {BTN_BOOT, false, true, 0, 0};
ButtonState keyBtn {BTN_KEY, false, true, 0, 0};

bool readPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

ButtonEvent pollOne(ButtonState& btn) {
  const bool reading = readPressed(btn.pin);
  const uint32_t now = millis();

  if (reading != btn.lastReading) {
    btn.changedAt = now;
    btn.lastReading = reading;
  }
  if (now - btn.changedAt < 30) return ButtonEvent::None;

  if (reading && !btn.stablePressed) {
    btn.stablePressed = true;
    btn.pressedAt = now;
    return ButtonEvent::None;
  }

  if (!reading && btn.stablePressed) {
    btn.stablePressed = false;
    const uint32_t held = now - btn.pressedAt;
    if (btn.pin == BTN_BOOT) {
      return held >= LONG_PRESS_MS ? ButtonEvent::BootLong : ButtonEvent::BootShort;
    }
    if (btn.pin == BTN_KEY) {
      return held >= LONG_PRESS_MS ? ButtonEvent::KeyLong : ButtonEvent::KeyShort;
    }
  }

  return ButtonEvent::None;
}
}  // namespace

void inputInit() {
  pinMode(BTN_BOOT, INPUT_PULLUP);
  pinMode(BTN_KEY, INPUT_PULLUP);
}

ButtonEvent inputPoll() {
  static bool comboLatched = false;
  static bool comboConsumed = false;
  const bool bootDown = readPressed(BTN_BOOT);
  const bool keyDown = readPressed(BTN_KEY);

  if (bootDown && keyDown) {
    if (!comboLatched) {
      comboLatched = true;
      comboConsumed = true;
      bootBtn.stablePressed = false;
      keyBtn.stablePressed = false;
      return ButtonEvent::ComboAdd;
    }
    return ButtonEvent::None;
  }

  if (!bootDown && !keyDown) {
    comboLatched = false;
    comboConsumed = false;
  }

  if (comboConsumed) {
    pollOne(bootBtn);
    pollOne(keyBtn);
    return ButtonEvent::None;
  }

  if (const ButtonEvent boot = pollOne(bootBtn); boot != ButtonEvent::None) return boot;
  if (const ButtonEvent key = pollOne(keyBtn); key != ButtonEvent::None) return key;
  return ButtonEvent::None;
}
