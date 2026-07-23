#include <Arduino.h>
#include <DigiCDC.h>
#include <EEPROM.h>
#include <stdlib.h>

/*
 * Digispark HW-018 CW Paddle USB Serial Device
 *
 * Pin Configuration:
 * - Dit (Dot) Pin:  P0 (PB0) - Safe input, does not interfere with USB.
 * - Dah (Dash) Pin: P2 (PB2) - Safe input, does not interfere with USB.
 * - Onboard LED:    P1 (PB1) - Configured as output for visual feedback.
 *
 * Pin 3 & Pin 4 are used by V-USB (D- and D+).
 * Pin 1 is hardwired to onboard LED.
 */

#define DIT_PIN 0
#define DAH_PIN 2
#define LED_PIN 1

// EEPROM settings
#define EEPROM_WPM_ADDR 0
#define MIN_WPM 5
#define MAX_WPM 60
#define DEFAULT_WPM 20

// State Tracking
int current_wpm = DEFAULT_WPM;
unsigned long dit_ms = 1200 / DEFAULT_WPM;

bool last_was_dit = false;
bool dit_latch = false;
bool dah_latch = false;

// Serial Buffer for USB configuration
char rx_buffer[16];
uint8_t rx_index = 0;

void set_wpm(int new_wpm) {
  if (new_wpm < MIN_WPM || new_wpm > MAX_WPM) {
    SerialUSB.print(F("ERROR: WPM must be between "));
    SerialUSB.print(MIN_WPM);
    SerialUSB.print(F(" and "));
    SerialUSB.println(MAX_WPM);
    return;
  }

  current_wpm = new_wpm;
  dit_ms = 1200 / current_wpm;

  // Persist to EEPROM if changed
  if (EEPROM.read(EEPROM_WPM_ADDR) != (uint8_t)current_wpm) {
    EEPROM.write(EEPROM_WPM_ADDR, (uint8_t)current_wpm);
  }

  SerialUSB.print(F("OK: WPM set to "));
  SerialUSB.print(current_wpm);
  SerialUSB.print(F(" (DIT: "));
  SerialUSB.print(dit_ms);
  SerialUSB.println(F(" ms)"));
}

void parse_serial_command(const char *cmd) {
  // Skip leading whitespace
  while (*cmd == ' ' || *cmd == '\t') {
    cmd++;
  }

  if (*cmd == '\0') {
    return;
  }

  // If command is "?" or "help" or "wpm", display current settings
  if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0 ||
      strcmp(cmd, "wpm") == 0) {
    SerialUSB.print(F("Current WPM: "));
    SerialUSB.print(current_wpm);
    SerialUSB.print(F(" (DIT: "));
    SerialUSB.print(dit_ms);
    SerialUSB.println(F(" ms)"));
    return;
  }

  // Skip optional "wpm" or "wpm=" or "w" prefix (case insensitive)
  if ((cmd[0] == 'w' || cmd[0] == 'W') && (cmd[1] == 'p' || cmd[1] == 'P') &&
      (cmd[2] == 'm' || cmd[2] == 'M')) {
    cmd += 3;
    if (*cmd == '=' || *cmd == ' ') {
      cmd++;
    }
  } else if (cmd[0] == 'w' || cmd[0] == 'W') {
    cmd++;
    if (*cmd == '=' || *cmd == ' ') {
      cmd++;
    }
  }

  // Parse integer
  int val = atoi(cmd);
  if (val > 0) {
    set_wpm(val);
  } else {
    SerialUSB.println(
        F("ERROR: Invalid command. Send a number (5-60) to set WPM."));
  }
}

void process_usb_serial() {
  while (SerialUSB.available()) {
    char c = SerialUSB.read();

    if (c == '\r' || c == '\n') {
      if (rx_index > 0) {
        rx_buffer[rx_index] = '\0';
        parse_serial_command(rx_buffer);
        rx_index = 0;
      }
    } else {
      if (rx_index < sizeof(rx_buffer) - 1) {
        rx_buffer[rx_index++] = c;
      } else {
        // Buffer overflow protection: reset index
        rx_index = 0;
      }
    }
  }
}

// Custom delay function that keeps the USB driver alive (via DigiCDC)
// while selectively polling inputs to implement paddle memory.
void delay_and_latch(unsigned long ms, bool poll_dit, bool poll_dah) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    // Read pins and latch if pressed (LOW)
    if (poll_dit && (digitalRead(DIT_PIN) == LOW)) {
      dit_latch = true;
    }
    if (poll_dah && (digitalRead(DAH_PIN) == LOW)) {
      dah_latch = true;
    }

    process_usb_serial();
    SerialUSB.delay(1);
  }
}

void send_dit() {
  digitalWrite(LED_PIN, HIGH); // Turn LED on for dot
  SerialUSB.print(".");
  last_was_dit = true;
  dit_latch = false; // Clear own latch

  // While sending a dot, poll DAH pin for iambic memory
  delay_and_latch(dit_ms, false, true);

  digitalWrite(LED_PIN, LOW); // Turn LED off for space
  delay_and_latch(dit_ms, false, true);
}

void send_dah() {
  digitalWrite(LED_PIN, HIGH); // Turn LED on for dash
  SerialUSB.print("-");
  last_was_dit = false;
  dah_latch = false; // Clear own latch

  // While sending a dash, poll DIT pin for iambic memory
  delay_and_latch(dit_ms * 3, true, false);

  digitalWrite(LED_PIN, LOW); // Turn LED off for space
  delay_and_latch(dit_ms, true, false);
}

void setup() {
  // Set LED pin as output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Set paddle pins as inputs with internal pull-up resistors
  pinMode(DIT_PIN, INPUT_PULLUP);
  pinMode(DAH_PIN, INPUT_PULLUP);

  // Read saved WPM from EEPROM
  uint8_t saved_wpm = EEPROM.read(EEPROM_WPM_ADDR);
  if (saved_wpm >= MIN_WPM && saved_wpm <= MAX_WPM) {
    current_wpm = saved_wpm;
    dit_ms = 1200 / current_wpm;
  }

  // Initialize USB CDC Serial
  SerialUSB.begin();

  // Keep USB stack actively polled for 2 seconds to allow Windows CDC driver (usbser.sys)
  // to complete descriptor requests and device enumeration.
  for (int i = 0; i < 200; i++) {
    SerialUSB.delay(10);
  }
}


void loop() {
  // Process any incoming USB serial commands
  process_usb_serial();

  // Check current physical paddle states
  bool dit_pressed = (digitalRead(DIT_PIN) == LOW);
  bool dah_pressed = (digitalRead(DAH_PIN) == LOW);

  // If either paddle is physically pressed, latch the state
  if (dit_pressed)
    dit_latch = true;
  if (dah_pressed)
    dah_latch = true;

  // Handle Iambic / Single paddle actions based on memory
  if (dit_latch && dah_latch) {
    // Both active: alternate based on what was last sent
    if (last_was_dit) {
      send_dah();
    } else {
      send_dit();
    }
  } else if (dit_latch) {
    send_dit();
  } else if (dah_latch) {
    send_dah();
  } else {
    // Idle state: refresh USB CDC stack
    SerialUSB.refresh();
    SerialUSB.delay(10);
  }
}
