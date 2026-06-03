#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <vector>

#define IR_LED_PIN 13
#define IR_RECV_PIN 26

// ================= WIFI =================
const char* ssid = "VANTARA";
const char* password = "VANTARA@123";
bool isAPMode = false;

WebServer server(80);

// ================= IR LEARNING & CONTROL =================
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50; // Timeout in milliseconds for silence

IRsend irsend(IR_LED_PIN);
IRrecv irrecv(IR_RECV_PIN, kCaptureBufferSize, kTimeout, true);
decode_results results;
bool isLearning = false;
unsigned long learningStartTime = 0;
String lastDecodedJSON = "{}";
String pendingButtonName = ""; // If set when starting learn, auto-save to this button
String lastSavedButtonName = ""; // Set when auto-save completes so UI can skip re-upload

void sendJsonResponse(int code, const String &payload) {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", payload);
}

// ================= HTML PAGE =================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Universal AC Hub</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --primary: #00f2fe;
            --secondary: #4facfe;
            --primary-glow: rgba(0, 242, 254, 0.3);
            --bg: #0b0f19;
            --card-bg: rgba(22, 30, 49, 0.6);
            --border: rgba(0, 242, 254, 0.15);
            --border-hover: rgba(0, 242, 254, 0.4);
            --text-main: #f8fafc;
            --text-dim: #94a3b8;
            --success: #10b981;
            --success-glow: rgba(16, 185, 129, 0.3);
            --danger: #ef4444;
            --danger-glow: rgba(239, 68, 68, 0.3);
        }
        
        body {
            font-family: 'Outfit', sans-serif;
            background-color: var(--bg);
            background-image: 
                radial-gradient(at 0% 0%, rgba(79, 172, 254, 0.12) 0px, transparent 50%),
                radial-gradient(at 100% 100%, rgba(0, 242, 254, 0.08) 0px, transparent 50%);
            background-attachment: fixed;
            color: var(--text-main);
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        
        .container {
            width: 95%;
            max-width: 600px;
            padding: 40px 20px;
            box-sizing: border-box;
        }
        
        h1.app-title {
            text-align: center;
            font-size: 2.5rem;
            font-weight: 700;
            margin: 0 0 5px 0;
            background: linear-gradient(135deg, #ffffff 0%, var(--primary) 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: -1px;
        }
        
        p.subtitle {
            text-align: center;
            color: var(--text-dim);
            margin-top: 0;
            margin-bottom: 40px;
            font-weight: 300;
            font-size: 1.1rem;
        }
        
        .glass-card {
            background: var(--card-bg);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid var(--border);
            border-radius: 20px;
            padding: 25px;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.5);
            margin-bottom: 25px;
            transition: border-color 0.3s, box-shadow 0.3s;
        }
        
        .glass-card:hover {
            border-color: var(--border-hover);
            box-shadow: 0 20px 45px rgba(0, 242, 254, 0.05);
        }
        
        button {
            font-family: 'Outfit', sans-serif;
            font-weight: 600;
            border: none;
            border-radius: 12px;
            cursor: pointer;
            transition: all 0.2s ease;
        }
        
        button:active {
            transform: scale(0.96);
        }
        
        button:disabled {
            opacity: 0.25;
            cursor: not-allowed;
            pointer-events: none;
        }
        
        .btn-train {
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.1);
            color: var(--text-main);
            padding: 16px;
            font-size: 1.1rem;
            width: 100%;
            margin-bottom: 30px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        
        .btn-train:hover {
            background: rgba(255, 255, 255, 0.08);
            border-color: rgba(255, 255, 255, 0.2);
            transform: translateY(-1px);
        }
        
        .btn-train.active {
            background: linear-gradient(135deg, #f59e0b 0%, #d97706 100%);
            color: #0b0f19;
            border-color: #f59e0b;
            box-shadow: 0 4px 15px rgba(245, 158, 11, 0.4);
        }
        
        .device-buttons {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(90px, 1fr));
            gap: 12px;
        }
        
        .ac-btn-wrapper {
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.05);
            border-radius: 14px;
            padding: 10px;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            align-items: center;
            height: 90px;
            transition: all 0.3s;
            box-sizing: border-box;
        }
        
        .ac-btn-wrapper.learned {
            background: rgba(0, 242, 254, 0.04);
            border-color: rgba(0, 242, 254, 0.3);
        }
        
        .ac-btn-wrapper.learned:hover {
            border-color: var(--primary);
            box-shadow: 0 0 15px rgba(0, 242, 254, 0.15);
        }
        
        .ac-btn {
            background: transparent;
            color: var(--text-dim);
            font-size: 1.1rem;
            width: 100%;
            padding: 6px 0;
            height: 40px;
        }
        
        .ac-btn-wrapper.learned .ac-btn {
            color: #ffffff;
            font-weight: 700;
            text-shadow: 0 0 10px var(--primary-glow);
        }
        
        .ac-btn-wrapper.learned .ac-btn.btn-on {
            color: #34d399;
            text-shadow: 0 0 10px var(--success-glow);
        }
        
        .ac-btn-wrapper.learned .ac-btn.btn-off {
            color: #f87171;
            text-shadow: 0 0 10px var(--danger-glow);
        }
        
        .btn-status-label {
            font-size: 0.65rem;
            color: var(--text-dim);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin-top: 4px;
        }
        
        .ac-btn-wrapper.learned .btn-status-label {
            color: var(--primary);
            font-weight: 600;
        }
        
        .modal-overlay {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(11, 15, 25, 0.8);
            backdrop-filter: blur(8px);
            z-index: 100;
            justify-content: center;
            align-items: center;
            animation: fadeIn 0.3s ease;
        }
        
        .modal {
            background: #111827;
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            padding: 30px;
            width: 90%;
            max-width: 400px;
            text-align: center;
            box-shadow: 0 25px 50px rgba(0,0,0,0.6);
            transform: scale(0.9);
            animation: scaleUp 0.3s forwards;
            box-sizing: border-box;
        }
        
        @keyframes scaleUp {
            to { transform: scale(1); }
        }
        
        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }
        
        .modal h3 {
            margin-top: 0;
            font-size: 1.3rem;
            color: #ffffff;
        }
        
        .modal p {
            color: var(--text-dim);
            font-size: 0.9rem;
            line-height: 1.5;
        }
        
        .modal-buttons {
            display: flex;
            justify-content: space-between;
            gap: 12px;
            margin-top: 20px;
        }
        
        .modal-buttons button {
            flex: 1;
            padding: 12px;
        }
        
        .btn-cancel {
            background: rgba(255, 255, 255, 0.08);
            color: #ffffff;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .btn-cancel:hover {
            background: rgba(255, 255, 255, 0.15);
        }
        
        .status-bar {
            position: fixed;
            bottom: 25px;
            right: 25px;
            padding: 12px 24px;
            border-radius: 12px;
            background: rgba(79, 172, 254, 0.95);
            backdrop-filter: blur(8px);
            color: #0b0f19;
            font-weight: 600;
            display: none;
            z-index: 1000;
            box-shadow: 0 10px 30px rgba(0, 242, 254, 0.3);
            animation: slideUp 0.3s ease;
        }
        
        @keyframes slideUp {
            from { transform: translateY(20px); opacity: 0; }
            to { transform: translateY(0); opacity: 1; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1 class="app-title">Universal AC Hub</h1>
        <p class="subtitle">Program and control your AC remote using infrared learning</p>
        
        <button id="btn-train-mode" class="btn-train" onclick="toggleTrainingMode()">Training Mode: OFF</button>
        
        <div class="glass-card">
            <div class="device-buttons" id="remote-grid">
                <!-- Buttons will load here -->
            </div>
        </div>
    </div>

    <!-- Learn Modal -->
    <div id="learn-modal" class="modal-overlay">
        <div class="modal">
            <h3 id="learn-title">Learning Mode</h3>
            <p id="learn-desc">Point your remote at the Hub and press the button...</p>
            <div id="modal-spinner" style="margin: 20px 0; display: flex; justify-content: center;">
                <svg width="40" height="40" viewBox="0 0 50 50">
                    <circle cx="25" cy="25" r="20" fill="none" stroke="var(--primary)" stroke-width="4" stroke-dasharray="31.4 31.4" stroke-linecap="round">
                        <animateTransform attributeName="transform" type="rotate" repeatCount="indefinite" dur="1s" keyTimes="0;1" values="0 25 25;360 25 25"></animateTransform>
                    </circle>
                </svg>
            </div>
            <div id="learn-success" style="display:none; color:var(--success); font-weight:bold; margin-bottom:15px;">Saved Successfully!</div>
            <button class="btn-cancel" style="width: 100%;" onclick="closeModals()">Cancel</button>
        </div>
    </div>

    <div id="status" class="status-bar"></div>

    <script>
        const STANDARD_BUTTONS = ["ON", "OFF", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"];
        let learnedButtons = {}; 
        let isTrainingMode = false;
        let pollInterval;
        let learningButton = "";

        const notify = (msg) => {
            const s = document.getElementById('status');
            s.innerText = msg; s.style.display = 'block';
            setTimeout(() => s.style.display = 'none', 3000);
        }

        const toggleTrainingMode = () => {
            isTrainingMode = !isTrainingMode;
            const btn = document.getElementById('btn-train-mode');
            if (isTrainingMode) {
                btn.classList.add('active');
                btn.innerText = "Training Mode: ON";
                notify("Training Mode Active. Click any button to train/record.");
            } else {
                btn.classList.remove('active');
                btn.innerText = "Training Mode: OFF";
                notify("Control Mode Active. Click learned buttons to send IR.");
            }
            renderButtons();
        }

        const loadButtons = () => {
            fetch('/api/buttons')
                .then(r => {
                    if (!r.ok) throw new Error("HTTP error " + r.status);
                    return r.json();
                })
                .then(data => {
                    learnedButtons = (data && typeof data === 'object') ? data : {};
                    renderButtons();
                })
                .catch(err => {
                    console.error("Error loading buttons:", err);
                    learnedButtons = {};
                    renderButtons();
                });
        }

        const renderButtons = () => {
            const grid = document.getElementById('remote-grid');
            if (!grid) return;
            grid.innerHTML = '';
            
            STANDARD_BUTTONS.forEach(btn => {
                const isLearned = learnedButtons.hasOwnProperty(btn);
                
                let wrapperClass = 'ac-btn-wrapper';
                if (isLearned) wrapperClass += ' learned';
                
                let btnColorClass = '';
                if (btn === 'ON') btnColorClass = 'btn-on';
                if (btn === 'OFF') btnColorClass = 'btn-off';
                
                const disabledAttr = (!isTrainingMode && !isLearned) ? 'disabled' : '';
                
                grid.innerHTML += `
                    <div class="${wrapperClass}">
                        <button class="ac-btn ${btnColorClass}" onclick="handleButtonClick('${btn}')" ${disabledAttr}>
                            ${btn}${!isNaN(btn) ? '&deg;' : ''}
                        </button>
                        <span class="btn-status-label">${isLearned ? 'Learned' : 'Empty'}</span>
                    </div>
                `;
            });
        }

        const handleButtonClick = (btnName) => {
            if (isTrainingMode) {
                startLearning(btnName);
            } else {
                controlAC(btnName);
            }
        }

        const closeModals = () => {
            document.getElementById('learn-modal').style.display = 'none';
            if(pollInterval) clearInterval(pollInterval);
        }

        const startLearning = (buttonName) => {
            learningButton = buttonName;
            
            document.getElementById('learn-modal').style.display = 'flex';
            document.getElementById('learn-title').innerText = `Learning [${buttonName}]`;
            document.getElementById('learn-desc').innerText = `Point remote and press ${buttonName}...`;
            document.getElementById('modal-spinner').style.display = 'block';
            document.getElementById('learn-success').style.display = 'none';

            fetch('/api/learn', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({ button: buttonName }) })
              .then(r => {
                if (!r.ok) throw new Error("HTTP error");
                pollInterval = setInterval(pollLearnStatus, 1000);
              })
                .catch(err => {
                    console.error("Error starting learning:", err);
                    notify("Failed to start learning");
                    closeModals();
                });
        }

        const pollLearnStatus = () => {
            fetch('/api/learn/status')
                .then(r => {
                    if (!r.ok) throw new Error("HTTP error");
                    return r.json();
                })
                .then(data => {
                  if(data.status === "success") {
                    clearInterval(pollInterval);
                    document.getElementById('modal-spinner').style.display = 'none';
                    if (data.saved) {
                      // Server auto-saved the capture, skip re-upload
                      document.getElementById('learn-success').style.display = 'block';
                      setTimeout(() => { closeModals(); loadButtons(); }, 1200);
                    } else {
                      saveLearnedButton(data.data);
                    }
                  } else if (data.status === "timeout") {
                    clearInterval(pollInterval);
                    document.getElementById('learn-title').innerText = "Timeout";
                    document.getElementById('learn-desc').innerText = "No signal received. Please try again.";
                    document.getElementById('modal-spinner').style.display = 'none';
                  }
                })
                .catch(err => {
                    console.error("Error polling status:", err);
                });
        }

        const saveLearnedButton = (decodedData) => {
            const payload = {
                button: learningButton,
                data: decodedData
            };
            
            fetch('/api/button/save', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(payload)
            })
            .then(r => {
                if (!r.ok) throw new Error("HTTP error");
                return r.text();
            })
            .then(t => {
                document.getElementById('learn-success').style.display = 'block';
                setTimeout(() => {
                    closeModals();
                    loadButtons();
                }, 1500);
            })
            .catch(err => {
                console.error("Error saving button:", err);
                notify("Failed to save button");
                closeModals();
            });
        }

        const controlAC = (buttonName) => {
            notify(`Sending ${buttonName}...`);
            fetch('/api/button/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({ button: buttonName })
            })
            .then(r => {
                if (!r.ok) throw new Error("HTTP error");
                return r.text();
            })
            .then(t => {
                notify(t);
            })
            .catch(err => {
                console.error("Error controlling AC:", err);
                notify("Failed to send command");
            });
        }

        // Safe DOMContentLoaded Init
        window.addEventListener('DOMContentLoaded', () => {
            loadButtons();
        });
    </script>
