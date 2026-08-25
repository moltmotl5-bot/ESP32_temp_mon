#pragma once

enum class ButtonEvent {
  None,
  BootShort,   // next edit field / next route row
  BootLong,    // remove route
  KeyShort,    // change direction / stop / route
  KeyLong,     // refresh ETAs
  ComboAdd,    // BOOT + KEY: add route
};

void inputInit();
ButtonEvent inputPoll();
