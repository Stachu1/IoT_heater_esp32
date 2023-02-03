#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>

#define EEPROM_SIZE 6
#define EEPROM_OFFSET 0
#define LED 2
#define OUTPUT_PIN 5

const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* time_server_url = "http://worldtimeapi.org/api/timezone/Europe/Warsaw";

WiFiServer server(80);
HTTPClient http;

const int timeout = 2000;
const int get_time_delay = 3600000;
// const long get_time_delay = 10000;
int previous_time_update = -get_time_delay;
int current_time_updated = 0;
int current_time = 0;

int start_time;
int duration_time;


void setup() {
  Serial.begin(115200);
  EEPROM.begin(6 + EEPROM_OFFSET);
  pinMode(LED, OUTPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  // digitalWrite(OUTPUT_PIN, HIGH);

  digitalWrite(LED, HIGH);
  connect_to_wifi();
  digitalWrite(LED, LOW);

  start_time = get_start_time();
  duration_time = get_duration_time();
}



void loop() {
  console_service();

  if (WiFi.status() == WL_CONNECTED) {
    if ((millis() - previous_time_update) > get_time_delay) {
      current_time_updated = get_current_time();
      previous_time_update = millis();
      current_time = current_time_updated;
    }
    else {
      current_time = current_time_updated + (millis() - previous_time_update) / 1000;
    }
    WiFiClient client = server.available();
    if (client) {
      client_service(client);
    }
    if (current_time >= start_time && current_time < (start_time + duration_time)) {
      // output_state = "on";
      digitalWrite(LED, HIGH);
      digitalWrite(OUTPUT_PIN, LOW);
    }
    else {
      // output_state = "off";
      digitalWrite(OUTPUT_PIN, HIGH);
      digitalWrite(LED, LOW);
    }
  }
  else {
    connect_to_wifi();
  }
}


void console_service() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    if (command == "EEPROM=0") {
      set_eeprom_zero();
      Serial.print(command);
      start_time = get_start_time();
      duration_time = get_duration_time();
      Serial.println(" [DONE]");
    }
    if (command == "EEPROM") {
      Serial.print("\nEEPROM:");
      for (int i=EEPROM_OFFSET; i<EEPROM_SIZE; i++) {
        Serial.print(" ");
        Serial.print(EEPROM.read(i));
      }
      Serial.print("\nStart_time: ");
      Serial.println(get_start_time());
      Serial.print("Duration_time: ");
      Serial.println(get_duration_time());
      Serial.println("[DONE]\n");
    }
  }
}



