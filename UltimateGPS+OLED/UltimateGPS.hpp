#pragma once

#include <MT3339.hpp>

class UltimateGps : public MT3339 {
public:
  UltimateGps(int8_t pinEn, SerialPort &uart) : MT3339(uart), PinEn(pinEn) { }

  void begin() override {
    MT3339::begin();
    pinMode(this->PinEn, OUTPUT);
  }

  void turnOn() override {
    MT3339::turnOn();
    digitalWrite(this->PinEn, LOW);
  }

  void turnOff() override {
    MT3339::turnOff();
    digitalWrite(this->PinEn, HIGH);
  }

  bool isOn() override {
    return (MT3339::isOn() && digitalRead(this->PinEn) == LOW);
  }

private:
  const int8_t PinEn;
};
