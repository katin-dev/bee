#include "Sim800L.h";

Sim800L::Sim800L(int RX, int TX) {
  gsm = new SoftwareSerial(RX, TX);
  gsm->begin(9600);
}

void Sim800L::init() {
  gsm->println("AT+GSN");
}

