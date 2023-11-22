#include <ESP8266WebServer.h>
#include <ESPAsyncTCP.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "SuN";
const char* password = "180825450";
const char* mqtt_server = "192.168.208.200";
const int ledPin = D6; 
bool isLedOn = false;

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht11(D4, DHT11);

unsigned long previousMillis = 0;
const long interval = 5000;

ESP8266WebServer server(80);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void handleRoot() {
  String ledStatus = isLedOn ? "on" : "off";

  String html = "<html><body>";
  html += "<h1>LED Control</h1>";
  html += "<p>LED Status: " + ledStatus + "</p>";
  html += "<form action='/led' method='post'>";
  html += "<input type='submit' name='led' value='Toggle LED'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht11.begin();

  // Start the server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/led", HTTP_POST, []() {
    isLedOn = !isLedOn;
    digitalWrite(ledPin, isLedOn ? HIGH : LOW);
    client.publish("led/status", isLedOn ? "on" : "off");
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    
    previousMillis = currentMillis;

    float humidity = dht11.readHumidity();
    float temperature = dht11.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    Serial.printf("Humidity: %.2f %%\tTemperature: %.2f °C\tIP: %s\n", humidity, temperature, WiFi.localIP().toString().c_str());

    StaticJsonDocument<200> doc;
    doc["humidity"] = humidity;
    doc["temperature"] = temperature;
    doc["ip"] = WiFi.localIP().toString();

    String jsonStr;
    serializeJson(doc, jsonStr);

    client.publish("dht11", jsonStr.c_str());
  }

  // Handle HTTP requests
  server.handleClient();
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "led") {
    if (message == "on") {
      digitalWrite(ledPin, HIGH);
      isLedOn = true;
    } else if (message == "off") {
      digitalWrite(ledPin, LOW);
      isLedOn = false;
    }
    
    client.publish("led/status", isLedOn ? "on" : "off");
  }
}
