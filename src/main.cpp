#include <Arduino.h>
#include <TFT_eSPI.h> // pisanje na display
#include <WiFi.h> // wifi
#include <HTTPClient.h> // http requesti
#include <WiFiClientSecure.h> // https
#include <ArduinoJson.h> // json
#include <Wire.h> // I2C
#include <Adafruit_CST8XX.h> // touchscreen

// WiFi omrežje
const char* ssid = "Nick XV";
const char* password = "petevrov";

// refresh rate
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000;

// postaje
struct Postaja {
  const char* buttonName;
  const char* stationName;
  const char* stationCode;
};
Postaja postaje[] = {
  {"Barje", "Barje ->c", "604023"},
  {"Konzo", "Konzorcij c->", "601012"},
  {"Gornji", "Gornji trg c->", "602102"}
};
int izbranaPostaja = 0;

TFT_eSPI tft = TFT_eSPI(); // display
Adafruit_CST8XX touch = Adafruit_CST8XX(); // touch

//test displaya
void nastaviDisplay(){
  // backlight
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(20, 40);
  tft.println("LPP Display");

  tft.setCursor(20, 80);
  tft.println("Display deluje");

  tft.drawRect(10, 10, 300, 220, TFT_GREEN);

  Serial.println("Display test končan.");
}

// povezovanje na WiFi
void connectToWiFi() {
  // izpis na zaslon
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 40);
  tft.println("Povezujem WiFi...");

  // izpis na serijca
  Serial.print("Povezovanje na ");
  Serial.println(ssid);

  // proba povezat
  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    tft.print(".");
    attempts++;
  }

  // se je povezal al ne
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi povezan!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 40);
    tft.println("WiFi povezan!");

    tft.setCursor(20, 80);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi povezava neuspela.");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 40);
    tft.println("WiFi povezava neuspela.");

    tft.setCursor(20, 80);
    tft.print("Wifi status: ");
    tft.println(WiFi.status());
  }
}

void drawArrivals(JsonDocument& doc);

// testira http request
void HttpRequest() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  // String url = "https://mestnipromet.cyou/";
  String url = "https://data.lpp.si/api/station/arrival?station-code="; // url brez kode postaje
  url += postaje[izbranaPostaja].stationCode; // urlju doda kodo postaje

  http.begin(client, url);
  int httpCode = http.GET(); //pošlje request
  //kaj je stran vrnla na request
  Serial.print("HTTP koda: ");
  Serial.println(httpCode);

  // inardimo stream, ker je zadeva predolga za shranjevanje
  WiFiClient* stream = http.getStreamPtr();

  // json
  JsonDocument doc; // ustvarmo json document
  DeserializationError error = deserializeJson(doc, *stream); // json iz streama razlčenmo v doc

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 40);
    tft.println("JSON failed");
    return;
  }

  drawArrivals(doc);

  http.end();
}

void drawStationButtons(){
  int buttonY = 210;
  int buttonH = 28;
  int buttonW = 100;

  for (int i = 0; i < 3; i++){
    int x = 10 + i * 105;

    if (i == izbranaPostaja) {
      tft.fillRect(x, buttonY, buttonW, buttonH, TFT_GREEN);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
    } else {
      tft.fillRect(x, buttonY, buttonW, buttonH, TFT_BLACK); // "pobrise" zeleno polnilo
      tft.drawRect(x, buttonY, buttonW, buttonH, TFT_WHITE); // nariše belo obrobo
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    tft.setCursor(x + 8, buttonY + 7);
    tft.print(postaje[i].buttonName);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void handleStationButtonTouch(int touchX, int touchY) {
  int buttonY = 210;
  int buttonH = 28;
  int buttonW = 100;

  for (int i = 0; i < 3; i++) {
    int x = 10 + i * 105;

    // je bil klik znotraj območja?
    bool insideX = touchX >= x && touchX <= x + buttonW;
    bool insideY = touchY >= buttonY && touchY <= buttonY + buttonH;

    // kliknjen je bil gumb i
    if (insideX && insideY) {
      izbranaPostaja = i;

      Serial.print("izbrana postaja: ");
      Serial.println(postaje[izbranaPostaja].stationName);

      drawStationButtons();
      HttpRequest(); //zdej je nastavljena izbrana postaja, zato šeenkrat nardimo request, da bo dodal tapravo kodo postaje
      lastUpdate = millis(); // ker smo nardil request smo ubistvu refreshal podatke
      break;
    }
  }
}

void drawArrivals(JsonDocument& doc) {
  const char* stationName = doc["data"]["station"]["name"];

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("LPP arrivals");
  tft.setCursor(10, 40);
  tft.println(stationName);
  tft.drawLine(10, 65, 310, 65, TFT_GREEN);

  JsonArray arrivals = doc["data"]["arrivals"];

  int y = 80;
  int count = 0;

  for (JsonObject arrival : arrivals) {
    const char* routeName = arrival["route_name"];
    int etaMin = arrival["eta_min"];

    tft.setCursor(10, y);
    tft.print(routeName);

    tft.setCursor(80, y);
    tft.print(etaMin);
    tft.println(" min");

    y += 28;
    count++;

    if (count >= 5) {
      break;
    }
  }

  drawStationButtons();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  nastaviDisplay();

  Wire.begin(33, 32);

  // prverimo ce esp komunicira s tocuh kontorlerjem
  if(!touch.begin()){
    Serial.println("Touch ni najden");
    tft.setCursor(20, 120);
    tft.println("Touch ni najden");
  } else {
    Serial.println("Touch najden");
    tft.setCursor(29, 120);
    tft.println("touch najden");
  }

  connectToWiFi();
  HttpRequest();
  lastUpdate = millis();
}

void loop() {
  // refreshanje podatkov
  if (millis() - lastUpdate > updateInterval){
    HttpRequest();
    lastUpdate = millis();
  }

  // prevrjanje dotikov
  if (touch.touched()){
    CST_TS_Point p = touch.getPoint(0);
    Serial.print("x: ");
    Serial.println(p.x);
    Serial.print("y: ");
    Serial.println(p.y);

    int screenX = p.y; // ker imamo zaslon ležeče, moramo zamenjati x in y
    int screenY = 240 - p.x; // visina zaslona je 240
    handleStationButtonTouch(screenX, screenY); // glede na lokacijo dotika popravimo gumbe

    delay(200);
  }
}