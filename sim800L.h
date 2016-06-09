#include <SoftwareSerial.h>

class Sim800L {
  private: 
    SoftwareSerial * gsm;
  public: 
    Sim800L(int RX, int TX);
    void init();
};

