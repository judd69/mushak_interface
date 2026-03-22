#include <MAVLink.h>
#include <ESP32Servo.h>
#include <Wire.h> 
#include <SensirionI2cScd4x.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Team_Mushak_Rover";
const char* password = "lunarlane2026"; 
WebServer server(80);

const int PWM_Z_AXIS = 18; 
const int DIR_Z_AXIS = 19; 
const int PWM_SCREW = 25;  
const int DIR_SCREW = 26;  
const int SERVO_PIN = 27;  

const int PH_PIN = 34;         
const int SOIL_SENSE_PIN = 35; 

Servo waterServo;
SensirionI2cScd4x scd4x;

enum SoilState { SOIL_IDLE, LOWERING, SCREWING, RAISING };
SoilState currentSoilState = SOIL_IDLE;
unsigned long soilTimer = 0;
bool soilActive = false;

enum WaterState { WATER_IDLE, SWEEP_OUT, SWEEP_IN };
WaterState currentWaterState = WATER_IDLE;
unsigned long waterTimer = 0;
bool waterActive = false;

unsigned long co2Timer = 0;
bool co2Active = false;

bool prevSoilSwitch = false;
bool prevWaterSwitch = false;
bool prevCo2Switch = false;

int lastSoilMoisture = 0;
int lastPhRaw = 0;
uint16_t lastCo2 = 0;
unsigned long lastTelemetryPrint = 0;


