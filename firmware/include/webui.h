// ---------------------------------------------------------------------------
//  WebUI - the device-hosted control panel (Page B).
//  Serves the LittleFS single-page app, a WebSocket for live commands, a small
//  REST surface, and a captive portal for first-time WiFi setup.
// ---------------------------------------------------------------------------
#pragma once

#include <stdint.h>

#include "buddy.h"

class WebUI {
public:
  void begin(Buddy *buddy);
  void loop();

  void        broadcastState();
  bool        apMode() const { return _apMode; }
  const char *ip() const;

private:
  Buddy *_buddy = nullptr;
  bool   _apMode = false;
  char   _ip[16] = "0.0.0.0";
};
