#include "webui.h"
#include "config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <string.h>

static AsyncWebServer s_server(HTTP_PORT);
static AsyncWebSocket s_ws(WS_PATH);
static DNSServer     s_dns;

static Buddy *s_buddy = nullptr;
static bool   s_ap = false;
static char   s_ip[16] = "0.0.0.0";
static uint32_t s_rebootAt = 0;

static void broadcastStateCb();

// ---------------------------------------------------------------------------
//  State serialisation
// ---------------------------------------------------------------------------
static String stateJson() {
  JsonDocument doc;
  doc["t"]         = "state";
  doc["fw"]        = FW_VERSION;
  doc["name"]      = FW_NAME;
  doc["face"]      = faceName(s_buddy->face());
  doc["faceIndex"] = (uint8_t)s_buddy->face();
  doc["faceCount"] = (uint8_t)FACE_COUNT;
  doc["mode"]      = (s_buddy->mode() == SCREEN_TEXT) ? "text" : "face";
  doc["text"]      = s_buddy->text();
  doc["vol"]       = s_buddy->volume();
  doc["track"]     = s_buddy->currentTrack();
  doc["playing"]   = s_buddy->playing();
  doc["audio"]     = s_buddy->audioReady();
  doc["storage"]   = s_buddy->storageReady();
  doc["fps"]       = s_buddy->fps();
  doc["uptime"]    = s_buddy->uptimeMs() / 1000;
  doc["ap"]        = s_ap;
  doc["ip"]        = s_ip;

  String out;
  serializeJson(doc, out);
  return out;
}

static void sendState(AsyncWebSocketClient *client) {
  if (!s_buddy || !client) return;
  client->text(stateJson());
}

static void broadcastStateCb() {
  if (!s_buddy) return;
  s_ws.textAll(stateJson());
}

