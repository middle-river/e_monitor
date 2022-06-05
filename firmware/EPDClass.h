// Library for the E-Ink ED060XC3.
// 2021-11-18,2022-05-29  T. Nakagawa

#ifndef EPDCLASS_H_
#define EPDCLASS_H_

#include <Arduino.h>

class EPDClass {
public:
  EPDClass(int pin_spv, int pin_ckv, int pin_mode, int pin_stl, int pin_oe, int pin_le, int pin_cl,int pin_d0, int pin_sda, int pin_scl, int pin_wakeup) : pin_spv_(pin_spv), pin_ckv_(pin_ckv), pin_mode_(pin_mode), pin_stl_(pin_stl), pin_oe_(pin_oe), pin_le_(pin_le), pin_cl_(pin_cl), pin_d0_(pin_d0), pin_sda_(pin_sda), pin_scl_(pin_scl), pin_wakeup_(pin_wakeup) {
    digitalWriteByte(pin_d0_, 0x00);
    pinModeByte(pin_d0_, INPUT);
    digitalWrite(pin_cl_, LOW);
    pinMode(pin_cl_, INPUT);
    digitalWrite(pin_le_, LOW);
    pinMode(pin_le_, INPUT);
    digitalWrite(pin_oe_, LOW);
    pinMode(pin_oe_, INPUT);
    digitalWrite(pin_stl_, LOW);
    pinMode(pin_stl_, INPUT);
    digitalWrite(pin_mode_, LOW);
    pinMode(pin_mode_, INPUT);
    digitalWrite(pin_ckv_, LOW);
    pinMode(pin_ckv_, INPUT);
    digitalWrite(pin_spv_, LOW);
    pinMode(pin_spv_, INPUT);
    digitalWrite(pin_wakeup_, LOW);
    pinMode(pin_wakeup_, INPUT);
    digitalWrite(pin_scl_, LOW);
    pinMode(pin_scl_, INPUT);
    digitalWrite(pin_sda_, LOW);
    pinMode(pin_sda_, INPUT);
  }

  void enable() {
    Wire.begin(pin_sda_, pin_scl_);
    pinMode(pin_wakeup_, OUTPUT);
    digitalWrite(pin_wakeup_, HIGH);
    delay(5);
    writeWire(0x01, 0x20);	// Enable VDD.
    delay(1);
    writeWire(0x01, 0xa0);	// Enable other power rails.
    delay(25);
    writeWire(0x01, 0x3f);	// Enable VCOM.
    delay(1);

    digitalWrite(pin_spv_, HIGH);
    pinMode(pin_spv_, OUTPUT);
    digitalWrite(pin_ckv_, HIGH);
    pinMode(pin_ckv_, OUTPUT);
    pinMode(pin_mode_, OUTPUT);
    digitalWrite(pin_stl_, HIGH);
    pinMode(pin_stl_, OUTPUT);
    pinMode(pin_oe_, OUTPUT);
    pinMode(pin_le_, OUTPUT);
    pinMode(pin_cl_, OUTPUT);
    pinModeByte(pin_d0_, OUTPUT);
  }

  void disable() {
    digitalWriteByte(pin_d0_, 0x00);
    pinModeByte(pin_d0_, INPUT);
    digitalWrite(pin_cl_, LOW);
    pinMode(pin_cl_, INPUT);
    digitalWrite(pin_le_, LOW);
    pinMode(pin_le_, INPUT);
    digitalWrite(pin_oe_, LOW);
    pinMode(pin_oe_, INPUT);
    digitalWrite(pin_stl_, LOW);
    pinMode(pin_stl_, INPUT);
    digitalWrite(pin_mode_, LOW);
    pinMode(pin_mode_, INPUT);
    digitalWrite(pin_ckv_, LOW);
    pinMode(pin_ckv_, INPUT);
    digitalWrite(pin_spv_, LOW);
    pinMode(pin_spv_, INPUT);

    writeWire(0x01, 0x2f);	// Disable VCOM.
    delay(1);
    writeWire(0x01, 0x60);	// Disable other power rails.
    delay(25);
    writeWire(0x01, 0x00);	// Disable VDD.
    digitalWrite(pin_wakeup_, LOW);
    pinMode(pin_wakeup_, INPUT);
    Wire.end();
    digitalWrite(pin_scl_, LOW);
    pinMode(pin_scl_, INPUT);
    digitalWrite(pin_sda_, LOW);
    pinMode(pin_sda_, INPUT);
  }