const char* htmlDashboard = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Mushak Mission Control - Space Protocol</title>
  <style>
    :root { 
      --space-primary: #ffffff; 
      --space-secondary: #b0b0b0;
      --space-accent: #00ffff;
      --space-dark: #0a0a0a;
      --space-light: #f5f5f5;
    }

    body { 
      background-color: var(--space-dark); 
      background-image: 
        radial-gradient(circle at 20% 50%, rgba(255, 255, 255, 0.08) 0%, transparent 50%),
        radial-gradient(circle at 80% 80%, rgba(0, 255, 255, 0.05) 0%, transparent 40%),
        radial-gradient(2px 2px at 20px 30px, rgba(255, 255, 255, 0.8) 0%, rgba(255, 255, 255, 0.15) 2px, transparent 40px),
        radial-gradient(2px 2px at 60px 70px, rgba(255, 255, 255, 0.6) 0%, rgba(255, 255, 255, 0.1) 2px, transparent 40px),
        radial-gradient(1px 1px at 50px 50px, rgba(255, 255, 255, 0.9) 0%, rgba(255, 255, 255, 0.2) 1px, transparent 30px);
      background-size: 200px 200px, 150px 150px, 200px 200px, 150px 150px, 100px 100px;
      background-attachment: fixed;
      animation: starfield 60s linear infinite;
      color: var(--space-light); 
      font-family: 'Courier New', 'Segoe UI', monospace; 
      text-align: center; 
      margin: 0; 
      padding-top: 30px; 
      box-sizing: border-box; 
      overflow-x: hidden;
    }

    body::before {
      content: '';
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background-image: 
        radial-gradient(1px 1px at 10% 20%, rgba(255, 255, 255, 0.9) 0%, transparent 1px),
        radial-gradient(1px 1px at 20% 30%, rgba(0, 255, 255, 0.7) 0%, transparent 1px),
        radial-gradient(2px 2px at 50% 50%, rgba(255, 255, 255, 0.8) 0%, transparent 2px),
        radial-gradient(1px 1px at 80% 10%, rgba(255, 255, 255, 0.6) 0%, transparent 1px),
        radial-gradient(1px 1px at 90% 60%, rgba(0, 255, 255, 0.5) 0%, transparent 1px),
        radial-gradient(2px 2px at 30% 70%, rgba(255, 255, 255, 0.7) 0%, transparent 2px),
        radial-gradient(1px 1px at 60% 20%, rgba(255, 255, 255, 0.9) 0%, transparent 1px),
        radial-gradient(1px 1px at 15% 80%, rgba(0, 255, 255, 0.6) 0%, transparent 1px);
      background-size: 300px 300px;
      background-repeat: repeat;
      pointer-events: none;
      animation: twinkleStar 8s ease-in-out infinite, driftStar 120s linear infinite;
      z-index: 0;
    }

    body > * {
      position: relative;
      z-index: 1;
    }

    h1 { 
      color: var(--space-primary); 
      letter-spacing: 5px; 
      text-transform: uppercase; 
      text-shadow: 0 0 10px var(--space-accent), 0 0 20px rgba(0, 255, 255, 0.5), 2px 2px 4px rgba(0,0,0,0.9);
      animation: spacePulse 3s ease-in-out infinite;
      font-size: 2.2em;
      margin: 0 0 10px 0;
      font-weight: 900;
    }

    .subtitle {
      color: var(--space-secondary);
      font-size: 0.9em;
      letter-spacing: 2px;
      margin-bottom: 30px;
      text-transform: uppercase;
      opacity: 0.7;
      text-shadow: 0 0 5px rgba(0, 255, 255, 0.3);
    }

    .container {
      max-width: 1000px;
      margin: 0 auto;
      padding: 0 20px;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 30px;
      margin: 30px 0;
    }

    .card { 
      background: linear-gradient(135deg, rgba(15, 15, 15, 0.95) 0%, rgba(25, 25, 25, 0.95) 100%);
      border: 2px solid var(--space-primary); 
      border-radius: 2px; 
      padding: 30px; 
      position: relative; 
      box-shadow: 
        0 0 20px rgba(255, 255, 255, 0.15),
        inset 0 0 15px rgba(0, 255, 255, 0.05),
        0 0 40px rgba(0, 255, 255, 0.1);
      overflow: hidden;
      transition: all 0.3s ease;
    }

    .card:hover {
      box-shadow: 
        0 0 30px rgba(255, 255, 255, 0.25),
        inset 0 0 20px rgba(0, 255, 255, 0.1),
        0 0 50px rgba(0, 255, 255, 0.2);
      transform: translateY(-2px);
    }

    .card::before { 
      content: ''; 
      position: absolute; 
      top: 0; 
      left: 0; 
      right: 0; 
      height: 2px;
      background: linear-gradient(90deg, transparent, var(--space-accent), transparent);
      animation: scanline 2s ease-in-out infinite;
    }

    .card::after { 
      content: ''; 
      position: absolute; 
      top: 0; 
      right: 0; 
      width: 40px; 
      height: 40px; 
      border-top: 3px solid var(--space-primary); 
      border-right: 3px solid var(--space-primary);
      opacity: 0.5;
    }

    .card-content {
      position: relative;
      z-index: 1;
    }

    h2 { 
      font-size: 1em; 
      color: var(--space-primary); 
      margin: 0 0 15px 0; 
      text-transform: uppercase; 
      letter-spacing: 2px;
      font-weight: 600;
      border-bottom: 1px solid rgba(255, 255, 255, 0.2);
      padding-bottom: 12px;
      text-shadow: 0 0 5px rgba(0, 255, 255, 0.2);
    }
    
    .data-val { 
      font-size: 2em; 
      font-weight: bold; 
      color: var(--space-accent); 
      text-shadow: 0 0 10px var(--space-accent), 0 0 20px rgba(0, 255, 255, 0.4);
      margin: 15px 0;
      font-family: 'Courier New', monospace;
      letter-spacing: 1px;
    }
    
    .status { 
      color: var(--space-secondary); 
      font-size: 0.95em; 
      font-weight: bold; 
      margin-top: 15px; 
      padding-top: 15px;
      border-top: 1px solid rgba(255, 255, 255, 0.15);
      min-height: 1.5em; 
      text-transform: uppercase;
      letter-spacing: 1px;
      animation: statusGlow 2s ease-in-out infinite;
    }

    .status.active {
      color: var(--space-accent);
      animation: statusGlowActive 1s ease-in-out infinite;
      text-shadow: 0 0 8px var(--space-accent);
    }

    .footer { 
      color: var(--space-secondary); 
      font-size: 0.85em; 
      margin: 60px auto 30px; 
      font-style: italic; 
      letter-spacing: 1px;
      opacity: 0.6;
      text-transform: uppercase;
      text-shadow: 0 0 5px rgba(0, 255, 255, 0.15);
    }

    .key-debug {
      max-width: 960px;
      margin: 0 auto 24px;
      padding: 12px 16px;
      border: 1px solid rgba(255, 255, 255, 0.25);
      background: rgba(0, 0, 0, 0.35);
      color: var(--space-secondary);
      font-size: 0.85em;
      letter-spacing: 0.5px;
      text-align: left;
    }

    .key-debug.ok { color: #7dffb0; border-color: rgba(125, 255, 176, 0.6); }
    .key-debug.err { color: #ff8e8e; border-color: rgba(255, 142, 142, 0.6); }

    .pulse-dot {
      display: inline-block;
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--space-accent);
      margin-right: 8px;
      animation: dotPulse 1.5s ease-in-out infinite;
      box-shadow: 0 0 5px var(--space-accent);
    }

    @keyframes starfield {
      0% { background-position: 0 0, 0 0, 0 0; }
      100% { background-position: 200px 0, 150px 0, 100px 0; }
    }

    @keyframes twinkleStar {
      0%, 100% { opacity: 0.3; }
      50% { opacity: 1; }
    }

    @keyframes driftStar {
      0% { transform: translateX(0) translateY(0); }
      50% { transform: translateX(10px) translateY(-5px); }
      100% { transform: translateX(0) translateY(0); }
    }
    
    @keyframes spacePulse {
      0%, 100% { 
        opacity: 0.85;
        text-shadow: 0 0 10px var(--space-accent), 0 0 20px rgba(0, 255, 255, 0.5), 2px 2px 4px rgba(0,0,0,0.9);
      }
      50% { 
        opacity: 1;
        text-shadow: 0 0 20px var(--space-accent), 0 0 40px rgba(0, 255, 255, 0.7), 2px 2px 4px rgba(0,0,0,0.9);
      }
    }

    @keyframes scanline {
      0% { opacity: 0; }
      50% { opacity: 1; }
      100% { opacity: 0; }
    }

    @keyframes statusGlow {
      0%, 100% { 
        opacity: 0.8;
        color: var(--space-secondary);
      }
      50% { 
        opacity: 1;
        color: var(--space-primary);
        text-shadow: 0 0 5px rgba(255, 255, 255, 0.3);
      }
    }

    @keyframes statusGlowActive {
      0%, 100% { 
        opacity: 0.9;
        color: var(--space-accent);
        text-shadow: 0 0 8px var(--space-accent);
      }
      50% { 
        opacity: 1;
        color: var(--space-accent);
        text-shadow: 0 0 15px var(--space-accent);
      }
    }

    @keyframes dotPulse {
      0%, 100% { 
        opacity: 0.4;
        box-shadow: 0 0 5px var(--space-accent);
      }
      50% { 
        opacity: 1;
        box-shadow: 0 0 15px var(--space-accent);
      }
    }

    @media (max-width: 600px) {
      h1 { font-size: 1.6em; }
      .card { padding: 20px; }
      .data-val { font-size: 1.5em; }
      h2 { font-size: 0.9em; }
    }
  </style>
  <script>
    function updateKeyDebug(message, statusClass) {
      const el = document.getElementById('keyDebug');
      if (!el) return;
      const now = new Date();
      const hh = String(now.getHours()).padStart(2, '0');
      const mm = String(now.getMinutes()).padStart(2, '0');
      const ss = String(now.getSeconds()).padStart(2, '0');
      el.textContent = '[' + hh + ':' + mm + ':' + ss + '] ' + message;
      el.classList.remove('ok', 'err');
      if (statusClass) el.classList.add(statusClass);
    }

    async function triggerTask(taskName) {
      const url = '/trigger/tasktool?task=' + encodeURIComponent(taskName);

      async function tryMethod(method) {
        try {
          const response = await fetch(url, { method: method });
          return { ok: response.ok, status: response.status, method: method };
        } catch (err) {
          return { ok: false, error: err.message || 'request-failed', method: method };
        }
      }

      const postResult = await tryMethod('POST');
      if (postResult.ok || postResult.status === 409) return postResult;

      const getResult = await tryMethod('GET');
      if (getResult.ok || getResult.status === 409) return getResult;

      if (getResult.status || getResult.error) return getResult;
      return postResult;
    }

    const taskByKey = {
      Shift: 'soil',
      Control: 'water',
      Ctrl: 'water',
      Alt: 'co2'
    };

    const taskByCode = {
      ShiftLeft: 'soil',
      ShiftRight: 'soil',
      ControlLeft: 'water',
      ControlRight: 'water',
      AltLeft: 'co2',
      AltRight: 'co2'
    };

    window.addEventListener('keydown', async function(event) {
      const targetTag = (event.target && event.target.tagName) ? event.target.tagName.toLowerCase() : '';
      if (targetTag === 'input' || targetTag === 'textarea' || targetTag === 'select') return;
      if (event.repeat) return;

      const task = taskByKey[event.key] || taskByCode[event.code];
      if (!task) {
        updateKeyDebug('Key seen but not mapped: key=' + event.key + ' code=' + event.code, 'err');
        return;
      }

      event.preventDefault();
      updateKeyDebug('Detected key=' + event.key + ' code=' + event.code + ' -> task=' + task + ' (sending...)');
      const result = await triggerTask(task);
      if (result.ok) {
        updateKeyDebug('OK key=' + event.key + ' code=' + event.code + ' -> task=' + task + ' via ' + result.method + ' HTTP ' + result.status, 'ok');
      } else if (result.status === 409) {
        updateKeyDebug('BUSY key=' + event.key + ' code=' + event.code + ' -> task=' + task + ' via ' + result.method + ' HTTP 409');
      } else if (result.status) {
        updateKeyDebug('FAILED key=' + event.key + ' code=' + event.code + ' -> task=' + task + ' via ' + result.method + ' HTTP ' + result.status, 'err');
      } else {
        updateKeyDebug('FAILED key=' + event.key + ' code=' + event.code + ' -> task=' + task + ' via ' + result.method + ' error=' + result.error, 'err');
      }
    }, true);

    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        document.getElementById('soil').innerText = data.soil;
        document.getElementById('ph').innerText = data.ph;
        document.getElementById('co2').innerText = data.co2;
        
        let soilStatusEl = document.getElementById('soilStatus');
        soilStatusEl.classList.toggle('active', data.sState !== 'IDLE');
        soilStatusEl.innerHTML = (data.sState !== 'IDLE' ? '<span class="pulse-dot"></span>' : '') + data.sState + (data.sState !== 'IDLE' ? ' [' + data.sTime + 's]' : '');

        let waterStatusEl = document.getElementById('waterStatus');
        waterStatusEl.classList.toggle('active', data.wState !== 'IDLE');
        waterStatusEl.innerHTML = (data.wState !== 'IDLE' ? '<span class="pulse-dot"></span>' : '') + data.wState + (data.wState !== 'IDLE' ? ' [' + data.wTime + 's]' : '');

        let co2StatusEl = document.getElementById('co2Status');
        co2StatusEl.classList.toggle('active', data.cState !== 'IDLE');
        co2StatusEl.innerHTML = (data.cState !== 'IDLE' ? '<span class="pulse-dot"></span>' : '') + data.cState + (data.cState !== 'IDLE' ? ' [' + data.cTime + 's]' : '');
      }).catch(err => console.log("Waiting for telemetry..."));
    }, 1000);
  </script>
