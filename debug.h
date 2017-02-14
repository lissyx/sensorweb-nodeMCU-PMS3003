#ifndef PM25_NODEMCU_DEBUG_H
#define PM25_NODEMCU_DEBUG_H

// #define DEBUG_SERIAL(m) printDebugMessage(m)
// const char* DEBUG_IP_TARGET = "192.168.1.15";
// const int   DEBUG_IP_PORT   = 8899;

#define DEBUG_SERIAL(m)
const char* DEBUG_IP_TARGET = "";
const int   DEBUG_IP_PORT   = 0;

void printDebugMessage(String msg) {
  Serial.println(msg);
}

// Helpers
#define serialUdpDebug(s)    DEBUG_SERIAL(s); dbgMsg(s);
#define serialUdpIntDebug(s) DEBUG_SERIAL(s); intDbgMsg(s);

#define dbgMsg(s)    sendDebugMessage(s, true);
#define intDbgMsg(s) sendDebugMessage(s, false);

double getCurrentExecutionTime() {
  return (((double)millis() - (double)startupMillis) / 1000.0);
}

#define DEBUG_ENABLED (strlen(DEBUG_IP_TARGET) > 0 && DEBUG_IP_PORT > 0)
void sendDebugMessage(String msg, bool withDelay = true) {
  if (!DEBUG_ENABLED) {
    DEBUG_SERIAL("UDP Debug not enabled");
    return;
  }

  // If we have some debug target, let us open plain UDP and spit out
  // debugging should be done client side with:
  // nc -n -k -4 -v -l -u -p 8899 | ts "%Y-%m-%d %H:%M:%S"

  String _msg = WiFi.hostname() + ": " + msg;

  WiFiUDP udp;
  udp.beginPacket(DEBUG_IP_TARGET, DEBUG_IP_PORT);
  if (withDelay) delay(1);
  udp.write(_msg.c_str());
  if (withDelay) delay(1);
  udp.write('\n');
  if (withDelay) delay(1);
  udp.endPacket();
  if (withDelay) delay(1);

  // delay(1) would help sending UDP packets as documented on:
  // https://github.com/esp8266/Arduino/issues/1009#issuecomment-189666095
  //
  // But calling delay()/yield() from interrupt context is not allowed.
  // yield() will panic() on purpose, delay() just crash.
  // Confere calls to cont_can_yield(&g_cont) in cores/esp8266/core_esp8266_main.cpp
  // and implementation in cores/esp8266/cont_util.c

  return;
}

#endif // PM25_NODEMCU_DEBUG_H

