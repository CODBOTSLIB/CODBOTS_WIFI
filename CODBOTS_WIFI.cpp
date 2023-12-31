#include "CODBOTS_WIFI.h"

// Default constructor
CODBOTS_WIFI::CODBOTS_WIFI() : rom_(nullptr), server_(nullptr)
{
  // Initialize other members of CODBOTS_WIFI here if needed
}

// Constructor that takes a reference to CODBOTS_ROM and a pointer to AsyncWebServer
CODBOTS_WIFI::CODBOTS_WIFI(CODBOTS_ROM &rom, AsyncWebServer &server) : rom_(&rom), server_(&server)
{
  // Initialize other members of CODBOTS_WIFI here if needed
}

/*
  Function: setMemory
  Description: Set memory addresses for Wi-Fi credentials in ROM.
*/
void CODBOTS_WIFI::setMemory(CODBOTS_ROM &rom, int rom_ssid, int rom_password)
{
  rom_ = &rom;
  rom_ssid_ = rom_ssid;
  rom_password_ = rom_password;
  rom_->createSlot(rom_ssid_, 32);
  rom_->createSlot(rom_password_, 32);
}

/*
  Function: setModePin
  Description: Set the mode pin for switching between AP and STA modes.
*/
void CODBOTS_WIFI::setModePin(int modepin, bool pindir)
{
  modepin_ = modepin;
  pindir_ = pindir;
}

/*
  Function: getSignalLevelName
  Description: Get the signal level name based on RSSI.
*/
String CODBOTS_WIFI::getSignalLevelName(int RSSI)
{
  int index = getSignalLevel(RSSI);
  if (index >= 0 && index < 5)
  {
    return signalLevelNames[index];
  }
  return "Unknown";
}

/*
  Function: getSignalLevel
  Description: Get the signal level index based on RSSI.
*/
int CODBOTS_WIFI::getSignalLevel(int RSSI)
{
  for (int i = 5 - 1; i >= 0; i--)
  {
    if (RSSI >= signalLevelRanges[i])
    {
      return i;
    }
  }
  return 0; // Default to "No Signal" if no match is found
}

/*
  Function: getWifiNetworksJSON
  Description: Get a JSON string with available Wi-Fi networks.
*/
String CODBOTS_WIFI::getWifiNetworksJSON()
{
  Serial.println("Reading wifi list...");
  // Create a JSON array
  StaticJsonDocument<1024> jsonDoc;
  JsonArray networks = jsonDoc.createNestedArray("networks");

  int numNetworks = WiFi.scanNetworks();
  Serial.println("Scanned...");
  if (numNetworks == 0)
  {
    // No networks found
    networks.add("No networks found");
  }
  else
  {
    for (int i = 0; i < numNetworks; i++)
    {
      JsonObject network = networks.createNestedObject();
      network["SSID"] = WiFi.SSID(i);
      network["RSSI"] = WiFi.RSSI(i);
      network["Encryption"] = getEncryptionTypeString(WiFi.encryptionType(i));
    }
  }

  // Serialize the JSON to a string
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  Serial.println(jsonString);
  return jsonString;
}

/*
  Function: getEncryptionTypeString
  Description: Get a string representing the encryption type.
*/
String CODBOTS_WIFI::getEncryptionTypeString(int encryptionType)
{
  switch (encryptionType)
  {
  case WIFI_AUTH_OPEN:
    return "Open";
  case WIFI_AUTH_WEP:
    return "WEP";
  case WIFI_AUTH_WPA_PSK:
    return "WPA-PSK";
  case WIFI_AUTH_WPA2_PSK:
    return "WPA2-PSK";
  case WIFI_AUTH_WPA_WPA2_PSK:
    return "WPA/WPA2-PSK";
  default:
    return "Unknown";
  }
}

/*
  Function: readWifiSettings
  Description: Read Wi-Fi settings from ROM.
*/
void CODBOTS_WIFI::readWifiSettings()
{
  sta_ssid = rom_->readValues(rom_ssid_);
  sta_password = rom_->readValues(rom_password_);
}

/*
  Function: connect
  Description: Connect to Wi-Fi (automatically determine mode).
*/
void CODBOTS_WIFI::connect()
{
  if (wifimode == 0)
  {
    pinMode(modepin_, INPUT);
    bool modeswitch = digitalRead(modepin_);
    readWifiSettings();
    if (modeswitch || sta_ssid.isEmpty())
    {
      connect(WIFI_AP);
    }
    else
    {
      connect(WIFI_STA);
    }
  }
}

/*
  Function: getWifiMode
  Description: Get the current Wi-Fi mode (AP or STA).
*/
int CODBOTS_WIFI::getWifiMode()
{
  return wifimode;
}