int get_current_time() {
  http.begin(time_server_url);
  int httpResponseCode = http.GET();
  Serial.println("");
  if (httpResponseCode > 0) {
    Serial.print("Time API HTTP response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();

    JSONVar data = JSON.parse(payload);
    if (JSON.typeof(data) == "undefined") {
      Serial.println("Parsing input failed!");
      return get_current_time();
    }

    String datetime = JSON.stringify(data["datetime"]);
    String time = datetime.substring(12, 20);
    Serial.print("Current time: ");
    Serial.println(time);

    int time_seconds = time_string_to_seconds(time);
    return time_seconds;
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    delay(3000);
    Serial.print("Retrying...");
    return get_current_time();
  }
}



String seconds_to_time_string(int seconds) {
  int minutes = seconds / 60;
  int hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  String time = "";
  if (hours < 10) {
    time += "0";
  }
  time += String(hours, DEC);
  time += ":";
  if (minutes < 10) {
    time += "0";
  }
  time += String(minutes, DEC);
  return time;
}



int time_string_to_seconds(String time) {
  int hours = time.substring(0,2).toInt();
  int minutes = time.substring(3,5).toInt();
  int seconds = time.substring(6,8).toInt();
  return hours * 3600 + minutes * 60 + seconds;
}



void set_eeprom_zero() {
  for (int i=EEPROM_OFFSET; i < EEPROM_SIZE + EEPROM_OFFSET; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}



int get_start_time() {
  int st = 0;
  st += EEPROM.read(EEPROM_OFFSET) * 3600;
  st += EEPROM.read(EEPROM_OFFSET + 1) * 60;
  st += EEPROM.read(EEPROM_OFFSET + 2);
  return st;
}



int get_duration_time() {
  int dt = 0;
  dt += EEPROM.read(EEPROM_OFFSET + 3) * 3600;
  dt += EEPROM.read(EEPROM_OFFSET + 4) * 60;
  dt += EEPROM.read(EEPROM_OFFSET + 5);
  return dt;
}



void save_settings() {
  int st = start_time;
  int dt = duration_time;

  int minutes = st / 60;
  int hours = minutes / 60;
  st %= 60;
  minutes %= 60;
  hours %= 24;

  EEPROM.write(EEPROM_OFFSET, hours);
  EEPROM.write(EEPROM_OFFSET + 1, minutes);
  EEPROM.write(EEPROM_OFFSET + 2, st);
  
  minutes = dt / 60;
  hours = minutes / 60;
  dt %= 60;
  minutes %= 60;
  hours %= 24;

  EEPROM.write(EEPROM_OFFSET + 3, hours);
  EEPROM.write(EEPROM_OFFSET + 4, minutes);
  EEPROM.write(EEPROM_OFFSET + 5, dt);
  EEPROM.commit();
}



void connect_to_wifi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}



void client_service(WiFiClient client) {
  int current_millis = millis();
  int previous_millis = current_millis;
  Serial.println("New Client.");
  String header;
  String currentLine = "";
  char data;
  while (client.connected() && current_millis - previous_millis <= timeout) {
    current_millis = millis();
    if (client.available()) {
      data = client.read();
      Serial.write(data);
      header += data;
      if (data == '\n') {
        if (currentLine.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          }
          if (header.indexOf("GET /start_time_add_30m") >= 0) {
            start_time += 1800;
            save_settings();
          }
          else if (header.indexOf("GET /start_time_subtract_30m") >= 0) {
            start_time -= 1800;
            save_settings();
          }
          else if (header.indexOf("GET /duration_time_add_5m") >= 0) {
            duration_time += 300;
            save_settings();
          }
          else if (header.indexOf("GET /duration_time_subtract_5m") >= 0) {
            duration_time -= 300;
            save_settings();
          }
          

          
          client.println("<!DOCTYPE html><html>");
          client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
          client.println("<link rel=\"icon\" href=\"data:,\">");

          client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
          client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
          client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
          client.println(".button2 {background-color: #af4c4c;}</style></head>");
          
          client.println("<body style='background-color:#1c1c1c;'><h1 style='color:white'>IoT Heater</h1>");
          
          client.println("<p style='color:white'>Current time: " + seconds_to_time_string(current_time) + "</p>");
          client.println("<p> </p>");
          client.println("<p> </p>");
          client.println("<p> </p>");
          
          client.println("<p style='color:white'>Start time: " + seconds_to_time_string(start_time) + "</p>"); 
          client.println("<p><a href=\"/start_time_add_30m\"><button class=\"button\">+30min</button></a></p>");
          client.println("<p><a href=\"/start_time_subtract_30m\"><button class=\"button button2\">-30min</button></a></p>");
          client.println("<p style='color:white'>Duration time: " + seconds_to_time_string(duration_time) + "</p>"); 
          client.println("<p><a href=\"/duration_time_add_5m\"><button class=\"button\">+5min</button></a></p>");
          client.println("<p><a href=\"/duration_time_subtract_5m\"><button class=\"button button2\">-5min</button></a></p>");
          
          client.println("</body></html>");
          
          client.println();
          break;
        }
        else {
          currentLine = "";
        }
      }

    else if (data != '\r') {
      currentLine += data;
    }
  }
  
  header = "";
  client.stop();
  Serial.println("Client disconnected.");
  Serial.println("");
}
