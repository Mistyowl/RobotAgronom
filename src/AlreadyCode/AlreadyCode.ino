#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <GyverStepper.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Настройка IP адреса
IPAddress local_ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

Preferences prefs;
WebServer server(80);

// Конфигурация шагового двигателя
GStepper<STEPPER4WIRE> stepper(2038, 26, 14, 27, 12);

// Конфигурация DC моторов
#define MOTOR_A_IN1 25
#define MOTOR_A_IN2 23
#define MOTOR_B_IN3 32
#define MOTOR_B_IN4 35

// Подключение SD-карты
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK 18
#define SD_CS 05

// Подключение Soil Sensor датчика 7 in 1
#define RE 4 
#define RXD2 17
#define TXD2 16
// Адресы регисторов чтения
const byte humi[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0xe84, 0x0a }; // Влажность
const byte temp[] = { 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xd5, 0xca }; // Температура
const byte cond[] = { 0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x25, 0xca }; // Электропроводность
const byte phph[] = { 0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x74, 0x0a }; // Водородный показатель
const byte nitro[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xec5, 0xcb }; // Азот
const byte phos[] = { 0x01, 0x03, 0x00, 0x05, 0x00, 0x01, 0xe94, 0x0b }; // Фосфор
const byte pota[] = { 0x01, 0x03, 0x00, 0x06, 0x00, 0x01, 0xe64, 0x0b }; // Калий
const byte sali[] = { 0x01, 0x03, 0x00, 0x07, 0x00, 0x01, 0xe35, 0xcb };  // Соленость
const byte tds[] = { 0x01, 0x03, 0x00, 0x08, 0x00, 0x01, 0xe05, 0xc8 }; // Общее количество растворенных твердых веществ
byte values[11]; // Переменная для хранения значения NPK

// Состояния системы
enum OperationState {
  IDLE,
  CALIBRATING,
  MOVING,
  LOWERING_PROBE,
  MEASURING,
  RAISING_PROBE
};

// Глобальные переменные
OperationState currentState = IDLE;
float calibratedSpeed = 0.5; // м/с
unsigned long movementEndTime = 0;
const float calibrationTime = 10.0; // секунд
bool sdInitialized = false; // SD-карта

// Параметры миссии
float totalDistance = 0;
int measurementsCount = 0;
int currentMeasurement = 0;
float segmentDistance = 0;

void setup() {
  Serial.begin(115200);

  // Датчик
  Serial1.begin(4800, SERIAL_8N1, RXD2, TXD2);
  pinMode(RE, OUTPUT);
  digitalWrite(RE, LOW);
  
  // Настройка выводов
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN3, OUTPUT);
  pinMode(MOTOR_B_IN4, OUTPUT);

  stepper.setRunMode(KEEP_SPEED);
  // можно установить скорость
  stepper.setSpeed(500);    // в шагах/сек
  // режим следования к целевй позиции
  stepper.setRunMode(FOLLOW_POS);
  // можно установить позицию
  stepper.setTarget(-2024);    // в шагах
  // установка макс. скорости в градусах/сек
  // stepper.setMaxSpeedDeg(400);
  
  // установка макс. скорости в шагах/сек
  stepper.setMaxSpeed(500);
  // установка ускорения в шагах/сек/сек
  stepper.setAcceleration(300);
  // отключать мотор при достижении цели
  stepper.autoPower(true);
  // включить мотор (если указан пин en)
  stepper.enable();
  
  // Загрузка калибровки
  prefs.begin("robot-data");
  calibratedSpeed = prefs.getFloat("speed", 0.5);
  prefs.end();

  // Настройка Wi-Fi
  WiFi.softAP("RobotControl", "12345678");
  
  // Маршруты веб-сервера
  server.on("/", handleRoot);
  server.on("/start-calibration", handleStartCalibration);
  server.on("/set-calibration", handleSetCalibration);
  server.on("/start-mission", handleStartMission);
  server.on("/stop", handleStop);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if(!SD.begin(SD_CS)) {
    Serial.println("SD Card Mount Failed");
    sdInitialized = false;
  } else {
    sdInitialized = true;
    createHeader(); // Создать файл с заголовками
  }
  
  server.begin();
}

void loop() {
  server.handleClient();
  processOperations();
}

void createHeader() {
  if(sdInitialized) {
    if(!SD.exists("/data.json")) {
      File file = SD.open("/data.json", FILE_WRITE);
      if(file) {
        file.println("[]"); // Инициализируем пустой массив
        file.close();
      }
    }
  }
}