/*
  Function: connect
  Description: Connect to Wi-Fi with specified mode (AP or STA).
*/
void CODBOTS_WIFI::connect(int mode)
{
  if (mode == WIFI_AP)
  {
    // Start the ESP32 in AP (Access Point) mode
    WiFi.softAP(ap_ssid, ap_password);

    // Optionally, you can configure the IP address of the AP
    IPAddress apIP(192, 168, 1, 1);         // IP address of the AP
    IPAddress subnetMask(255, 255, 255, 0); // Subnet mask

    WiFi.softAPConfig(apIP, apIP, subnetMask);
    apmode = true;
    mode_ap = true;
    Serial.println("Connecting AP");
  }
  else if (mode == WIFI_STA)
  {
    WiFi.begin(sta_ssid, sta_password);
    connectstarttime = millis();
    Serial.println("Connecting STA");
  }
  wifimode = mode;
}

/*
  Function: getConnectStatus
  Description: Get the connection status.
*/
int CODBOTS_WIFI::getConnectStatus()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    wifimode = WIFI_STA;
    mode_sta = true;
  }
  else
  {
    mode_sta = false;
  }
  return WiFi.status();
}

/*
  Function: getConnectStatus
  Description: Get a string representation of the connection status.
*/
String CODBOTS_WIFI::getConnectStatus(int status)
{
  if (status >= 0 && status < 8)
  {
    return wifiStatusStrings[status];
  }
  else
  {
    return "UNKNOWN";
  }
}

/*
  Function: beginServer
  Description: Start the AsyncWebServer.
*/
bool CODBOTS_WIFI::beginServer(AsyncWebServer &server)
{
  if (server_started)
  {
    return server_started;
  }
  server_ = &server;

  // Serve static files
  wifi_list = getWifiNetworksJSON();
  server_->onNotFound([](AsyncWebServerRequest *request)
                      {
    if (request->method() == HTTP_GET) {
      String path = request->url();
     // Serial.println(path);
      if (path.endsWith("/")) {
        path += "index.html";  // Serve index.html by default when a directory is requested
      }
      if (SPIFFS.exists(path)) {
        request->send(SPIFFS, path, String(), false);
      } else {
        request->send(404, "text/plain", "File not found");
      }
    } });

  // Handle requests for the /networks_list endpoint
  server_->on("/networks_list", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "application/json", wifi_list); });

  // Handle requests for the /wifi_connect endpoint
  server_->on("/wifi_connect", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
    String q_ssid;
    String q_password;

    // Get the 'ssid' parameter from the request
    if (request->hasParam("ssid")) {
      q_ssid = request->getParam("ssid")->value();
      rom_->writeSlot(q_ssid, rom_ssid_);
    }

    // Get the 'password' parameter from the request
    if (request->hasParam("password")) {
      q_password = request->getParam("password")->value();
      rom_->writeSlot(q_password, rom_password_);
    }

    readWifiSettings();
    connect(WIFI_STA);

    request->send(200, "application/json", getConnectStatusJSON()); });

  // Handle requests for the /wifi_disconnect endpoint
  server_->on("/wifi_disconnect", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
    WiFi.disconnect();
    request->send(200, "application/json", getConnectStatusJSON()); });

  // Handle requests for the /wifi_status endpoint
  server_->on("/wifi_status", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send(200, "application/json", getConnectStatusJSON()); });

  // Mount SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("An error occurred while mounting SPIFFS");
    return false;
  }
  else
  {
    Serial.println("SPIFFS Started!");
  }

  server_->begin();
  server_started = true;
  Serial.println("Server Started!");
  return server_started;
}

/*
  Function: getConnectStatusJSON
  Description: Get connection status information as a JSON string.
*/
String CODBOTS_WIFI::getConnectStatusJSON()
{
  StaticJsonDocument<1024> jsonDoc;
  JsonObject statusjs = jsonDoc.to<JsonObject>();
  statusjs["AP"] = apmode;
  statusjs["AP_ssid"] = ap_ssid;
  statusjs["AP_password"] = ap_password;
  statusjs["AP_ip"] = "192.168.1.1";

  int status = getConnectStatus();
  statusjs["STA"] = status;
  statusjs["STA_status"] = getConnectStatus(status);
  statusjs["STA_ssid"] = sta_ssid;
  statusjs["STA_password"] = sta_password.length();
  statusjs["STA_time"] = connectstarttime;
  statusjs["STA_ip"] = WiFi.localIP();

  statusjs["pingtime"] = millis();

  String jsonString;
  serializeJson(jsonDoc, jsonString);
  return jsonString;
}

/*
  Function: getIP
  Description: Get the IP address of the ESP32.
*/
String CODBOTS_WIFI::getIP()
{
  return WiFi.localIP().toString();
}