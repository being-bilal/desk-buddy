// ---------------------------------------------------------------------------
//  Desk Buddy - entry point.
//
//  setup() brings up the display, storage, touch, audio and the web layer.
//  loop() runs the buddy (touch gestures, animation, audio) and then the web
//  server. buddy.loop() paces its own rendering, so this can spin freely.
// ---------------------------------------------------------------------------
#include <Arduino.h>

#include "buddy.h"
#include "webui.h"

static Buddy buddy;
static WebUI web;

void setup() {
  buddy.begin();
  web.begin(&buddy);

  Serial.printf("[sys] ready - %.1f KB heap free\n", ESP.getFreeHeap() / 1024.0f);
}

void loop() {
  buddy.loop();
  web.loop();
  delay(2);          // let the WiFi/lwIP tasks breathe
}