</head>
<body>
  <h1>TEAM MUSHAK</h1>
  <div class="subtitle">Space Protocol - Telemetry Active</div>
  <div id="keyDebug" class="key-debug">Keyboard debug: click anywhere on this page, then press Shift/Ctrl/Alt.</div>
  
  <div class="container">
    <div class="grid">
      <div class="card">
        <div class="card-content">
          <h2>SOIL MOISTURE</h2>
          <div class="data-val" id="soil">WAITING</div>
          <div class="status" id="soilStatus">IDLE</div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-content">
          <h2>WATER pH</h2>
          <div class="data-val" id="ph">WAITING</div>
          <div class="status" id="waterStatus">IDLE</div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-content">
          <h2>ATMOSPHERE CO2</h2>
          <div class="data-val" id="co2">WAITING</div>
          <div class="status" id="co2Status">IDLE</div>
        </div>
      </div>
    </div>
  </div>
  
  <div class="footer">SPACE PROTOCOL ACTIVE :: SECURE TELEMETRY LINK</div>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlDashboard);
}

void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  
  String sState = "IDLE"; int sTime = 0;
  if(soilActive) {
    unsigned long t = millis() - soilTimer;
    if(currentSoilState == LOWERING) { sState = "LOWERING Z-AXIS"; sTime = (10000 - t)/1000; }
    else if(currentSoilState == SCREWING) { sState = "DRILLING SOIL"; sTime = (30000 - t)/1000; }
    else if(currentSoilState == RAISING) { sState = "RAISING Z-AXIS"; sTime = (10000 - t)/1000; }
    if(sTime < 0) sTime = 0;
  }
  
  String wState = "IDLE"; int wTime = 0;
  if(waterActive) {
    unsigned long t = millis() - waterTimer;
    if(currentWaterState == SWEEP_OUT) { wState = "TESTING WATER pH"; wTime = (8000 - t)/1000; }
    else if(currentWaterState == SWEEP_IN) { wState = "RETRACTING pH"; wTime = (2000 - t)/1000; }
    if(wTime < 0) wTime = 0;
  }
  
  String cState = "IDLE"; int cTime = 0;
  if(co2Active) {
    unsigned long t = millis() - co2Timer;
    cState = "SCANNING ATMOSPHERE"; cTime = (30000 - t)/1000;
    if(cTime < 0) cTime = 0;
  }

  
  float airValue = 3500.0;
  float waterValue = 1500.0;
  float vwcPercent = ((airValue - lastSoilMoisture) / (airValue - waterValue)) * 100.0;
  if (vwcPercent < 0) vwcPercent = 0;
  if (vwcPercent > 100) vwcPercent = 100;
  float m3m3 = vwcPercent / 100.0;
  String soilFormatted = String(vwcPercent, 0) + "% VWC | " + String(m3m3, 2) + " m³/m³";

  float phVoltage = lastPhRaw * (3.3 / 4095.0);
  float finalPh = (-5.70 * phVoltage) + 21.25; 
  if (finalPh < 0.0) finalPh = 0.0;
  if (finalPh > 14.0) finalPh = 14.0;
  String phFormatted = String(finalPh, 1) + " pH";

  String co2Formatted = String(lastCo2) + " ppm";

  String json = "{";
  json += "\"soil\":\"" + soilFormatted + "\",";
  json += "\"ph\":\"" + phFormatted + "\",";
  json += "\"co2\":\"" + co2Formatted + "\",";
  json += "\"sState\":\"" + sState + "\",\"sTime\":" + String(sTime) + ",";
  json += "\"wState\":\"" + wState + "\",\"wTime\":" + String(wTime) + ",";
  json += "\"cState\":\"" + cState + "\",\"cTime\":" + String(cTime);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleTaskToolTrigger() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");

  String task = server.arg("task");
  task.toLowerCase();

  if (task == "soil") {
    if (!soilActive && currentSoilState == SOIL_IDLE) {
      startSoil();
      server.send(200, "application/json", "{\"ok\":true,\"trigger\":\"keyboard\",\"task\":\"soil\"}");
      return;
    }
    server.send(409, "application/json", "{\"ok\":false,\"reason\":\"soil-task-busy\"}");
    return;
  }

  if (task == "water") {
    if (!waterActive && currentWaterState == WATER_IDLE) {
      startWater();
      server.send(200, "application/json", "{\"ok\":true,\"trigger\":\"keyboard\",\"task\":\"water\"}");
      return;
    }
    server.send(409, "application/json", "{\"ok\":false,\"reason\":\"water-task-busy\"}");
    return;
  }

  if (task == "co2") {
    if (!co2Active) {
      startCO2();
      server.send(200, "application/json", "{\"ok\":true,\"trigger\":\"keyboard\",\"task\":\"co2\"}");
      return;
    }
    server.send(409, "application/json", "{\"ok\":false,\"reason\":\"co2-task-busy\"}");
    return;
  }

  server.send(400, "application/json", "{\"ok\":false,\"reason\":\"unknown-task\"}");
}


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17); 
  Wire.begin(21, 22); 

  pinMode(PWM_Z_AXIS, OUTPUT); pinMode(DIR_Z_AXIS, OUTPUT);
  pinMode(PWM_SCREW, OUTPUT);  pinMode(DIR_SCREW, OUTPUT);
  pinMode(PH_PIN, INPUT);      pinMode(SOIL_SENSE_PIN, INPUT);

  analogWrite(PWM_Z_AXIS, 0); analogWrite(PWM_SCREW, 0);
  waterServo.setPeriodHertz(50); waterServo.attach(SERVO_PIN, 500, 2400); waterServo.write(90); 

  scd4x.begin(Wire, 0x62); scd4x.startPeriodicMeasurement();

  Serial.println("\n--- Starting Mushak Comm Link ---");
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Wi-Fi Network Created: "); Serial.println(ssid);
  Serial.print("Mission Control IP Address: "); Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/trigger/tasktool", HTTP_GET, handleTaskToolTrigger);
  server.on("/trigger/tasktool", HTTP_POST, handleTaskToolTrigger);
  server.begin();
  Serial.println("Web Server Online.");
}


