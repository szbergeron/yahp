#include "utils.cpp"
#include "config.cpp"
#include <Array.h>
#include <cstdint>
#include "usb_serial.h"

Array<uint8_t, 16> detect_boards() {
  Serial.println("Getting board info");
  const size_t LINE_LEN = 256;
  uint8_t linebuf[LINE_LEN];
  // first, detect which boards are present
  // do this by checking if we can pull up/down
  // the voltage on the lines, or if
  // they automatically return to some given value
  // // for now, just ask \:)
  Serial.print("How many boards are there? [0-255]:");

  uint8_t board_count = Serial.parseInt();

  Array<uint8_t, 16> boards;

  bool sequential = false;

  while (true) {
    Serial.print("Are they numbered sequentially starting from 0? (Y/n): ");
    auto s = Serial.readString();
    if (s == "n" || s == "N") {
      sequential = false;
      break;
    } else if (s == "y" || s == "Y" || s == "") {
      sequential = true;
      break;
    } else {
      Serial.println("Unrecognized value: " + s);
    }
  }

  Serial.println();

  if (sequential) {
    for (uint8_t i = 0; i < board_count; i++) {
      boards.push_back(i);
    }
  } else {
    for (uint8_t i = 0; i < board_count; i++) {
      Serial.print("What is the board ID (between 0 and 255) of bord number" +
                   String(i) + "? [0-255]:");

      uint8_t bid = Serial.parseInt();

      boards.push_back(bid);
    }
  }

  return boards;
}

Array<Array<bool, KEYS_PER_BOARD>, 16> detect_keys(Array<uint8_t, 16> &boards) {
}

void calibrate() {
  Serial.println("Beginning calibration");

  auto boards = detect_boards();

  auto keys = detect_keys(&boards);
}
