#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WebServer server(80);
const int ledAstable = 13;  // LED astable
const int outputMono = 14;  // Salida monoestable
const int buttonMono = 0;   // Botón BOOT (GPIO0)

unsigned long previousMillis = 0;
float frequency = 0; // Hz
bool ledState = LOW;

unsigned long monoPulseDuration = 0;
unsigned long monoStartTime = 0;
bool monoActive = false;

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Simulador NE555</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #B0E0E6; text-align: center; padding: 30px; }
    .container { background-color: white; padding: 30px; border-radius: 12px; display: inline-block; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
    h1 { margin-bottom: 20px; }
    input, button { padding: 10px; margin: 5px; font-size: 16px; border-radius: 5px; }
    button { background-color: #28a745; color: white; border: none; }
    button:hover { background-color: #218838; cursor: pointer; }
    .result { margin-top: 15px; font-size: 18px; }
    .formula { font-size: 14px; color: #555; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Simulador NE555</h1>

    <h2>Modo Astable</h2>
    <label>R1 (Ω): <input type="number" id="R1_astable"></label><br>
    <label>R2 (Ω): <input type="number" id="R2_astable"></label><br>
    <label>C1 (F): <input type="number" id="C1_astable" step="any"></label><br>
    <button onclick="calcularAstable()">Calcular Astable</button>
    <div class="result" id="resultadoAstable"></div>
    <div class="formula">
      F = 1.44 / ((R1 + 2×R2) × C1)
    </div>

    <hr style="margin: 30px 0;">

    <h2>Modo Monoestable</h2>
    <label>R (Ω): <input type="number" id="R_mono"></label><br>
    <label>C (F): <input type="number" id="C_mono" step="any"></label><br>
    <button onclick="calcularMonoestable()">Calcular Monoestable</button>
    <div class="result" id="resultadoMono"></div>
    <div class="formula">
      T = 1.1 × R × C
    </div>
  </div>

  <script>
    function calcularAstable() {
      const R1 = parseFloat(document.getElementById("R1_astable").value);
      const R2 = parseFloat(document.getElementById("R2_astable").value);
      const C1 = parseFloat(document.getElementById("C1_astable").value);
      if (R1 > 0 && R2 > 0 && C1 > 0) {
        const F = 1.44 / ((R1 + 2 * R2) * C1);
        document.getElementById("resultadoAstable").textContent = `Frecuencia: ${F.toFixed(2)} Hz`;
        fetch(`/set?freq=${F.toFixed(2)}`);
      }
    }

    function calcularMonoestable() {
      const R = parseFloat(document.getElementById("R_mono").value);
      const C = parseFloat(document.getElementById("C_mono").value);
      if (R > 0 && C > 0) {
        const T = 1.1 * R * C * 1000; // ms
        document.getElementById("resultadoMono").textContent = `Duración del pulso: ${T.toFixed(0)} ms`;
        fetch(`/setmono?time=${T.toFixed(0)}`);
      }
    }
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSetAstable() {
  if (server.hasArg("freq")) {
    frequency = server.arg("freq").toFloat();
    Serial.print("Nueva frecuencia astable: ");
    Serial.print(frequency);
    Serial.println(" Hz");
  }
  server.send(200, "text/plain", "OK");
}

void handleSetMonoestable() {
  if (server.hasArg("time")) {
    monoPulseDuration = server.arg("time").toInt();
    Serial.print("Nuevo tiempo monoestable: ");
    Serial.print(monoPulseDuration);
    Serial.println(" ms");
  }
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  pinMode(ledAstable, OUTPUT);
  pinMode(outputMono, OUTPUT);
  pinMode(buttonMono, INPUT_PULLUP);

  WiFi.softAP(ssid, password);
  Serial.println("AP iniciado. IP:");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/set", handleSetAstable);
  server.on("/setmono", handleSetMonoestable);

  server.begin();
  Serial.println("Servidor web iniciado");
}

void loop() {
  server.handleClient();

  // ASTABLE
  if (frequency > 0.01) {
    unsigned long interval = 1000.0 / (frequency * 2);
    if (millis() - previousMillis >= interval) {
      previousMillis = millis();
      ledState = !ledState;
      digitalWrite(ledAstable, ledState);
    }
  } else {
    digitalWrite(ledAstable, LOW);
    ledState = LOW;
  }

  // MONOESTABLE
  if (digitalRead(buttonMono) == LOW && !monoActive && monoPulseDuration > 0) {
    monoActive = true;
    monoStartTime = millis();
    digitalWrite(outputMono, HIGH);
    Serial.println("Pulso monoestable activado");
    delay(200); // debounce simple
  }

  if (monoActive && (millis() - monoStartTime >= monoPulseDuration)) {
    monoActive = false;
    digitalWrite(outputMono, LOW);
    Serial.println("Pulso monoestable terminado");
  }
}