</body>
</html>
)rawliteral";

// ================= STORAGE MIGRATION =================
void migrateStorage() {
  File root = LittleFS.open("/", "r");
  if (!root) return;
  
  std::vector<String> filesToRename;
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (name.startsWith("/dev_") && name.endsWith(".json")) {
      filesToRename.push_back(name);
    }
    file = root.openNextFile();
  }
  root.close();
  
  for (const String& oldName : filesToRename) {
    int lastUnderscore = oldName.lastIndexOf('_');
    if (lastUnderscore != -1) {
      String btnName = oldName.substring(lastUnderscore + 1); 
      String newName = "/btn_" + btnName;
      if (!LittleFS.exists(newName)) {
        File oldF = LittleFS.open(oldName, "r");
        File newF = LittleFS.open(newName, "w");
        if (oldF && newF) {
          while (oldF.available()) {
            newF.write(oldF.read());
          }
          oldF.close();
          newF.close();
          Serial.println("Migrated " + oldName + " to " + newName);
        }
      }
      LittleFS.remove(oldName);
    }
  }
  
  if (LittleFS.exists("/devices.json")) {
    LittleFS.remove("/devices.json");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

  migrateStorage();

  irsend.begin();
  irrecv.enableIRIn(); 

  // WIFI CONNECT
  WiFi.disconnect(true); 
  delay(100);
  WiFi.mode(WIFI_STA);   
  WiFi.setSleep(false);  
  
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting to WiFi");
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed. Starting Access Point Mode...");
    WiFi.softAP("Smart-AC-Hub-test", "12345678");
    isAPMode = true;
    Serial.print("AP IP Address: ");
    Serial.println(WiFi.softAPIP());
  }

  // ================= WEB ROUTES =================

  server.on("/", []() {
    // Force browser to load fresh webpage to clear cached outdated client files
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(200, "text/html", webpage);
  });

  server.on("/api/buttons", HTTP_GET, []() {
    DynamicJsonDocument doc(2048);
    JsonObject obj = doc.to<JsonObject>();
    const char* STANDARD_BUTTONS[] = {"ON", "OFF", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"};
    
    for (const char* btn : STANDARD_BUTTONS) {
      String btnFile = "/btn_" + String(btn) + ".json";
      if (LittleFS.exists(btnFile)) {
        obj[btn] = true;
      }
    }
    
    String response;
    serializeJson(doc, response);
    sendJsonResponse(200, response);
  });

  // Diagnostics: list saved btn files and their raw lengths + samples
  server.on("/api/debug/buttons", HTTP_GET, []() {
    DynamicJsonDocument outDoc(4096);
    JsonArray arr = outDoc.createNestedArray("files");

    File root = LittleFS.open("/", "r");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        String name = file.name();
        if (name.startsWith("/btn_") && name.endsWith(".json")) {
          JsonObject info = arr.createNestedObject();
          info["name"] = name.substring(1); // strip leading /
          size_t sz = file.size();
          info["size"] = (uint32_t)sz;

          String content;
          content.reserve(sz + 16);
          while (file.available()) content += (char)file.read();

          DynamicJsonDocument d(sz * 2 + 256);
          DeserializationError err = deserializeJson(d, content);
          if (err) {
            info["parsed"] = false;
            info["error"] = err.c_str();
          } else {
            info["parsed"] = true;
            if (d.containsKey("raw") && d["raw"].is<JsonArray>()) {
              JsonArray raw = d["raw"].as<JsonArray>();
              info["raw_len"] = (uint32_t)raw.size();
              JsonArray sample = info.createNestedArray("sample");
              // include up to first 6 values and last 2 values
              size_t take = raw.size();
              for (size_t i = 0; i < raw.size() && i < 6; i++) sample.add((unsigned long)raw[i].as<unsigned long>());
              if (raw.size() > 8) {
                sample.add("...and...");
                sample.add((unsigned long)raw[raw.size() - 2].as<unsigned long>());
                sample.add((unsigned long)raw[raw.size() - 1].as<unsigned long>());
              }
            }
          }
        }
        file = root.openNextFile();
      }
      root.close();
    }

    String resp;
    serializeJson(outDoc, resp);
    sendJsonResponse(200, resp);
  });

  // Fallback endpoint for older browser versions that request legacy /api/devices
  server.on("/api/devices", HTTP_GET, []() {
    sendJsonResponse(200, "{}");
  });

  server.on("/api/learn", HTTP_POST, []() {
    isLearning = true;
    learningStartTime = millis();
    lastDecodedJSON = "{}";
    pendingButtonName = "";
    // If caller provided a JSON body with {"button":"NAME"}, honor it and auto-save
    if (server.hasArg("plain")) {
      DynamicJsonDocument req(256);
      DeserializationError err = deserializeJson(req, server.arg("plain"));
      if (!err && req.containsKey("button")) {
        pendingButtonName = req["button"].as<String>();
        pendingButtonName.trim();
      }
    }
    irrecv.resume();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Learning Started");
  });

  server.on("/api/learn/status", HTTP_GET, []() {
    if (isLearning) {
      StaticJsonDocument<256> doc;
      if (millis() - learningStartTime > 15000) {
        isLearning = false;
        doc["status"] = "timeout";
      } else {
        doc["status"] = "learning";
      }
      String response; serializeJson(doc, response); sendJsonResponse(200, response);
      return;
    }

    if (lastDecodedJSON != "{}") {
      // lastDecodedJSON already contains a proper JSON object; embed it directly to avoid re-parsing large payloads
      String response = String("{\"status\":\"success\",\"data\":") + lastDecodedJSON;
      if (lastSavedButtonName.length() > 0) {
        response += String(",\"saved\":\"") + lastSavedButtonName + '"';
        lastSavedButtonName = "";
      }
      response += "}";
      lastDecodedJSON = "{}";
      sendJsonResponse(200, response);
      return;
    }

    StaticJsonDocument<256> idleDoc;
    idleDoc["status"] = "idle";
    String response; serializeJson(idleDoc, response); sendJsonResponse(200, response);
  });

  server.on("/api/button/save", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(400, "text/plain", "Body not found");
    }

    String body = server.arg("plain");
    
    // We'll stream the resulting JSON to LittleFS to avoid in-memory limits
    String btnName = "";

    // Case A: C-style array in body: parse tokens and stream directly
    if (body.indexOf("uint16_t") >= 0 && body.indexOf('{') >= 0) {
      int braceOpen = body.indexOf('{');
      int braceClose = body.indexOf('}', braceOpen);
      if (braceOpen > 0 && braceClose > braceOpen) {
        int nameStart = body.indexOf(' ', body.indexOf("uint16_t")) + 1;
        int nameEnd = body.indexOf('[', nameStart);
        if (nameStart > 0 && nameEnd > nameStart) {
          btnName = body.substring(nameStart, nameEnd);
          btnName.trim();
        }
        if (btnName.length() == 0) {
          server.sendHeader("Access-Control-Allow-Origin", "*");
          return server.send(400, "text/plain", "Missing button name");
        }

        String btnFile = "/btn_" + btnName + ".json";
        File f = LittleFS.open(btnFile, "w");
        if (!f) {
          server.sendHeader("Access-Control-Allow-Origin", "*");
          return server.send(500, "text/plain", "Write Error");
        }

        // Write JSON prefix
        f.print("{\"raw\":[");

        String inside = body.substring(braceOpen + 1, braceClose);
        int start = 0;
        bool first = true;
        while (start < inside.length()) {
          int comma = inside.indexOf(',', start);
          String tok;
          if (comma == -1) {
            tok = inside.substring(start);
            start = inside.length();
          } else {
            tok = inside.substring(start, comma);
            start = comma + 1;
          }
          tok.trim();
          if (tok.length() > 0) {
            if (!first) f.print(',');
            f.print((unsigned long)tok.toInt());
            first = false;
          }
        }

        // Close JSON and write empty metadata fields
        f.print("], \"protocol\":\"\", \"bits\":0}");
        f.close();

        // Also write C export
        String cFile = "/btn_" + btnName + ".c";
        File cf = LittleFS.open(cFile, "w");
        if (cf) {
          cf.print("uint16_t "); cf.print(btnName); cf.print("[] = {\n");
          // rewrite inside tokens for C file
          start = 0; first = true;
          while (start < inside.length()) {
            int comma = inside.indexOf(',', start);
            String tok;
            if (comma == -1) {
              tok = inside.substring(start);
              start = inside.length();
            } else {
              tok = inside.substring(start, comma);
              start = comma + 1;
            }
            tok.trim();
            if (tok.length() > 0) {
              if (!first) cf.print(',');
              cf.print((unsigned long)tok.toInt());
              first = false;
            }
          }
          cf.print("\n};\n");
          cf.close();
        }

        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", "Button Saved!");
        return;
      }
    }

    // Case B: JSON body. Parse with a document sized from body length to avoid truncation,
    // then stream the `raw` array into file to avoid building huge intermediate arrays.
    size_t approxCapacity = body.length() * 2 + 1024;
    DynamicJsonDocument req(approxCapacity);
    DeserializationError error = deserializeJson(req, body);
    if (error) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(400, "text/plain", "Invalid payload format");
    }
    btnName = req["button"].as<String>();
    if (btnName.length() == 0) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(400, "text/plain", "Missing button name");
    }

    String btnFile = "/btn_" + btnName + ".json";
    File f = LittleFS.open(btnFile, "w");
    if (!f) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(500, "text/plain", "Write Error");
    }

    // Begin JSON
    f.print('{');
    f.print("\"raw\":[");

    // Try common locations for raw array
    JsonArray src;
    if (req.containsKey("data") && req["data"].is<JsonObject>() && req["data"].containsKey("raw")) {
      src = req["data"]["raw"].as<JsonArray>();
    } else if (req.containsKey("raw") && req["raw"].is<JsonArray>()) {
      src = req["raw"].as<JsonArray>();
    } else if (req.is<JsonArray>()) {
      src = req.as<JsonArray>();
    }

    bool first = true;
    for (JsonVariant v : src) {
      if (!first) f.print(',');
      unsigned long val = v.as<unsigned long>();
      f.print(val);
      first = false;
    }

    // add metadata if present
    f.print("],\"");
    f.print("protocol\":\"");
    if (req.containsKey("data") && req["data"].is<JsonObject>() && req["data"].containsKey("protocol")) {
      String p = req["data"]["protocol"].as<String>();
      f.print(p);
    } else if (req.containsKey("protocol")) {
      String p = req["protocol"].as<String>();
      f.print(p);
    }
    f.print("\",\"bits\":");
    int bits = 0;
    if (req.containsKey("data") && req["data"].is<JsonObject>() && req["data"].containsKey("bits")) bits = req["data"]["bits"].as<int>();
    else if (req.containsKey("bits")) bits = req["bits"].as<int>();
    f.print(bits);
    f.print('}');
    f.close();

    // Also write a C-style export
    String cFile = "/btn_" + btnName + ".c";
    File cf = LittleFS.open(cFile, "w");
    if (cf) {
      cf.print("uint16_t "); cf.print(btnName); cf.print("[] = {\n");
      first = true;
      for (JsonVariant v : src) {
        if (!first) cf.print(',');
        cf.print((unsigned long)v.as<unsigned long>());
        first = false;
      }
      cf.print("\n};\n");
      cf.close();
    }

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Button Saved!");
  });

  server.on("/api/button/control", HTTP_POST, []() {
    if (!server.hasArg("plain")) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(400, "text/plain", "Body not found");
    }
    
    DynamicJsonDocument req(512);
    DeserializationError error = deserializeJson(req, server.arg("plain"));
    if (error) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      return server.send(400, "text/plain", "Invalid JSON");
    }
    
    String btnName = req["button"];
    
    String btnFile = "/btn_" + btnName + ".json";
    if (LittleFS.exists(btnFile)) {
      File file = LittleFS.open(btnFile, "r");
      size_t fileSize = file.size();
      // Read file into string then parse with appropriately sized document
      String fileContent;
      fileContent.reserve(fileSize + 16);
      while (file.available()) fileContent += (char)file.read();
      file.close();

      DynamicJsonDocument btnDoc(fileSize * 2 + 1024);
      DeserializationError readError = deserializeJson(btnDoc, fileContent);
      
      if (!readError && btnDoc.containsKey("raw")) {
        JsonArray rawArr = btnDoc["raw"].as<JsonArray>();
        size_t sz = rawArr.size();
        if (sz > 0) {
          // Debug: print stored metadata and a sample of raw timings
          if (btnDoc.containsKey("protocol")) {
            Serial.print("Protocol: "); Serial.println((const char*)btnDoc["protocol"]);
          }
          if (btnDoc.containsKey("bits")) {
            Serial.print("Bits: "); Serial.println((int)btnDoc["bits"]);
          }
          Serial.print("Sending raw length: "); Serial.println((int)sz);
          Serial.print("Raw sample: ");
          for (size_t i = 0; i < sz && i < 12; i++) {
            Serial.print((unsigned long)rawArr[i].as<unsigned long>());
            Serial.print(i + 1 < sz && i < 11 ? "," : "\n");
          }

          // Allocate as uint16_t for IR library and clamp values to uint16_t range
          uint16_t* rawData = new uint16_t[sz];
          for (size_t i = 0; i < sz; i++) {
            unsigned long v = rawArr[i].as<unsigned long>();
            rawData[i] = (v > 0xFFFFUL) ? 0xFFFF : (uint16_t)v;
          }

          // Use 38kHz by default; consider storing frequency in JSON if needed
          irsend.sendRaw(rawData, (uint16_t)sz, 38);
          delete[] rawData;
          
          server.sendHeader("Access-Control-Allow-Origin", "*");
          server.send(200, "text/plain", "Command Sent");
          return;
        }
      }
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "text/plain", "Command not found");
  });

  server.onNotFound([]() {
    if (server.uri() == "/favicon.ico") {
      server.send(204, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  server.begin();
  Serial.println("Web Server Started");
}

// ================= LOOP =================
void loop() {
  server.handleClient();
  
  if (isLearning) {
    if (irrecv.decode(&results)) {
      if (results.overflow) {
        Serial.printf("WARNING: IR capture buffer overflow! Some data was lost. Try increasing kCaptureBufferSize (currently %d).\n", kCaptureBufferSize);
      }
      // Build JSON string dynamically to avoid JsonDocument size limits for long captures
      uint16_t *raw_array = resultToRawArray(&results);
      uint16_t raw_length = getCorrectedRawLength(&results);

      String json;
      // reserve approximate size (estimate 6 bytes per entry)
      json.reserve(64 + (size_t)raw_length * 6);
      json += "{";
      json += "\"protocol\":\"";
      json += typeToString(results.decode_type);
      json += "\",";
      json += "\"bits\":";
      json += String(results.bits);
      json += ",\"raw\": [";

      for (uint16_t i = 0; i < raw_length; i++) {
        if (i) json += ',';
        json += String((unsigned long)raw_array[i]);
      }
      json += "]}";

      // set lastDecodedJSON for /api/learn/status
      lastDecodedJSON = json;
      // Auto-save if a button name was provided when learning started (stream raw_array)
      if (pendingButtonName.length() > 0) {
        String btnFile = "/btn_" + pendingButtonName + ".json";
        File f = LittleFS.open(btnFile, "w");
        if (f) {
          // Write JSON with raw array and metadata
          f.print('{');
          f.print("\"raw\":[");
          for (uint16_t i = 0; i < raw_length; i++) {
            if (i) f.print(',');
            f.print((unsigned long)raw_array[i]);
          }
          f.print("]");
          f.print(",\"protocol\":\"");
          f.print(typeToString(results.decode_type));
          f.print("\",\"bits\":");
          f.print(results.bits);
          f.print('}');
          f.close();

          // Also create C export
          String cFile = "/btn_" + pendingButtonName + ".c";
          File cf = LittleFS.open(cFile, "w");
          if (cf) {
            cf.print("uint16_t "); cf.print(pendingButtonName); cf.print("[] = {\n");
            for (uint16_t i = 0; i < raw_length; i++) {
              if (i) cf.print(',');
              cf.print((unsigned long)raw_array[i]);
            }
            cf.print("\n};\n");
            cf.close();
          }

          Serial.printf("Auto-saved learned button: %s\n", pendingButtonName.c_str());
          lastSavedButtonName = pendingButtonName;
        } else {
          Serial.printf("Failed to auto-save button: %s\n", pendingButtonName.c_str());
        }
        pendingButtonName = "";
      }
      isLearning = false;
      irrecv.resume();
      delete[] raw_array;
    }
  }
  delay(1); 
}