void loop() {
  server.handleClient(); 

  mavlink_message_t msg; mavlink_status_t status;
  while (Serial2.available() > 0) {
    uint8_t c = Serial2.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
      if (msg.msgid == MAVLINK_MSG_ID_SERVO_OUTPUT_RAW) {
        mavlink_servo_output_raw_t servo;
        mavlink_msg_servo_output_raw_decode(&msg, &servo);
        
        bool currentSoilSwitch = (servo.servo5_raw > 1400 && servo.servo5_raw < 1600); 
        bool currentWaterSwitch = (servo.servo5_raw > 1800); 
        bool currentCo2Switch = (servo.servo6_raw > 1800); 
        
        if (currentSoilSwitch && !prevSoilSwitch && !soilActive && currentSoilState == SOIL_IDLE) startSoil();
        if (currentWaterSwitch && !prevWaterSwitch && !waterActive && currentWaterState == WATER_IDLE) startWater();
        if (currentCo2Switch && !prevCo2Switch && !co2Active) startCO2();

        prevSoilSwitch = currentSoilSwitch;
        prevWaterSwitch = currentWaterSwitch;
        prevCo2Switch = currentCo2Switch;
      }
    }
  }

  runSoil(); runWater(); runCO2();

  if (millis() - lastTelemetryPrint >= 1000) {
    updateTelemetry();
    lastTelemetryPrint = millis();
  }
}


