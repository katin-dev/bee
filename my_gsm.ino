#include <SoftwareSerial.h>
#include <MemoryFree.h>
#include <DHT.h>
#include "GSMLoc.h"
#include "SensorData.h"

#define DHTTYPE DHT11
#define SIM_RX 2
#define SIM_TX 3

#define SERIAL 0

DHT dhtInner(10, DHTTYPE);
DHT dhtOuter(12, DHTTYPE);

SoftwareSerial GSMport(SIM_RX, SIM_TX);

SensorData s1, s2, s1Prev, s2Prev;

String IMEI = "";
String tmpStr = "";
const char * imei;
const char * tmp_str;
GSMLoc gsmLoc;
unsigned int posted = 0;
unsigned int lastSended = 0;


// История замеров (температура и влажность)
int T1[180];
int T2[180];

int H1[180];
int H2[180];

int TH1[6];
int TH2[6];
int HH1[6];
int HH2[6];

int C180 = 0;
int C6 = 0;

void setup() {
  
  delay(3000); // подождём инициализацию GSM модуля:

  #if SERIAL == 1
  Serial.begin(9600);  //скорость порта
  #endif
  
  GSMport.begin(9600);

  // GET IMEI
  GSMport.println("AT+GSN");
  delay(500);
  IMEI = ReadGSM();
  IMEI = IMEI.substring(9, IMEI.indexOf("\r\n", 10));

  // Init Internet connection
  gprs_init();

  dhtInner.begin();
  dhtOuter.begin();

  lastSended = millis();

  memset(T1, 0, 180);
  memset(T2, 0, 180);
  memset(H1, 0, 180);
  memset(H2, 0, 180);

  memset(TH1, 0, 6);
  memset(TH2, 0, 6);
  memset(HH1, 0, 6);
  memset(HH2, 0, 6);
}



void loop() {

  s1.t = dhtInner.readTemperature();
  s1.h = dhtInner.readHumidity();
  s2.t = dhtOuter.readTemperature();
  s2.h = dhtOuter.readHumidity();

  if(
      // Действуем, если какие-то из показателей изменились:
      ( s1.t != s1Prev.t || s1.h != s1Prev.h ||
        s2.t != s2Prev.t || s2.h != s2Prev.h
      ) && millis() > lastSended + 60000
    )
    {

      // Сохраняем текущие значения:
      T1[C180] = s1.t;
      T2[C180] = s2.t;

      H1[C180] = s1.h;
      H2[C180] = s2.h;

      C180++;

      if(C180 == 180) {
        TH1[C6] = avg(T1, 180);
        TH2[C6] = avg(T2, 180);

        HH1[C6] = avg(H1, 180);
        HH2[C6] = avg(H2, 180);

        C6++;

        if(C6 == 6) {
          // пора отправить данные
          C6 = 0;
        }

        C180 = 0;
      }
      
    // get location
    #if SERIAL == 1
    Serial.println("Try to get location...");
    #endif
    getLocation();
    #if SERIAL == 1
    Serial.println("location complete");
    #endif
    
    String post = 
        IMEI + "&" + 
        String(gsmLoc.latitude) + "&" + 
        String(gsmLoc.longitude) + "&" + 
        String(s1.t) + "&" +
        String(s1.h) + "&" +
        String(s2.t) + "&" +
        String(s2.h);
                  

    s1Prev = s1;
    s2Prev = s2;

    gprs_send(post);
    lastSended = millis();
    posted ++;
    
    #if SERIAL == 1
    Serial.println("Posted counter: " + String(posted));
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());
    #endif
    
    delay(100);
  }
  if (GSMport.available()) {  //если GSM модуль что-то послал нам, то
    ReadGSM();
    #if SERIAL == 1
    Serial.println(ReadGSM());  //печатаем в монитор порта пришедшую строку
    #endif
  }
  delay(100);
}

