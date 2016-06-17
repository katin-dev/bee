#include <SoftwareSerial.h>
#include <MemoryFree.h>
#include <DHT.h>
#include "GSMLoc.h"
#include "SensorData.h"

#define DHTTYPE DHT11
#define SIM_RX 2
#define SIM_TX 3

#define MEASURE_INTERVAL 60000 // измерения проводятся каждую минуту
#define SIZE_AVG 15 // каждые 15 минут вычисляем среднее значение
#define SIZE_SUM 16 // 4 показания за час * 4 часа

#define SERIAL 0

DHT dhtInner(10, DHTTYPE);
DHT dhtOuter(12, DHTTYPE);

SoftwareSerial GSMport(SIM_RX, SIM_TX);

String IMEI = "";
unsigned int posted = 0;
unsigned long lastTime = 0;
// GSMLoc gsmLoc;

// История замеров (температура и влажность)
short T1[SIZE_AVG];
short T2[SIZE_AVG];

short H1[SIZE_AVG];
short H2[SIZE_AVG];

short TH1[SIZE_SUM];
short TH2[SIZE_SUM];
short HH1[SIZE_SUM];
short HH2[SIZE_SUM];

int C60 = 0;
int C6 = 0;

char post[SIZE_SUM * 13 + 21];

void setup() {
  
  delay(3000); // подождём инициализацию GSM модуля:

  #if SERIAL == 1
  Serial.begin(9600);  //скорость порта
  #endif
  
  GSMport.begin(9600);

  //GSMport.println("AT+EGMR=0,7");
  //delay(500);
  //Serial.println(ReadGSM());

  //GSMport.println("AT+EGMR=1,7,\"01311700571122\"");
  //delay(500);
  //Serial.println(ReadGSM());

  // GET IMEI
  GSMport.println("AT+GSN");
  delay(500);
  IMEI = ReadGSM();
  IMEI = IMEI.substring(9, IMEI.indexOf("\r\n", 10));

  dhtInner.begin();
  dhtOuter.begin();

  lastTime = millis();

  memset(T1, 0, SIZE_AVG);
  memset(T2, 0, SIZE_AVG);
  memset(H1, 0, SIZE_AVG);
  memset(H2, 0, SIZE_AVG);

  memset(TH1, 0, SIZE_SUM);
  memset(TH2, 0, SIZE_SUM);
  memset(HH1, 0, SIZE_SUM);
  memset(HH2, 0, SIZE_SUM);
}



void loop() 
{
  if( millis() > lastTime + MEASURE_INTERVAL )
  {

      // Сохраняем текущие значения:
      T1[C60] = dhtInner.readTemperature();
      T2[C60] = dhtOuter.readTemperature();

      H1[C60] = dhtInner.readHumidity();
      H2[C60] = dhtOuter.readHumidity();

      C60++;

      if(C60 == SIZE_AVG) {
        TH1[C6] = avg(T1, SIZE_AVG);
        TH2[C6] = avg(T2, SIZE_AVG);

        HH1[C6] = avg(H1, SIZE_AVG);
        HH2[C6] = avg(H2, SIZE_AVG);

        C6++;

        if(C6 == SIZE_SUM) {
          // пора отправить данные
          #if SERIAL == 1
          printar(TH1, SIZE_SUM);
          printar(TH2, SIZE_SUM);
          printar(HH1, SIZE_SUM);
          printar(HH2, SIZE_SUM);
          #endif

          pushData(TH1, TH2, HH1, HH2);
          
          C6 = 0;
          memset(TH1, 0, SIZE_SUM);
          memset(TH2, 0, SIZE_SUM);
          memset(HH1, 0, SIZE_SUM);
          memset(HH2, 0, SIZE_SUM);
        }

        C60 = 0;
        memset(T1, 0, SIZE_AVG);
        memset(T2, 0, SIZE_AVG);
        memset(H1, 0, SIZE_AVG);
        memset(H2, 0, SIZE_AVG);
      }

      #if SERIAL == 1
      Serial.print(C60);
      Serial.print("x");
      Serial.println(C6);
      #endif
      lastTime = millis();
  }
}