  void begin() {
    digitalWrite(pin_oe_, HIGH);
    digitalWrite(pin_mode_, HIGH);

    digitalWrite(pin_spv_, LOW);
    digitalWrite(pin_ckv_, LOW);
    delayMicroseconds(1);
    digitalWrite(pin_ckv_, HIGH);
    digitalWrite(pin_spv_, HIGH);
  }

  void end() {
    digitalWrite(pin_mode_, LOW);
    digitalWrite(pin_oe_, LOW);
  }

  void transfer(const uint8_t *data, bool erase = false) {
    digitalWrite(pin_stl_, LOW);
    for (int i = 0; i < WIDTH / 4; i++) {
      // Convert the data from 1 pixel/byte to the native 4 pixel/byte format.
      const uint8_t index = ((data[0] << 3) | (data[1] << 2) | (data[2] << 1) | data[3]);
      data += 4;
      static const uint8_t palette[16] = {
        0b10101010,
        0b10101001,
        0b10100110,
        0b10100101,
        0b10011010,
        0b10011001,
        0b10010110,
        0b10010101,
        0b01101010,
        0b01101001,
        0b01100110,
        0b01100101,
        0b01011010,
        0b01011001,
        0b01010110,
        0b01010101,
      };
      digitalWriteByte(pin_d0_, palette[index] & (erase ? 0b11111111 : 0b01010101));
      digitalWrite(pin_cl_, HIGH);
      digitalWrite(pin_cl_, LOW);
    }
    digitalWrite(pin_stl_, HIGH);
    digitalWrite(pin_cl_, HIGH);
    digitalWrite(pin_cl_, LOW);

    digitalWrite(pin_le_, HIGH);
    digitalWrite(pin_le_, LOW);

    digitalWrite(pin_ckv_, LOW);
    delayMicroseconds(1);
    digitalWrite(pin_ckv_, HIGH);
  }

  static constexpr int WIDTH = 1024;
  static constexpr int HEIGHT = 758;

private:
  static void pinModeByte(int pin0, int mode) {
    for (int i = 0; i < 8; i++) pinMode(pin0 + i, mode);
    if (mode == INPUT) {
      REG_WRITE(GPIO_ENABLE_W1TC_REG, 0xff << pin0);
    } else if (mode == OUTPUT) {
      REG_WRITE(GPIO_ENABLE_W1TS_REG, 0xff << pin0);
    }
  }

  static uint8_t digitalReadByte(int pin0) {
    return (uint8_t)(REG_READ(GPIO_IN_REG >> pin0));
  }

  static void digitalWriteByte(int pin0, uint8_t value) {
    REG_WRITE(GPIO_OUT_REG, (REG_READ(GPIO_OUT_REG) & ~(0xff << pin0)) | (((uint32_t)value) << pin0));
  }

  static uint8_t readWire(uint8_t adrs) {
    Wire.beginTransmission(0x68);
    Wire.write(adrs);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 1);
    return Wire.read();
  }

  static void writeWire(uint8_t adrs, uint8_t data) {
    Wire.beginTransmission(0x68);
    Wire.write(adrs);
    Wire.write(data);
    Wire.endTransmission();
  }

  int pin_spv_;
  int pin_ckv_;
  int pin_mode_;
  int pin_stl_;
  int pin_oe_;
  int pin_le_;
  int pin_cl_;
  int pin_d0_;
  int pin_sda_;
  int pin_scl_;
  int pin_wakeup_;
};

#endif
