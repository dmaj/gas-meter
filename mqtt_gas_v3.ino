#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

// --- KONFIGURATION HARDWARE ---
const int REED_PIN        = 4;
const int BOOT_BUTTON_PIN = 9; 
const int STATUS_LED_PIN  = 10;
const int NUM_PIXELS      = 1;
const int DEBOUNCE_MS     = 100;

Adafruit_NeoPixel pixels(NUM_PIXELS, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// --- GLOBALE VARIABLEN ---
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
Preferences preferences;

String mqtt_server, mqtt_topic;
int mqtt_port;
bool isConfigMode = false;

volatile unsigned long totalTicks = 0;
volatile unsigned long lastTickTime = 0;
volatile double aktuelleLeistung = 0.0;
volatile unsigned long lastDebounceTime = 0;
volatile bool bootUp = true;
volatile double gasCounter = 0.0;

volatile unsigned long lastWifiRetry = 0, lastMqttRetry = 0;
volatile unsigned long lastSave = 0, lastHourlyLog = 0;
volatile bool dataChanged = false;
float hourlyHistory[24] = {0};

// --- HILFSFUNKTIONEN ---
void setLED(uint32_t color) {
  pixels.setPixelColor(0, color);
  pixels.show();
}

void loadConfigs() {
  preferences.begin("mqtt_config", true);
  mqtt_server = preferences.getString("server", "192.168.178.20");
  mqtt_port   = preferences.getInt("port", 1883);
  mqtt_topic  = preferences.getString("topic", "gas");
  preferences.end();

  preferences.begin("gas_raw", true);
  totalTicks = preferences.getULong("ticks", 0);
  gasCounter = preferences.getDouble("counter", 0.0);
  preferences.end();
}

// --- ISR (Interrupt) ---
void IRAM_ATTR handleInterrupt() {
  unsigned long currentTime = millis();
  if ((currentTime - lastDebounceTime) > DEBOUNCE_MS) {
    unsigned long delta = currentTime - lastTickTime;
    if (delta > 500) aktuelleLeistung = 36.0 / (delta / 1000.0);
    lastTickTime = currentTime;
    lastDebounceTime = currentTime;
    totalTicks++;
    dataChanged = true;
    //if (!isConfigMode) setLED(pixels.Color(0, 255, 0)); // Grün bei Tick
  }
}

// --- WEB SERVER HANDLER ---
void handleUpdate() {
  if (millis() - lastTickTime > 300000) aktuelleLeistung = 0.0;
  double aktuellerStand = gasCounter + (totalTicks * 0.01);
  
  // JSON String manuell zusammenbauen
  String json = "{";
  json += "\"total\":" + String(aktuellerStand, 3) + ",";
  json += "\"ticks\":" + String(totalTicks) + ","; // <--- Hier sind sie wieder!
  json += "\"power\":" + String(aktuelleLeistung, 2) + ",";
  json += "\"mqtt\":" + String(client.connected() ? "true" : "false") + ",";
  json += "\"history\": [";
  for (int i = 0; i < 24; i++) {
    json += String(hourlyHistory[i], 2) + (i < 23 ? "," : "");
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleRoot() {
  if (isConfigMode) {
    String h = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body style='font-family:sans-serif;padding:20px;'><h1>WiFi Setup</h1><form action='/config' method='POST'>";
    h += "SSID:<br><input name='ssid' style='width:100%;padding:10px;'><br>Pass:<br><input name='pass' type='password' style='width:100%;padding:10px;'><br><br>";
    h += "<input type='submit' value='Verbinden' style='width:100%;background:#27ae60;color:white;padding:10px;border:none;'></form></body></html>";
    server.send(200, "text/html", h);
    return;
  }
  
  String html = "<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Gas Meter</title><script src='https://cdn.jsdelivr.net/npm/chart.js'></script><style>";
  html += "body{font-family:sans-serif;background:#f0f2f5;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center;} .card{background:white;padding:20px;border-radius:15px;box-shadow:0 4px 10px rgba(0,0,0,0.1);width:100%;max-width:400px;position:relative;margin-bottom:20px;text-align:center;}";
  html += ".dot{height:8px;width:8px;border-radius:50%;display:inline-block;margin-right:5px;} .green{background:#2ecc71;} .red{background:#e74c3c;animation:p 1s infinite;} @keyframes p{50%{opacity:0.3;}}";
  html += ".menu-btn{position:absolute;top:10px;right:15px;font-size:24px;cursor:pointer;} .menu-content{display:none;position:absolute;top:40px;right:15px;background:white;box-shadow:0 4px 10px rgba(0,0,0,0.2);border-radius:8px;z-index:100;} .menu-content label{display:block;padding:12px 20px;cursor:pointer;border-bottom:1px solid #eee;font-size:14px;}";
  html += ".counter{font-size:3rem;font-weight:bold;font-family:monospace;color:#2c3e50;} .power{font-size:1.5rem;color:#2ecc71;} .modal{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:1000;justify-content:center;align-items:center;} .modal-box{background:white;padding:20px;border-radius:10px;width:85%;max-width:320px;}</style></head><body>";
  
  // Hauptkarte
  html += "<div class='card'><div style='position:absolute;top:15px;left:15px;font-size:0.7rem;'><span id='s-dot' class='dot red'></span><span id='s-txt'>Warte...</span></div>";
  html += "<div class='menu-btn' onclick='tM()'>&#9776;</div>";
  html += "<div id='menu' class='menu-content'><label onclick='sMo(\"mo\")'>Basiswert setzen</label><label onclick='sMo(\"mq\")'>MQTT Setup</label><label onclick='rT()'>Ticks Reset</label></div>";
  html += "<h1>Zähler m³</h1><div class='counter'><span id='m-v'>00000</span><span id='d-v' style='color:#e74c3c'>.000</span></div>";
  html += "<div class='power'><span id='p-v'>0.00</span> <small>m³/h</small></div>";
  html += "<div style='font-size:0.7rem; color:#ccc; margin-top:10px;'>Impulse (Ticks): <span id='t-v'>0</span></div></div>";
  
  // Chart Karte
  html += "<div class='card'><canvas id='gChart'></canvas></div>";

  // Modal: Basiswert
  html += "<div id='mo' class='modal'><div class='modal-box'><h3>Basiswert</h3><form action='/setCounter' method='POST'><input type='number' name='val' step='0.001' style='width:100%;font-size:1.2rem;margin-bottom:15px;' required><button type='submit' style='width:100%;background:#27ae60;color:white;padding:10px;border:none;'>Speichern</button></form><button onclick='hMo()' style='width:100%;background:none;border:none;margin-top:10px;'>Abbruch</button></div></div>";
  
  // Modal: MQTT
  html += "<div id='mq' class='modal'><div class='modal-box'><h3>MQTT Setup</h3><form action='/setMqtt' method='POST'>IP:<input name='server' value='"+mqtt_server+"' style='width:100%;'><br>Port:<input name='port' type='number' value='"+String(mqtt_port)+"' style='width:100%;'><br>Topic:<input name='topic' value='"+mqtt_topic+"' style='width:100%;'><br><br><button type='submit' style='width:100%;background:#3498db;color:white;padding:10px;border:none;'>Speichern</button></form><button onclick='hMo()' style='width:100%;background:none;border:none;margin-top:10px;'>Abbruch</button></div></div>";

  // JavaScript
  html += "<script>";
  html += "let chart; function tM(){let m=document.getElementById('menu');m.style.display=m.style.display==='block'?'none':'block';}";
  html += "function sMo(id){document.getElementById(id).style.display='flex';tM();}";
  html += "function hMo(){document.querySelectorAll('.modal').forEach(m=>m.style.display='none');}";
  html += "function rT(){if(confirm('Wirklich alle Ticks zurücksetzen?')){fetch('/reset',{method:'POST'}).then(()=>location.reload());}}";
  
  html += "function up(){fetch('/update').then(r=>r.json()).then(d=>{";
  html += "let s=d.total.toFixed(3).padStart(9,'0');";
  html += "document.getElementById('m-v').innerText=s.substring(0,5);";
  html += "document.getElementById('d-v').innerText=s.substring(5);";
  html += "document.getElementById('p-v').innerText=d.power.toFixed(2);";
  html += "document.getElementById('t-v').innerText=d.ticks;"; // Ticks Anzeige
  html += "document.getElementById('s-dot').className=d.mqtt?'dot green':'dot red';";
  html += "document.getElementById('s-txt').innerText=d.mqtt?'MQTT OK':'MQTT ERR';";
  html += "if(!chart){initC(d.history)}else{chart.data.datasets[0].data=d.history;chart.update();}";
  html += "}).catch(e=>console.log('Update Error:',e));}";
  
  html += "function initC(h){const ctx=document.getElementById('gChart').getContext('2d');chart=new Chart(ctx,{type:'bar',data:{labels:Array.from({length:24},(_,i)=>i+'h'),datasets:[{label:'m³',data:h,backgroundColor:'#3498db88'}]},options:{scales:{y:{beginAtZero:false}}}});}";
  html += "setInterval(up,2000); up();";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(40);
  setLED(pixels.Color(0, 0, 0));

  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(REED_PIN, INPUT_PULLUP);

  loadConfigs();

  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid == "") {
    isConfigMode = true;
    WiFi.softAP("Gas-Zaehler-Setup", "12345678");
    setLED(pixels.Color(0, 0, 255)); // Blau: AP Modus
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
  }

  attachInterrupt(digitalPinToInterrupt(REED_PIN), handleInterrupt, FALLING);

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.on("/config", HTTP_POST, [](){
    preferences.begin("wifi", false);
    preferences.putString("ssid", server.arg("ssid"));
    preferences.putString("pass", server.arg("pass"));
    preferences.end();
    server.send(200, "text/plain", "Gespeichert. Neustart...");
    delay(1000); ESP.restart();
  });
  server.on("/setMqtt", HTTP_POST, [](){
    preferences.begin("mqtt_config", false);
    preferences.putString("server", server.arg("server"));
    preferences.putInt("port", server.arg("port").toInt());
    preferences.putString("topic", server.arg("topic"));
    preferences.end();
    loadConfigs(); client.disconnect();
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/setCounter", HTTP_POST, [](){
    gasCounter = server.arg("val").toDouble(); totalTicks = 0;
    preferences.begin("gas_raw", false);
    preferences.putDouble("counter", gasCounter);
    preferences.putULong("ticks", 0);
    preferences.end();
    server.sendHeader("Location", "/"); server.send(303);
  });
  server.on("/reset", HTTP_POST, [](){
    totalTicks = 0; preferences.begin("gas_raw", false); preferences.putULong("ticks", 0); preferences.end();
    server.send(200, "OK");
  });
  server.begin();
}

// --- LOOP ---
void loop() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    unsigned long start = millis();
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (millis() - start > 5000) setLED(pixels.Color(255, 0, 255)); // Violett: Bereit zum Reset
    }
    if (millis() - start > 5000) {
      preferences.begin("wifi", false); preferences.clear(); preferences.end();
      setLED(pixels.Color(0, 0, 0)); ESP.restart();
    }
  }

  if (!isConfigMode) {
    // LED Management (Blitz-Dauer)
    if (WiFi.status() == WL_CONNECTED)
      if (millis() - lastTickTime > 100)
        setLED(pixels.Color(0, 0, 0)); // OK: Aus
      else setLED(pixels.Color(255, 0, 0)); // Grün bei Tick
    else setLED(pixels.Color(0, 255, 0)); // Rot: Offline

    // Original
    // if (millis() - lastTickTime > 100) {
    //   if (WiFi.status() != WL_CONNECTED) setLED(pixels.Color(255, 0, 0)); // Rot: Offline
    //   else setLED(pixels.Color(0, 0, 0)); // OK: Aus
    // }

    // WiFi & MQTT Reconnect
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected() && (millis() - lastMqttRetry > 5000)) {
        client.setServer(mqtt_server.c_str(), mqtt_port);
        if (client.connect("GasZaehlerC3")) {
           client.publish((mqtt_topic + "/status").c_str(), "online", true);
        }
        lastMqttRetry = millis();
      }
      client.loop();
    } else if (millis() - lastWifiRetry > 30000) {
      WiFi.reconnect(); lastWifiRetry = millis();
    }

    // MQTT Senden & Save (alle 60s)
    if ((dataChanged && (millis() - lastSave > 60000)) || bootUp) {
      if (client.connected()) {
        bootUp = false;
        client.publish((mqtt_topic + "/ticks").c_str(), String(totalTicks).c_str(), true);
        client.publish((mqtt_topic + "/counter").c_str(), String(gasCounter + (totalTicks * 0.01), 3).c_str(), true);
        preferences.begin("gas_raw", false);
        preferences.putULong("ticks", totalTicks);
        preferences.end();
        dataChanged = false; lastSave = millis();
      }
    }
    // Hourly Log
    if (millis() - lastHourlyLog > 3600000 || lastHourlyLog == 0) {
      lastHourlyLog = millis();
      for (int i=0; i<23; i++) hourlyHistory[i] = hourlyHistory[i+1];
      hourlyHistory[23] = totalTicks * 0.01;
    }
  }
  server.handleClient();
}