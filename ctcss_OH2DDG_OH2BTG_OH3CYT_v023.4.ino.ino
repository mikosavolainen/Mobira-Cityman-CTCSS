// Aaro OH2DDG Versio
// 30.10.2024 v010 Eero OH2BTG Versio
// 01.11.2024 v011 Eero version
// 02.11.2024 v014 Code cleaned
// 03.11.2024 v015 producing nice sine wave when wifi and server is shut down
// 04.11.2024 v016 Mapping ctcss values to microSeconds. Done serial debug port
// 05.11.2024 v017 Working SW
// 09.11.2024 v020 Minor code cleaned
// OH3CYT Versio
// 23.02.2026 v021 Päivitetty graafinen ja responsiivinen käyttöliittymä, Amplitudin & Offsetin säätö
// 24.02.2026 v022 Serial monitori lisätty webisivulle
// 28.02.2026 v023.4 Korjattu WiFi-sammutus (PTT & Timeout) ja lisätty manuaalinen nappi
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <math.h>
#include "esp_wifi.h" 

String webLog = "";
const int MAX_LOG_SIZE = 1500;

void webPrint(String msg) {
  Serial.print(msg);
  webLog += msg;
  if (webLog.length() > MAX_LOG_SIZE) webLog = webLog.substring(webLog.length() - MAX_LOG_SIZE);
}

void webPrintln(String msg) {
  Serial.println(msg);
  webLog += msg + "\n";
  if (webLog.length() > MAX_LOG_SIZE) webLog = webLog.substring(webLog.length() - MAX_LOG_SIZE);
}

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long duration = 300000; // 5 minuuttia
int counter = 0;
bool serverStarted = false;
bool TX_ON = false;

const char* ap_ssid = "CTCSS_aanet";
const char* ap_password = "12345678";

const int dacPin = 17;
const int inputPin = 9;
const int resolution = 64;

int amplitude = 127;
int offset = 128;

const float epsilon = 0.001;
float frequency_dac = 118;
float frequency_dac_saved = 10;
float number = 29;
float inquiryValue = 118;
float outputValue = 128;

IPAddress local_IP(192, 168, 10, 1);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

const float ctcssFrequencies[] = {
  67.0, 71.9, 74.4, 77.0, 79.7, 82.5, 85.4, 88.5, 91.5, 94.8, 97.4, 100.0,
  103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8, 136.5, 141.3, 146.2,
  151.4, 156.7, 162.2, 167.9, 173.8, 179.9, 186.2, 192.8, 203.5, 210.7, 218.1,
  225.7, 233.6, 241.8, 250.3
};

const float ctcss_to_micro_s[] = {
  229, 212, 205, 197, 191, 185, 178, 172, 166, 161, 156, 151, 147, 141, 137, 132, 128, 122, 118, 114,
  110, 106, 102, 99, 95, 92, 98, 86, 83, 80, 77, 73, 70, 67, 65, 63, 61, 58
};

const int LUT_SIZE = sizeof(ctcssFrequencies) / sizeof(ctcssFrequencies[0]);
int currentFrequencyIndex = 0;
int sineLUT[resolution];

// --- APUFUNKTIO WIFIN TÄYDELLISEEN SAMMUTTAMISEEN ---
void stopWiFi() {
  if (!serverStarted) return;
  webPrintln("Sammutetaan WiFi");
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop(); 
  serverStarted = false;
}

float mapToLUT(float inputValue) {
  for (int i = 0; i < LUT_SIZE; i++) {
    if (abs(inputValue - ctcssFrequencies[i]) < epsilon) {
      return ctcss_to_micro_s[i];
    }
  }
  return -1.0;
}

void generateSineLUT() {
  for (int i = 0; i < resolution; i++) {
    float angle = (2.0 * PI * i) / resolution;
    int val = (int)(amplitude * sin(angle) + offset);
    if (val > 255) val = 255;
    if (val < 0) val = 0;
    sineLUT[i] = val;
  }
}

void handleLog() {
  server.send(200, "text/plain", webLog);
}

