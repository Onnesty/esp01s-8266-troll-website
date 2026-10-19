// On ESP-01S
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>



const String VictimsPath = "/Victims.txt";
const String SSIDPath = "/SSID.txt";


AsyncWebServer Server(80); // Creates a Server ( No Launch Yet )
DNSServer DNS;
const byte DNS_PORT = 53; // Default
IPAddress apIP(192, 168, 69, 1); // IP for the Server Created



String GetSSID() { // Initialize the SSID from LittleFS
  
  File file = LittleFS.open(SSIDPath, "r");

  String SSID = "DefaultWifi";

  if (file) {

    SSID = file.readStringUntil('\n');
    SSID.trim();

    file.close();

  }

  
  return SSID;

}

int GetVictimCount() {
   
  File file = LittleFS.open(VictimsPath, "r");

  if (!file) return -1;

  int Count = 0; 
  
  while (file.available()) {
    
    String Str = file.readStringUntil('\n');
    Str.trim();

    if (Str.length() > 0) Count++;
  }
  

  file.close();
  
  return Count;

}


void setup() {

  Serial.begin(115200);
  
  // Initialize LittleFS File System
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart(); // Restart as this is a vital component 
  }

  // Initialize Victims.txt
  if (!LittleFS.exists(VictimsPath)) { // If File Doesnt Exist
    
    Serial.println("Victims.txt doesnt exist. Creating new one");

    File file = LittleFS.open(VictimsPath, "w");

    file.close();
  }
  // Initialize SSID.txt
  if (!LittleFS.exists(SSIDPath)) { // If File Doesnt Exist
    
    Serial.println("SSID.txt doesnt exist. Creating new one");

    File file = LittleFS.open(SSIDPath, "w");

    file.write("DefaultWifi");

    file.close();
  }

  const String SSID = GetSSID();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0)); // (ServerIP, Its Gateway Ip, The IP Group it belongs)
  Serial.println("Access Point Started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  DNS.start(DNS_PORT, "*", apIP); // Start DNS that redirects anything to the ESP Server



  // Upon Someone entering the Server
  Server.on("/", HTTP_GET, [](AsyncWebServerRequest *Request) {
    Request->send(LittleFS, "/Website.html", "text/html");
  });

  Server.serveStatic("/Views/", LittleFS, "/Views/");

  Server.on("/AdminPage", HTTP_GET, [](AsyncWebServerRequest *Request) { // Server request for admin page
    Request->send(LittleFS, "/Admin.html", "text/html");  
  });

  Server.on("/NewSSID", HTTP_POST, [](AsyncWebServerRequest *Request) {
    
    Serial.println("New SSID Request");

    if (Request->hasParam("SSIDName", true)) {

      String NewSSID = Request->getParam("SSIDName", true)->value();
      
      File file = LittleFS.open(SSIDPath, "w");
      
      if (file) {
        file.print( NewSSID );

        file.close();
        
      }

      Request->send(200, "text/plain", "New SSID Changed");

      ESP.restart(); // Restart ESP to display the updated troll page


    } else {

      Request->send(400, "text/plain", "Missing Field: StoredSSID");
    }
    
    
  });

  Server.on("/GetVictimCount", HTTP_GET, [](AsyncWebServerRequest *Request) {

    Serial.println("Victim Count Request");

    Request->send(200, "text/plain", String( GetVictimCount() ) );

  });

  Server.on("/NewVictim", HTTP_POST, [](AsyncWebServerRequest *Request) {

    Serial.println("New Victim Request");

    IPAddress clientIP = Request->client()->remoteIP();
    
    struct station_info *stat_info = wifi_softap_get_station_info();
    
    String MAC = "";

    while (stat_info != NULL) { // Search for the connected devices with the same IP, in order to get their MAC address
      IPAddress IP = stat_info->ip;

      if (IP == clientIP) {
       
        for (int i = 0; i < 6; i++) {

          if (stat_info->bssid[i] < 16) MAC += "0";

          String ByteHex = String(stat_info->bssid[i], HEX);
          ByteHex.toUpperCase();
          MAC += ByteHex;
          
          if (i != 5) {
            MAC += ":";
          }
          
        }

        break;

      }

      
      stat_info = STAILQ_NEXT(stat_info, next);

    }
    wifi_softap_free_station_info();

    if (MAC.length() == 0) { // If no MAC Address is found
      Serial.println("Could not find MAC Address");
      Request->send(500, "text/plain", "Could not find MAC Address");
      return;
    }

    File file = LittleFS.open(VictimsPath, "r");

    if (!file) {
      Serial.println("Could not read Victims file");
      Request->send(500, "text/plain", "Could not read Victims file");
      return;
    }

    bool IsVictim = false;
     
    while (file.available()) {
      
      String CurrentMAC = file.readStringUntil('\n');
      CurrentMAC.trim();

      if (CurrentMAC == MAC) {
        IsVictim = true;
        break;
      }
    }

    file.close();
    
    if (!IsVictim) {
      
      File file = LittleFS.open(VictimsPath, "a");

      if (!file) {
        Serial.println("Could not read Victims file");
        Request->send(500, "text/plain", "Could not read Victims file");
        return;
      }

      file.println(MAC);

      file.close();

      Serial.print("Successfully Added New Victim: ");
      Serial.println(MAC);

      Request->send(200, "text/plain", "Successfully Added New Victim");



    } else {

      Request->send(200, "text/plain", "Already a Victim");
    }


  });

  Server.on("/GetConnectedDevices", HTTP_GET, [](AsyncWebServerRequest *Request) {
    
    struct station_info *stat_info = wifi_softap_get_station_info();

    JsonDocument Payload; 

    int UserID = 1;

    while (stat_info != NULL) { 

      IPAddress IP = IPAddress(stat_info->ip);

      String IPString = IP.toString();

      String MACString = "";

      for (int i = 0; i < 6; i++) {

        if (stat_info->bssid[i] < 16) MACString += "0";

        String ByteHex = String(stat_info->bssid[i], HEX);
        ByteHex.toUpperCase();
        MACString += ByteHex;
        
        if (i != 5) {
          MACString += ":";
        }
        
      }

      Payload[String(UserID)]["IP"] = IPString;
      Payload[String(UserID)]["MAC"] = MACString;

      UserID++;

      stat_info = STAILQ_NEXT(stat_info, next);

    }
    wifi_softap_free_station_info();

    String StringPayload;
    serializeJson(Payload, StringPayload);


    Request->send(200, "application/json", StringPayload);

  });

  Server.on("/GetVictims", HTTP_GET, [](AsyncWebServerRequest *Request) {
    
    String Output = "";

    File file = LittleFS.open(VictimsPath, "r");

    if (!file) {
      Serial.println("Could not read Victims file");
      Request->send(500, "text/plain", "Could not read Victims file");
      return;
    }
     
    Output = file.readString();

    file.close();
    
    Request->send(200, "text/plain", Output);

  });

  Server.on("/DeleteVictims", HTTP_DELETE, [](AsyncWebServerRequest *Request) {
    
    File file = LittleFS.open(VictimsPath, "w");
    
    if (!file) {
      Serial.println("Failed to Delete Victims");
      Request->send(500, "text/plain", "Failed to Delete Victims");
    }

    file.close();
    
    Request->send(200, "text/plain", "Deleted Victims");

  });


  // OS Probes
  Server.on("/generate_204", [](AsyncWebServerRequest* Request) {
    Request->redirect("http://192.168.69.1/");
  });
  Server.on("/hotspot-detect.html", [](AsyncWebServerRequest* Request) {
    Request->redirect("http://192.168.69.1/");
  });


  // Redirect unknown URLs to portal or maybe unknown get requests
  Server.onNotFound([](AsyncWebServerRequest *Request) {
    Request->redirect("http://192.168.69.1/");
  });

  Server.begin(); // Launch Server


}

void loop() {
  DNS.processNextRequest(); // Keep handling DNS requests
  

}