// посчитать среднее:
int avg(int * data, int length) {
  int sum = 0;
  for(int i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum / length;
}

void printar(int * data, int length) {
  String s;
  for(int i = 0; i < length; i++) {
    Serial.print(i);
    Serial.print(",");
  }
  Serial.println();
}

void getLocation() {
  
  String cmd = "AT+CIPGSMLOC=1,1";
  String cmdName = "CIPGSMLOC";
  String tmpStr;
  int pos = 0;
  GSMport.println(cmd);
  //Serial.println("GET LOCATION:");
  do {
    delay(250);
    tmpStr = ReadGSM();
  } while ( (pos = tmpStr.indexOf("+" + cmdName + ":")) == -1 );

  String result = tmpStr.substring(pos, tmpStr.indexOf("\r\n", pos));
  result.replace("+CIPGSMLOC: ", "");
  result.trim();


  char source[100];
  memset(source, 0, 100);
  strcpy(source, result.c_str());

  int i = 0;
  char * wrd = strtok(source, ",");
  while ( wrd != NULL ) {
    switch (i++)  {
      case 0:
        gsmLoc.code = atoi(wrd); break;
      case 1:
        strcpy(gsmLoc.latitude, wrd);
        break;
      case 2:
        strcpy(gsmLoc.longitude, wrd);
        break;
      case 3:
        strcpy(gsmLoc.date, wrd);
        break;
      case 4:
        strcpy(gsmLoc.time, wrd);
        break;
    }
    wrd = strtok(NULL, ",");
  }
}

void gprs_init() {  //Процедура начальной инициализации GSM модуля
  int d = 500;
  int ATsCount = 7;
  String ATs[] = {  //массив АТ команд
    "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",  //Установка настроек подключения
    "AT+SAPBR=3,1,\"APN\",\"internet\"",
    "AT+SAPBR=3,1,\"USER\",\"tele2\"",
    "AT+SAPBR=3,1,\"PWD\",\"tele2\"",
    "AT+SAPBR=1,1",  //Устанавливаем GPRS соединение
    "AT+HTTPINIT",  //Инициализация http сервиса
    "AT+HTTPPARA=\"CID\",1"  //Установка CID параметра для http сессии
  };
  int ATsDelays[] = {6, 1, 1, 1, 3, 3, 1}; //массив задержек
  #if SERIAL == 1
    Serial.println("GPRG init start");
  #endif
  for (int i = 0; i < ATsCount; i++) {
    
    #if SERIAL == 1
    Serial.println(ATs[i]);  //посылаем в монитор порта
    #endif
    
    GSMport.println(ATs[i]);  //посылаем в GSM модуль
    delay(d * ATsDelays[i]);

    String s = ReadGSM();
    #if SERIAL == 1
    Serial.println(s);  //показываем ответ от GSM модуля
    #endif
    delay(d);
  }
  
  #if SERIAL == 1
    Serial.println("GPRG init complete");
  #endif  
}

void gprs_send(String data) {  //Процедура отправки данных на сервер
  //отправка данных на сайт
  int d = 500;
  String s;
  
  GSMport.println("AT+HTTPPARA=\"URL\",\"http://gourry.ru/bee.php\"");
  delay(d);
  s = ReadGSM();
  #if SERIAL == 1
    Serial.println(s);
  #endif  
  
  
  //GSMport.println("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"");
  //delay(d);
  //ReadGSM();
  
  GSMport.println("AT+HTTPDATA=" + String(data.length()) + ",2000");
  delay(d);
  s = ReadGSM();  // DOWNLOAD
  #if SERIAL == 1
    Serial.println(s);
  #endif  
  
  GSMport.print(data);
  delay(2500);
  s = ReadGSM();
  #if SERIAL == 1
    Serial.println(s);
  #endif  
  
  GSMport.println("AT+HTTPACTION=1");
  delay(d * 5);
  
  String response = ReadGSM();
  if( response.indexOf("ERROR") != -1 ) {
    #if SERIAL == 1
    Serial.println("POST FAIL with err:\r\n" + response);
    #endif
    gprs_init();
  } else {
    #if SERIAL == 1
    Serial.println("POST OK");
    #endif
  }
}

String ReadGSM() {  //функция чтения данных от GSM модуля
  int c;
  String v;
  while (GSMport.available()) {  //сохраняем входную строку в переменную v
    c = GSMport.read();
    v += char(c);
    delay(10);
  }
  return v;
}