void handleShutdown() {
  server.send(200, "text/html", "<html><body><h1>WiFi Sammutettu</h1><p>Bootti vaaditaan WiFin palauttamiseksi.</p></body></html>");
  delay(500);
  stopWiFi();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='fi'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>CTCSS Hallinta</title>";
  html += "<style>";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #e9ecef; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }";
  html += ".card { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.1); width: 100%; max-width: 450px; }";
  html += "h1 { color: #333; font-size: 24px; text-align: center; margin-bottom: 25px; }";
  html += "label { font-weight: 600; color: #555; display: flex; justify-content: space-between; margin-top: 15px; font-size: 14px; }";
  html += "select, input[type=range] { width: 100%; margin-top: 8px; padding: 10px; border-radius: 6px; border: 1px solid #ccc; font-size: 16px; box-sizing: border-box; }";
  html += "input[type=submit], .btn-off { color: white; border: none; padding: 14px; border-radius: 6px; width: 100%; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 25px; transition: background 0.3s; text-decoration: none; display: block; text-align: center; box-sizing: border-box; }";
  html += "input[type=submit] { background: #007bff; } input[type=submit]:hover { background: #0056b3; }";
  html += ".btn-off { background: #dc3545; margin-top: 20px; } .btn-off:hover { background: #a71d2a; }";
  html += ".status { text-align: center; margin-top: 20px; font-size: 14px; color: #444; background: #f8f9fa; padding: 12px; border-radius: 6px; border: 1px solid #ddd; }";
  html += ".terminal { background-color: #121212; color: #00ff00; font-family: 'Courier New', Courier, monospace; padding: 12px; border-radius: 6px; margin-top: 10px; height: 160px; overflow-y: auto; font-size: 12px; line-height: 1.4; border: 2px solid #333; box-shadow: inset 0 0 10px rgba(0,255,0,0.1); text-align: left; white-space: pre-wrap; word-wrap: break-word;}";
  html += ".term-title { font-size: 11px; color: #888; text-transform: uppercase; margin-bottom: 0px; margin-top: 20px; text-align: left; }";
  html += "</style>";
  html += "<script>";
  html += "function updateVal(id, val) { document.getElementById(id).innerText = val; }";
  html += "function fetchLog() {";
  html += "  fetch('/log').then(response => response.text()).then(data => {";
  html += "    let term = document.getElementById('terminalBox');";
  html += "    let isScrolledToBottom = term.scrollHeight - term.clientHeight <= term.scrollTop + 5;";
  html += "    term.innerText = data;";
  html += "    if (isScrolledToBottom) term.scrollTop = term.scrollHeight;";
  html += "  });";
  html += "}";
  html += "setInterval(fetchLog, 1000);";
  html += "</script></head><body>";
  
  html += "<div class='card'><h1><span style='color:#007bff;'>CTCSS</span> Asetukset</h1>";
  
  // ASETUSLOMAKE
  html += "<form action='/set' method='POST'>";
  html += "<label for='freq'>CTCSS-taajuus:</label><select name='freq' id='freq'>";
  for (int i = 0; i < LUT_SIZE; i++) {
    html += "<option value='" + String(i) + "'" + (i == currentFrequencyIndex ? " selected" : "") + ">" + String(ctcssFrequencies[i]) + " Hz</option>";
  }
  html += "</select>";
  html += "<label>Amplitudi: <span id='ampVal'>" + String(amplitude) + "</span></label>";
  html += "<input type='range' name='amp' min='0' max='127' value='" + String(amplitude) + "' oninput='updateVal(\"ampVal\", this.value)'>";
  html += "<label>DC Offset: <span id='offVal'>" + String(offset) + "</span></label>";
  html += "<input type='range' name='offset' min='0' max='255' value='" + String(offset) + "' oninput='updateVal(\"offVal\", this.value)'>";
  html += "<input type='submit' value='Tallenna & Päivitä'></form>";

  // TILATIETO JA LOKI
  html += "<div class='status'>Aktiivinen taajuus: <b>" + String(frequency_dac) + " Hz</b></div>";
  html += "<div class='term-title'>Järjestelmäloki</div>";
  html += "<div class='terminal' id='terminalBox'>Ladataan...</div>";

  // SAMMUTUSNAPPI NYT ALIMMAISENA
  html += "<a href='/shutdown' class='btn-off' onclick=\"return confirm('Sammutetaanko WiFi?')\">SAMMUTA WIFI HETI</a>";
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("freq")) {
    currentFrequencyIndex = server.arg("freq").toInt();
    frequency_dac = ctcssFrequencies[currentFrequencyIndex];
  }
  if (server.hasArg("amp")) amplitude = server.arg("amp").toInt();
  if (server.hasArg("offset")) offset = server.arg("offset").toInt();

  webPrintln("Asetukset päivitetty.");
  webPrintln("Taajuus: " + String(frequency_dac) + " Hz");
  generateSineLUT();

  EEPROM.write(0, currentFrequencyIndex);
  EEPROM.write(1, amplitude);
  EEPROM.write(2, offset);
  EEPROM.commit();

  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);
  pinMode(inputPin, INPUT);
  startMillis = millis();

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);

  webPrintln("Tukiasema käynnissä: " + String(ap_ssid));
  webPrintln("IP: " + WiFi.softAPIP().toString());

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/log", handleLog);
  server.on("/shutdown", handleShutdown);
  server.begin();
  serverStarted = true;

  EEPROM.begin(16);
  currentFrequencyIndex = EEPROM.read(0);
  if (currentFrequencyIndex >= LUT_SIZE) currentFrequencyIndex = 0;
  frequency_dac = ctcssFrequencies[currentFrequencyIndex];

  int savedAmp = EEPROM.read(1);
  if (savedAmp != 255) amplitude = savedAmp;
  int savedOffset = EEPROM.read(2);
  if (savedOffset != 255) offset = savedOffset;

  generateSineLUT();
  outputValue = mapToLUT(frequency_dac);
}

void loop() {
  // DAC-ajo
  for (int i = 0; i < resolution; i++) {
    dacWrite(dacPin, sineLUT[i]);
    delayMicroseconds(outputValue);
  }

  // PTT-tarkistus (Pinni 9)
  int pinState = digitalRead(inputPin);
  if (pinState == HIGH) {
    if (serverStarted) {
      webPrintln("TX PÄÄLLÄ (PTT) - WiFi sammutetaan välittömästi");
      stopWiFi();
    }
    TX_ON = true;
  } else {
    TX_ON = false;
  }

  // Sarjaportti-ohjaus
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      number = input.toFloat();
      webPrintln("Sarjaportti syöte: " + String(number));
    }
  }

  // Web-palvelimen käsittely
  if (serverStarted) {
    server.handleClient();
    
    // 5 minuutin aikakatkaisu
    currentMillis = millis();
    if (currentMillis - startMillis >= duration) {
      webPrintln("5min aikaraja täynnä. WiFi OFF.");
      stopWiFi();
    }
  }

  // Päivitetään viive jos taajuus on muuttunut
  if (frequency_dac != frequency_dac_saved) {
    frequency_dac_saved = frequency_dac;
    outputValue = mapToLUT(frequency_dac);
    webPrintln("Uusi DAC viive: " + String(outputValue));
  }
}