// ---------------------------------------------------------------------------
//  Incoming commands
// ---------------------------------------------------------------------------
static void handleCommand(const uint8_t *data, size_t len,
                          AsyncWebSocketClient *client) {
  if (!s_buddy) return;

  JsonDocument doc;
  if (deserializeJson(doc, (const char *)data, len)) return;

  const char *t = doc["t"] | "";
  if (!t[0]) return;

  if (!strcmp(t, "face")) {
    if (doc["i"].is<int>()) {
      s_buddy->setFaceIndex((uint8_t)(doc["i"] | 0));
    } else if (doc["v"].is<const char *>()) {
      s_buddy->setFaceByName(doc["v"] | "neutral");
    }
  } else if (!strcmp(t, "next")) {
    s_buddy->nextFace();
  } else if (!strcmp(t, "text")) {
    s_buddy->setText(doc["v"] | "");
  } else if (!strcmp(t, "show")) {
    s_buddy->showTextNow(doc["ms"] | 0);
  } else if (!strcmp(t, "hide")) {
    s_buddy->hideText();
  } else if (!strcmp(t, "play")) {
    s_buddy->playTrack((uint8_t)(doc["n"] | TRACK_INTRO));
  } else if (!strcmp(t, "intro")) {
    s_buddy->playIntro();
  } else if (!strcmp(t, "stop")) {
    s_buddy->stopAudio();
  } else if (!strcmp(t, "vol")) {
    s_buddy->setVolume((uint8_t)(doc["v"] | AUDIO_VOLUME_DEFAULT));
  } else if (!strcmp(t, "get")) {
    sendState(client);
    return;                      // no broadcast needed
  }

  broadcastStateCb();
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[ws] client %u connected\n", client->id());
      sendState(client);
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[ws] client %u gone\n", client->id());
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len &&
          info->opcode == WS_TEXT) {
        handleCommand(data, len, client);
      }
      break;
    }

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
//  Routing
// ---------------------------------------------------------------------------
void WebUI::begin(Buddy *buddy) {
  _buddy = buddy;
  s_buddy = buddy;

  WiFi.persistent(false);
  WiFi.setSleep(false);

  char ssid[33] = {0};
  char pass[65] = {0};
  const bool haveCreds =
      buddy->wifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass));

  if (haveCreds) {
    Serial.printf("[wifi] joining \"%s\" ...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (uint32_t)(millis() - start) < WIFI_CONNECT_MS) {
      delay(200);
      Serial.print('.');
    }
    Serial.println();
  }

  if (haveCreds && WiFi.status() == WL_CONNECTED) {
    _apMode = false;
    strncpy(_ip, WiFi.localIP().toString().c_str(), sizeof(_ip) - 1);
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", HTTP_PORT);
      Serial.printf("[wifi] http://%s.local  (%s)\n", MDNS_HOSTNAME, _ip);
    } else {
      Serial.printf("[wifi] http://%s\n", _ip);
    }
  } else {
    // No credentials, or they stopped working: fall back to the setup AP so
    // the device can always be reached.
    _apMode = true;
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP(AP_SSID);
    s_dns.start(53, "*", WiFi.softAPIP());
    strncpy(_ip, WiFi.softAPIP().toString().c_str(), sizeof(_ip) - 1);
    Serial.printf("[wifi] setup AP \"%s\" -> http://%s\n", AP_SSID, _ip);
  }
  s_ap = _apMode;

  // ------------------------------ WebSocket ------------------------------
  s_ws.onEvent(onWsEvent);
  s_server.addHandler(&s_ws);

  // --------------------------------- REST --------------------------------
  s_server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", stateJson());
  });

  s_server.on("/api/text", HTTP_POST,
              [](AsyncWebServerRequest *r) {
                r->send(200, "application/json", "{\"ok\":true}");
              },
              nullptr,
              [](AsyncWebServerRequest *r, uint8_t *data, size_t len,
                 size_t index, size_t total) {
                static String body;
                if (index == 0) body = "";
                body.reserve(total + 1);
                for (size_t i = 0; i < len; i++) body += (char)data[i];
                if (index + len == total && s_buddy) s_buddy->setText(body.c_str());
              });

  s_server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument doc;
    doc["ap"] = s_ap;
    doc["ip"] = s_ip;
    if (s_buddy) {
      char ssid[33] = {0}, pass[65] = {0};
      if (s_buddy->wifiCredentials(ssid, sizeof(ssid), pass, sizeof(pass)))
        doc["ssid"] = ssid;
    }
    String out;
    serializeJson(doc, out);
    r->send(200, "application/json", out);
  });

  s_server.on("/api/wifi", HTTP_POST,
              [](AsyncWebServerRequest *r) {
                r->send(200, "application/json", "{\"ok\":true}");
              },
              nullptr,
              [](AsyncWebServerRequest *r, uint8_t *data, size_t len,
                 size_t index, size_t total) {
                static String body;
                if (index == 0) body = "";
                body.reserve(total + 1);
                for (size_t i = 0; i < len; i++) body += (char)data[i];
                if (index + len != total || !s_buddy) return;

                JsonDocument doc;
                if (deserializeJson(doc, body)) return;
                s_buddy->saveWifi(doc["ssid"] | "", doc["pass"] | "");
                s_rebootAt = millis() + 900;   // reboot into STA from loop()
              });

  s_server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", "{\"ok\":true}");
    s_rebootAt = millis() + 500;
  });

  // ------------------------------- static --------------------------------
  s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  s_server.onNotFound([](AsyncWebServerRequest *r) {
    // In AP mode this is what makes the captive-portal sheet appear.
    if (s_ap) {
      r->redirect("http://" + WiFi.softAPIP().toString() + "/");
    } else {
      r->send(404, "text/plain", "not found");
    }
  });

  s_server.begin();
  buddy->onStateChanged = broadcastStateCb;
}

// ---------------------------------------------------------------------------
void WebUI::loop() {
  s_ws.cleanupClients();
  if (s_ap) s_dns.processNextRequest();
  if (s_rebootAt && (int32_t)(millis() - s_rebootAt) >= 0) {
    s_rebootAt = 0;
    Serial.println("[sys] rebooting for new WiFi settings");
    delay(50);
    ESP.restart();
  }
}

const char *WebUI::ip() const { return _ip; }
