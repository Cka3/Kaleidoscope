/* -*- mode: c++ -*-
 * Kaleidoscope-Hardware-ATMegaKeyboard -- Base class for some ATMega-based boards
 * Copyright (C) 2018  Keyboard.io, Inc
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Kaleidoscope.h"

#if KALEIDOSCOPE_WITH_ATMEGA_KEYBOARD

#include "kaleidoscope/hardware/ATMegaKeyboard.h"

namespace kaleidoscope {
namespace hardware {

bool ATMegaKeyboard::do_scan_;
uint8_t ATMegaKeyboard::debounce = 3;

void ATMegaKeyboard::setup(void) {
  wdt_disable();

  for (uint8_t i = 0; i < KeyboardHardware.matrix_columns; i++) {
    DDR_INPUT(KeyboardHardware.matrix_col_pins[i]);
    ENABLE_PULLUP(KeyboardHardware.matrix_col_pins[i]);
  }

  for (uint8_t i = 0; i < KeyboardHardware.matrix_row_pin_cnt; i++) {
    DDR_OUTPUT(KeyboardHardware.matrix_row_pins[i]);
    OUTPUT_HIGH(KeyboardHardware.matrix_row_pins[i]);
  }

  /* Set up Timer1 for 1700usec */
  TCCR1B = _BV(WGM13);
  TCCR1A = 0;

  const uint32_t cycles = (F_CPU / 2000000) * 1700;

  ICR1 = cycles;
  TCCR1B = _BV(WGM13) | _BV(CS10);
  TIMSK1 = _BV(TOIE1);
}

void ATMegaKeyboard::detachFromHost() {
  UDCON |= _BV(DETACH);
}

void ATMegaKeyboard::attachToHost() {
  UDCON &= ~_BV(DETACH);
}

void __attribute__((optimize(3))) ATMegaKeyboard::readMatrix(void) {
  for (int8_t current_row = 0;
       current_row < KeyboardHardware.matrix_row_pin_cnt;
       current_row++) {
    uint16_t mask, cols;
    int8_t matrix_row = current_row +
                        (KeyboardHardware.matrix_rows - KeyboardHardware.matrix_row_pin_cnt);

    mask = KeyboardHardware.debounceMaskForRow(matrix_row);

    OUTPUT_TOGGLE(KeyboardHardware.matrix_row_pins[current_row]);
    cols = (KeyboardHardware.readCols() & mask) | (KeyboardHardware.keyState_[matrix_row] & ~mask);
    OUTPUT_TOGGLE(KeyboardHardware.matrix_row_pins[current_row]);

    KeyboardHardware.debounceRow(cols ^ KeyboardHardware.keyState_[matrix_row], current_row);
    KeyboardHardware.keyState_[matrix_row] = cols;
  }
}

uint8_t ATMegaKeyboard::pressedKeyswitchCount() {
  uint8_t count = 0;

  for (int8_t r = 0; r < KeyboardHardware.matrix_rows; r++) {
    count += __builtin_popcount(KeyboardHardware.keyState_[r]);
  }
  return count;
}

bool ATMegaKeyboard::isKeyswitchPressed(uint8_t row, byte col) {
  return (bitRead(KeyboardHardware.keyState_[row], col) != 0);

}

bool ATMegaKeyboard::isKeyswitchPressed(uint8_t keyIndex) {
  keyIndex--;
  return isKeyswitchPressed(keyIndex / KeyboardHardware.matrix_columns,
                            keyIndex % KeyboardHardware.matrix_columns);
}

void __attribute__((optimize(3))) ATMegaKeyboard::actOnMatrixScan() {
  for (byte row = 0; row < KeyboardHardware.matrix_rows; row++) {
    for (byte col = 0; col < KeyboardHardware.matrix_columns; col++) {
      uint8_t keyState = (bitRead(KeyboardHardware.previousKeyState_[row], col) << 0) |
                         (bitRead(KeyboardHardware.keyState_[row], col) << 1);
      if (keyState) {
        handleKeyswitchEvent(Key_NoKey, row, col, keyState);
      }
    }
    KeyboardHardware.previousKeyState_[row] = KeyboardHardware.keyState_[row];
  }
}

void ATMegaKeyboard::scanMatrix() {
  if (do_scan_) {
    do_scan_ = false;
    // We only want to update our matrix if the timer has expired.
    KeyboardHardware.readMatrix();
  }
  // We ALWAYS want to tell Kaleidoscope about the state of the matrix
  KeyboardHardware.actOnMatrixScan();
}

void ATMegaKeyboard::maskKey(byte row, byte col) {
  if (row >= KeyboardHardware.matrix_rows || col >= KeyboardHardware.matrix_columns)
    return;

  bitWrite(KeyboardHardware.masks_[row], col, 1);
}

void ATMegaKeyboard::unMaskKey(byte row, byte col) {
  if (row >= KeyboardHardware.matrix_rows || col >= KeyboardHardware.matrix_columns)
    return;

  bitWrite(KeyboardHardware.masks_[row], col, 0);
}

bool ATMegaKeyboard::isKeyMasked(byte row, byte col) {
  if (row >= KeyboardHardware.matrix_rows || col >= KeyboardHardware.matrix_columns)
    return false;

  return bitRead(KeyboardHardware.masks_[row], col);
}

uint16_t ATMegaKeyboard::readCols() {
  uint16_t results = 0x00 ;
  for (uint8_t i = 0; i < KeyboardHardware.matrix_columns; i++) {
    asm("NOP"); // We need to pause a beat before reading or we may read before the pin is hot
    results |= (!READ_PIN(KeyboardHardware.matrix_col_pins[i]) << i);
  }
  return results;
}

uint16_t ATMegaKeyboard::debounceMaskForRow(uint8_t row) {
  uint16_t result = 0;

  for (uint16_t c = 0; c < KeyboardHardware.matrix_columns; ++c) {
    if (KeyboardHardware.debounce_matrix_[row][c]) {
      --KeyboardHardware.debounce_matrix_[row][c];
    } else {
      result |= _BV(c);
    }
  }
  return result;
}

void ATMegaKeyboard::debounceRow(uint16_t change, uint8_t row) {
  for (uint16_t i = 0; i < KeyboardHardware.matrix_columns; ++i) {
    if (change & _BV(i)) {
      KeyboardHardware.debounce_matrix_[row][i] = debounce;
    }
  }
}

}
}

#endif
