#include <DHTesp.h>
#include "esp_camera.h"
#include <WiFi.h>

const char* ssid = "***";
const char* password = "***";

// // Sensor configuration
#define DHT_PIN 2
#define FLASH_GPIO_NUM 4
#define SOUND_PIN 12
#define PIR_PIN 13

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

DHTesp dht;
float temp_prev = 0.0;
float hum_prev = 0.0;

WiFiServer server(80);
WiFiClient live_client;

bool connected = false;

String index_html;

String indexHtml[] = {
    "<meta charset=\"utf-8\"/>\n",
    "<style>\n",
    "#content {\n",
    "display: flex;\n",
    "flex-direction: column;\n",
    "justify-content: center;\n",
    "align-items: center;\n",
    "text-align: center;\n",
    "min-height: 100vh;}\n",
    "</style>\n",
    "<body bgcolor=\"#000000\"><div id=\"content\"><h2 style=\"color:#ffffff\">HTTP ESP32 Cam live stream in server_ip</h2><img src=\"video\">\n",
    "<p style=\"color:#ffffff\">Temparature: None, Humidity: None<p>\n",
    "<p style=\"color:#8B0000\" hidden> INTRUDER DETECTED.</p>\n",
    "<p style=\"color:#7CFC00\" hidden> SOUND DETECTED.</p>\n",
    "</div></body>"
  };


String concatenateStrings(String* strings, int count) {
  String result = "";
  for (int i = 0; i < count; i++) {
    result += strings[i];
  }
  return result;
}


void configCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 9;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

//continue sending camera frame
void liveCam(WiFiClient &client){
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("Frame buffer could not be acquired");
      return;
  }
  client.print("--frame\n");
  client.print("Content-Type: image/jpeg\n\n");
  client.flush();
  client.write(fb->buf, fb->len);
  client.flush();
  client.print("\n");
  //return the frame buffer back to be reused
  esp_camera_fb_return(fb);
}

void http_resp(){

  WiFiClient client = server.available();                
    /* check client is connected */           
  if (client.connected()) {     
      /* client send request? */     
      /* request end with '\r' -> this is HTTP protocol format */
      
      String req = "";
      while(client.available()){
        req += (char)client.read();
      }
      Serial.println("request " + req);
      /* First line of HTTP request is "GET / HTTP/1.1"  
        here "GET /" is a request to get the first page at root "/"
        "HTTP/1.1" is HTTP version 1.1
      */
      /* now we parse the request to see which page the client want */
      int addr_start = req.indexOf("GET") + strlen("GET");
      int addr_end = req.indexOf("HTTP", addr_start);
      if (addr_start == -1 || addr_end == -1) {
          Serial.println("Invalid request " + req);
          return;
      }
      req = req.substring(addr_start, addr_end);
      req.trim();
      Serial.println("Request: " + req);
      client.flush();
  
      String s;
      /* if request is "/" then client request the first page at root "/" -> we process this by return "Hello world"*/
      if (req == "/")
      {
          s = "HTTP/1.1 200 OK\n";
          s += "Content-Type: text/html\n\n";
          s += index_html;
          s += "\n";
          client.print(s);
          client.stop();
      }
      else if (req == "/video")
      {
          live_client = client;
          live_client.print("HTTP/1.1 200 OK\n");
          live_client.print("Content-Type: multipart/x-mixed-replace; boundary=frame\n\n");
          live_client.flush();
          connected = true;
      }
      else
      {
          /* if we can not find the page that client request then we return 404 File not found */
          s = "HTTP/1.1 404 Not Found\n\n";
          client.print(s);
          client.stop();
      }
    }       
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(PIR_PIN,INPUT_PULLUP);

  digitalWrite(FLASH_GPIO_NUM, HIGH);

  dht.setup(DHT_PIN, DHTesp::DHT11);
	Serial.println("DHT initiated");

  pinMode(PIR_PIN, INPUT_PULLUP);

  // Give some time for the PIR sensor to warm up
  Serial.println("Waiting for PIR sensor to warm up on first boot");
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(FLASH_GPIO_NUM, LOW);
    delay(500);
    Serial.print(".");
    digitalWrite(FLASH_GPIO_NUM, HIGH);
  }
  Serial.println("Connected");
  String IP = WiFi.localIP().toString();
  Serial.println("IP address: " + IP);
  indexHtml[10].replace("server_ip",IP);
  index_html= concatenateStrings(indexHtml, sizeof(indexHtml) / sizeof(indexHtml[0]));

  server.begin();
  configCamera();
  digitalWrite(FLASH_GPIO_NUM, LOW);

}
    

void loop() {

  String nstr;

  digitalWrite(FLASH_GPIO_NUM, LOW);

  if (digitalRead(SOUND_PIN) == LOW){
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    nstr = "<p style=\"color:#7CFC00\"> SOUND DETECTED.</p>\n";
  }
  else nstr = "<p style=\"color:#7CFC00\" hidden> SOUND DETECTED.</p>\n";

  indexHtml[13] = nstr;


  if (digitalRead(PIR_PIN) == LOW){
    digitalWrite(FLASH_GPIO_NUM, HIGH);
    nstr ="<p style=\"color:#8B0000\"> INTRUDER DETECTED.</p>\n";
  }
  else nstr = "<p style=\"color:#8B0000\" hidden> INTRUDER DETECTED.</p>\n";

  indexHtml[12] = nstr;
  

  float h = dht.getHumidity();
  float t = dht.getTemperature();

 if (!isnan(h) && !isnan(t) && h != hum_prev && t != temp_prev){
    hum_prev = h;
    temp_prev = t;
    String nstr = "<p style=\"color:#ffffff\">Temparature: "+ String(t,2) +"°C, Humidity: "+ String(h) +"%<p>\n";
    indexHtml[11] = nstr;
     
  }

  index_html = concatenateStrings(indexHtml, sizeof(indexHtml) / sizeof(indexHtml[0]));

  http_resp();

  if(connected == true){
    liveCam(live_client);
  }
  
  delay(100);

}