void logDataToSD(float humi, float temp, float cond, float ph, 
                float nitro, float phos, float pota, 
                float sali, float tds) {
  if(sdInitialized) {
    StaticJsonDocument<512> doc;
    
    // Заполняем данные
    doc["timestamp"] = millis();
    doc["humi"] = humi;
    doc["temp"] = temp;
    doc["cond"] = cond;
    doc["ph"] = ph;
    doc["nitro"] = nitro;
    doc["phos"] = phos;
    doc["pota"] = pota;
    doc["sali"] = sali;
    doc["tds"] = tds;

    // Открываем файл для записи
    File file = SD.open("/data.json", FILE_APPEND);
    if(file) {
      // Сериализуем JSON в компактном формате
      serializeJson(doc, file);
      file.println(); // Добавляем перевод строки
      file.close();
      Serial.println("JSON logged to SD");
    }
  }
}

void processOperations() {
  switch(currentState) {
    case CALIBRATING:
      if(millis() > movementEndTime) {
        stopMotors();
        currentState = IDLE;
      }
      break;
      
    case MOVING:
      if(millis() > movementEndTime) {
        stopMotors();
        startLoweringProbe();
      }
      break;
      
    case LOWERING_PROBE:
      if(!stepper.tick()) {
        currentState = MEASURING;
        startMeasurement();
      }
      break;
      
    case MEASURING:
      if(millis() > movementEndTime) {
        startRaisingProbe();
      }
      break;
      
    case RAISING_PROBE:
      if(!stepper.tick()) {
        currentMeasurement++;
        if(currentMeasurement < measurementsCount) {
          startNextSegment();
        } else {
          currentState = IDLE;
        }
      }
      break;
  }
}

// ================== Управление DC моторами ==================
void startMotorsForward() {
  digitalWrite(MOTOR_A_IN1, HIGH);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN3, HIGH);
  digitalWrite(MOTOR_B_IN4, LOW);
}

void stopMotors() {
  digitalWrite(MOTOR_A_IN1, LOW);
  digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN3, LOW);
  digitalWrite(MOTOR_B_IN4, LOW);
}

// ================== Калибровка скорости ==================
void handleStartCalibration() {
  if(currentState == IDLE) {
    startMotorsForward();
    movementEndTime = millis() + (calibrationTime * 1000);
    currentState = CALIBRATING;
    server.send(200, "text/plain", "Калибровка начата! Двигайтесь 10 сек");
  }
}

void handleSetCalibration() {
  if(currentState == CALIBRATING && server.hasArg("d")) {
    float realDistance = server.arg("d").toFloat();
    calibratedSpeed = realDistance / calibrationTime;
    
    prefs.begin("robot-data");
    prefs.putFloat("speed", calibratedSpeed);
    prefs.end();
    
    currentState = IDLE;
    server.send(200, "text/plain", "Скорость: " + String(calibratedSpeed) + " м/с");
  }
}

// ================== Управление зондом ==================
void startLoweringProbe() {
  stepper.setTarget(-1000, ABSOLUTE); // Опускание на полную глубину 50000
  currentState = LOWERING_PROBE;
}

void startRaisingProbe() {
  stepper.setTarget(0, ABSOLUTE); // Подъем в исходное положение
  currentState = RAISING_PROBE;
}