void startSoil() { soilActive = true; currentSoilState = LOWERING; soilTimer = millis(); }
void runSoil() {
  if (!soilActive) return;
  unsigned long timePassed = millis() - soilTimer;
  switch(currentSoilState) {
    case LOWERING:
      digitalWrite(DIR_Z_AXIS, HIGH); analogWrite(PWM_Z_AXIS, 200); 
      if (timePassed >= 10000) { analogWrite(PWM_Z_AXIS, 0); currentSoilState = SCREWING; soilTimer = millis(); }
      break;
    case SCREWING:
      digitalWrite(DIR_SCREW, HIGH); analogWrite(PWM_SCREW, 255); 
      if (timePassed >= 30000) { analogWrite(PWM_SCREW, 0); currentSoilState = RAISING; soilTimer = millis(); }
      break;
    case RAISING:
      digitalWrite(DIR_Z_AXIS, LOW); analogWrite(PWM_Z_AXIS, 200);  
      if (timePassed >= 10000) { analogWrite(PWM_Z_AXIS, 0); currentSoilState = SOIL_IDLE; soilActive = false; }
      break;
    case SOIL_IDLE: analogWrite(PWM_Z_AXIS, 0); analogWrite(PWM_SCREW, 0); break;
  }
}

void startWater() { waterActive = true; currentWaterState = SWEEP_OUT; waterTimer = millis(); }
void runWater() {
  if (!waterActive) return;
  unsigned long timePassed = millis() - waterTimer;
  switch(currentWaterState) {
    case SWEEP_OUT:
      waterServo.write(180); 
      if (timePassed >= 8000) { currentWaterState = SWEEP_IN; waterTimer = millis(); }
      break;
    case SWEEP_IN:
      waterServo.write(90); 
      if (timePassed >= 2000) { currentWaterState = WATER_IDLE; waterActive = false; }
      break;
    case WATER_IDLE: break;
  }
}

void startCO2() { co2Active = true; co2Timer = millis(); }
void runCO2() {
  if (!co2Active) return;
  if (millis() - co2Timer >= 30000) { co2Active = false; }
}

void updateTelemetry() {
  if (soilActive && currentSoilState == SCREWING) {
    lastSoilMoisture = analogRead(SOIL_SENSE_PIN);
  }
  if (waterActive && currentWaterState == SWEEP_OUT) {
    lastPhRaw = analogRead(PH_PIN);
  }
  if (co2Active) {
    float temp = 0.0f, humidity = 0.0f; 
    bool isDataReady = false;
    scd4x.getDataReadyStatus(isDataReady);
    if (isDataReady) {
      scd4x.readMeasurement(lastCo2, temp, humidity);
    }
  }
}