// посчитать среднее:
int avg(short * data, int length) {
  int sum = 0;
  for(int i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum / length;
}

void printar(short * data, int length) {
  String s;
  for(int i = 0; i < length; i++) {
    Serial.print(data[i]);
    Serial.print(",");
  }
  Serial.println();
}

void pushData(short * t1, short * t2, short * h1, short * h2) {
  
    // get location
    #if SERIAL == 1 
    Serial.println("Try to get location..."); 
    #endif
    // getLocation();
    #if SERIAL == 1 
    Serial.println("location complete"); 
    #endif

    char str[4];
    char comma[2] = ",";
    unsigned int p = 0;

    memset(post, '\0', SIZE_SUM * 13 + 1);

    strcpy(post, IMEI.c_str());
    p += IMEI.length();

    strcpy(post + p, "&");
    p++;

    /* strcpy(post + p, gsmLoc.latitude);
    p += strlen(gsmLoc.latitude);

    strcpy(post + p, "&");
    p++;

    strcpy(post + p, gsmLoc.longitude);
    p += strlen(gsmLoc.longitude);

    strcpy(post + p, "&");
    p++;
    */
    
    for(int i = 0; i < SIZE_SUM; i++) {      
      itoa(t1[i], str, 10);str[2] = ',';str[3] = '\0';
      strcpy( post + p,  str);
      p += 3;

      itoa(t2[i], str, 10); str[2] = ','; str[3] = '\0';
      strcpy( post + p,  str);
      p += 3;

      itoa(h1[i], str, 10); str[2] = ','; str[3] = '\0';
      strcpy( post + p,  str);
      p += 3;

      itoa(h2[i], str, 10); str[2] = '\0'; str[3] = '\0';
      strcpy( post + p,  str);
      p += 2;

      if(i + 1 < SIZE_SUM) {
        strcpy( post + p,  "|");
        p ++;
      }
    }

    #if SERIAL == 1
    Serial.println(post);
    #endif

    gprs_init();
    gprs_send(post);
    gprs_stop();
    
    #if SERIAL == 1
    Serial.println("Posted counter: " + String(posted));
    Serial.print("freeMemory()=");
    Serial.println(freeMemory());
    #endif

    if (GSMport.available()) {  //если GSM модуль что-то послал нам, то
      ReadGSM();
      #if SERIAL == 1
      Serial.println(ReadGSM());  //печатаем в монитор порта пришедшую строку
      #endif
    }
}

/* void getLocation() {
  
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
  #if SERIAL == 1
  Serial.println(result);
  #endif
  result.replace("+CIPGSMLOC: ", "");
  result.trim();


  
  
  char source[100];
  memset(source, 0, 100);
  strcpy(source, result.c_str());

  int i = 0;
  char * wrd = strtok(source, ",");
  while ( wrd != NULL ) {
    #if SERIAL == 1
    Serial.println(wrd);
    #endif
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
} */

void gprs_init() {  //Процедура начальной инициализации GSM модуля
  int d = 500;
  int ATsCount = 7;
  String ATs[] = {  //массив АТ команд
    "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",  //Установка настроек подключения
    "AT+SAPBR=3,1,\"APN\",\"internet\"",
    "AT+SAPBR=3,1,\"USER\",\"user\"",
    "AT+SAPBR=3,1,\"PWD\",\"password\"",
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

void gprs_stop() 
{
  GSMport.println("AT+SAPBR=0,1");  //посылаем в GSM модуль
  delay(500);
  
  String s = ReadGSM();
  #if SERIAL == 1
  Serial.println(s);  //показываем ответ от GSM модуля
  #endif
}

void gprs_send(char * data) {  //Процедура отправки данных на сервер
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
  
  GSMport.println("AT+HTTPDATA=" + String(strlen(data)) + ",2000");
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
  delay(d);
  
  String response = ReadGSM();
  while(response.indexOf("+HTTPACTION:") == -1 && response.indexOf("ERROR") == -1) {
    delay(d);
    response = ReadGSM();
  }
  
  #if SERIAL == 1
    Serial.println(response);
  #endif
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