void startMeasurement() {
  movementEndTime = millis() + 5000; // 5 секунд на замер
  float val_humi, val_temp, val_cond, val_phph, val_nitro, val_phos, val_pota, val_soli, val_tds;
  // ================== Влажность ==================
  Serial.print("Humidity: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(humi); i++) Serial1.write(humi[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_humi = int(values[3] << 8 | values[4]);
  val_humi = val_humi / 10;
  Serial.print(" = ");
  Serial.print(val_humi, 1);
  Serial.println(" %");
  delay(200);
  // ================== Температура ==================
  Serial.print("Temperature: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(temp); i++) Serial1.write(temp[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_temp = int(values[3] << 8 | values[4]);
  val_temp = val_temp / 10;
  Serial.print(" = ");
  Serial.print(val_cond, 1);
  Serial.println(" deg.C");
  delay(200);
  // ================== Электропроводность ==================
  Serial.print("Conductivity: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(cond); i++) Serial1.write(cond[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_cond = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_cond);
  Serial.println(" uS/cm");
  delay(200);
  // ================== Водородный показатель ==================
  Serial.print("pH: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(phph); i++) Serial1.write(phph[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_phph = int(values[3] << 8 | values[4]);
  val_phph = val_phph / 10;
  Serial.print(" = ");
  Serial.println(val_phph, 1);
  delay(200);
  // ================== Азот ==================
  Serial.print("Nitrogen: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(nitro); i++) Serial1.write(nitro[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_nitro = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_nitro);
  Serial.println(" mg/L");
  delay(200);
  // ================== Фосфор ==================
  Serial.print("Phosphorus: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(phos); i++) Serial1.write(phos[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_phos = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_phos);
  Serial.println(" mg/L");
  delay(200);
  // ================== Калий ==================
  Serial.print("Potassium: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(pota); i++) Serial1.write(pota[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_pota = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_pota);
  Serial.println(" mg/L");
  delay(200);
  // ================== Соленость ==================
  Serial.print("Salinity: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(sali); i++) Serial1.write(sali[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_soli = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_soli);
  Serial.println(" g/L");
  delay(200);
  // ================== Общее количество растворенных твердых веществ ==================
  Serial.print("TDS: ");
  digitalWrite(RE, HIGH);
  delay(10);
  for (uint8_t i = 0; i < sizeof(tds); i++) Serial1.write(tds[i]);
  Serial1.flush();
  digitalWrite(RE, LOW);
  delay(100);
  for (byte i = 0; i < 7; i++) {
    values[i] = Serial1.read();
  }
  val_tds = int(values[3] << 8 | values[4]);
  Serial.print(" = ");
  Serial.print(val_tds);
  Serial.println(" mg/L");
  delay(200);

  if(sdInitialized) {
    // Читаем существующий файл
    File file = SD.open("/data.json", FILE_READ);
    if(file) {
      // Парсим JSON массив
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, file);
      file.close();

      if(!error) {
        // Добавляем новые данные
        JsonObject newEntry = doc.createNestedObject();
        newEntry["timestamp"] = millis();
        newEntry["humi"] = val_humi;
        newEntry["temp"] = val_temp;
        newEntry["cond"] = val_cond;
        newEntry["phph"] = val_phph;
        newEntry["nitro"] = val_nitro;
        newEntry["phos"] = val_phos;
        newEntry["pota"] = val_pota;
        newEntry["soli"] = val_soli;
        newEntry["tds"] = val_tds;

        // Перезаписываем файл
        file = SD.open("/data.json", FILE_WRITE);
        if(file) {
          serializeJsonPretty(doc, file); // Красивое форматирование
          file.close();
        }
      }
    }
  }

  currentState = MEASURING;
  // Здесь будет вызов вашей функции измерения
}

// ================== Логика миссии ==================
void handleStartMission() {
  if(currentState == IDLE && server.hasArg("distance") && server.hasArg("measurements")) {
    totalDistance = server.arg("distance").toFloat();
    measurementsCount = server.arg("measurements").toInt();
    
    if(measurementsCount > 0 && totalDistance > 0) {
      segmentDistance = totalDistance / measurementsCount;
      currentMeasurement = 0;
      startNextSegment();
      server.send(200, "text/plain", "Миссия начата!");
    }
  }
}

void startNextSegment() {
  unsigned long duration = (segmentDistance / calibratedSpeed) * 1000;
  startMotorsForward();
  movementEndTime = millis() + duration;
  currentState = MOVING;
}

void handleStop() {
  currentState = IDLE;
  stopMotors();
  // probeStepper.step(0); // Прервать текущее движение зонда
  stepper.stop();
  server.send(200, "text/plain", "Операция прервана");
}

// ================== Веб-интерфейс ==================
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE HTML>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
      body {font-family: sans-serif;}
      h1 {margin: 0; text-align: center;}
      h3 {margin: 10px 0;}
      input, button {width: 100%; padding: 10px; margin: 5px 0;}
      button {background: #4CAF50; color: white; border: none;}
      input {box-sizing: border-box;}
      .container {max-width: 600px; margin: auto; padding: 20px;}
      .section {margin: 20px 0; padding: 15px; border: 1px solid #ddd;}
    </style>
    <script>
      function send(url, params) {
        fetch(url + (params || ''))
          .then(r => r.text())
          .then(t => alert(t));
      }
    </script>
  </head>
  <body>
    <div class="container">
      <h1>Управление агроботом</h1>
      
      <div class="section">
        <h3>Калибровка скорости</h3>
        <button onclick="send('/start-calibration')">Калибровочный заезд (10 сек)</button>
        <input type="number" id="calibDistance" placeholder="Реальное расстояние (м)">
        <button onclick="send('/set-calibration?d=' + document.getElementById('calibDistance').value)">
          Установить калибровку
        </button>
        <p>Текущая скорость: )rawliteral" + String(calibratedSpeed) + R"rawliteral( м/с</p>
      </div>

      <div class="section">
        <h3>Программа мисси
        и</h3>
        <input type="number" id="distance" placeholder="Общая дистанция (м)" step="0.1">
        <input type="number" id="measurements" placeholder="Количество замеров">
        <button onclick="send('/start-mission?distance=' + 
          document.getElementById('distance').value + 
          '&measurements=' + document.getElementById('measurements').value)">
          Старт миссии
        </button>
        <button onclick="send('/stop')">Экстренный стоп</button>
        <p>Статус SD-карты: )rawliteral" + (sdInitialized ? String("OK") : String("Ошибка")) + R"rawliteral(</p>
      </div>
    </div>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}