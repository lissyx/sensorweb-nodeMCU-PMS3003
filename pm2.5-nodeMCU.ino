#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>
#include <FS.h>
#include <NtpClientLib.h>
#include <WiFiUdp.h>

uint32 pm1   = UINT32_MAX;
uint32 pm2_5 = UINT32_MAX;
uint32 pm10  = UINT32_MAX;

unsigned int sleepWakeCycles = 0;

#include "debug.h"
#include "config.h"
#include "LEDs.h"
#include "PMS3003.h"
#include "SensorWeb.h"
#include "RTC.h"

bool ntpInitialSync = false;
unsigned int ntpErrors = 0;
unsigned long startupMillis = 0;

void ntpSyncEventHandler(NTPSyncEvent_t error) {
  if (error) {
    ntpErrors += 1;
    if (error == noResponse) {
      serialUdpDebug("NTPSyncEvent: server not reachable");
    } else if (error == invalidAddress) {
      serialUdpDebug("NTPSyncEvent: invalid server address");
    } else {
      serialUdpDebug("NTPSyncEvent: " + String(error));
    }
  } else {
    ntpErrors = 0;
    serialUdpDebug("NTPSyncEvent: " + NTP.getTimeDateString(NTP.getLastNTPSync()));
    if (!ntpInitialSync) {
      serialUdpDebug("NTPSyncEvent: set ntpInitialSync");
      ntpInitialSync = true;
    }
  }
}

void wifiHasIpAddress(WiFiEventStationModeGotIP evt) {
  serialUdpIntDebug("Got IP: " + evt.ip.toString());

  NtpConfig ntpConfig;
  NTP.begin(ntpConfig.ntpServer, ntpConfig.ntpTZOffset, ntpConfig.ntpDayLight);
  serialUdpIntDebug("NTP: NTP.begin(" + ntpConfig.ntpServer + ", " + ntpConfig.ntpTZOffset + ", " + ntpConfig.ntpDayLight + ")");

  NTP.onNTPSyncEvent(ntpSyncEventHandler);
  NTP.setInterval(ntpFirstSync, ntpInterval);
}

void wifiConnected(WiFiEventStationModeConnected evt) {
  serialUdpIntDebug("Connected to SSID: " + evt.ssid);
}

void wifiDisconnected(WiFiEventStationModeDisconnected evt) {
  DEBUG_SERIAL("Disconnected from SSID: " + evt.ssid);
  DEBUG_SERIAL("Reason: " + evt.reason);
  NTP.stop();
}

void setup() {
  static WiFiEventHandler connectEvent, hasIpEvent, disconnectEvent;

  startupMillis = millis();

  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);

  // Measure how much power we can save there ...
  // WiFi.setPhyMode(WIFI_PHY_MODE_11G);

  const rst_info * resetInfo = system_get_rst_info();
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    if (readTimeFromRTC()) {
      if (sleepWakeCycles < forceResync) {
        ntpInitialSync  = true;
      } else {
        sleepWakeCycles = 0;
      }
    }
  }

  // Reconfigure watchdog at least twice expected secs.
  ESP.wdtEnable(30 * 1000);

  PM_SERIAL.begin(9600);
  DEBUG_SERIAL("Booting ...");

  configure_leds();
  configure_pms3003_pin();

  // put PMS3003 into standby for now
  power_pms3003(false);

  hasIpEvent      = WiFi.onStationModeGotIP(wifiHasIpAddress);
  connectEvent    = WiFi.onStationModeConnected(wifiConnected);
  disconnectEvent = WiFi.onStationModeDisconnected(wifiDisconnected);

  ESP8266WiFiMulti WiFiMulti;
  WifiConfig wifiConfig;
  for(int itCreds = 0 ; itCreds < wifiConfig.creds.size(); itCreds++ ) {
    WifiCreds c = wifiConfig.creds[itCreds];
    if (c.pass().length() > 0) {
      WiFiMulti.addAP(c.ssid().c_str(), c.pass().c_str());
      DEBUG_SERIAL("WifiConfig: Adding WiFi for protected " + c.ssid());
    } else {
      WiFiMulti.addAP(c.ssid().c_str());
      DEBUG_SERIAL("WifiConfig: Adding WiFi for open " + c.ssid());
    }
  }

  BLINK_LED_BOOT;
}

void loop() {
  // Wait for NTP sync at least 3*45 secs
  // If we get nothing, go to sleep. Let's hope it is a transient issue.
  String timeNow = date_ISO8601(now());
  if (!ntpInitialSync && (ntpErrors < 3)) {
    serialUdpDebug("Loop: no NTP initial sync, waiting ... ntpErrors=" + String(ntpErrors));
    delay(1250);
    return;
  }

  // Do not try to collect or send data if we have not received any NTP sync event.
  if (ntpErrors < 3) {
    String iotId = ensureIotId();

    serialUdpDebug("IoT-ID: " + iotId);
    serialUdpDebug("NTP: " + timeNow);

    // Read values from the PMS3003 connected sensor
    collect_pms3003_sensor();

    // Send even if value is 0 ; it just means sensor detected nothing.
    if(pm2_5 < UINT32_MAX && iotId.length() > 0) {
      String sent = pushObservation(iotId, timeNow, pm2_5);
      serialUdpDebug("NTP: " + timeNow + " PM2.5: " + String(pm2_5) + " @iot.id:" + iotId + " sent:" + sent);
    }
  } else {
    serialUdpDebug("Loop: too many NTP errors. Aborting.");
  }

  double executionTime = ((double)millis() - (double)startupMillis) / 1000.0;
  double nextInterval  = ((double)execInterval - (double)executionTime) * 1.0;

  // Ensure that we have something within range of [1; execInterval]
  nextInterval = MAX(1, MIN(nextInterval, execInterval));

  serialUdpDebug("Loop: deepSleep: nextInterval=" + String(nextInterval) + "; executionTime=" + String(executionTime) + "; deepSleep(" + String(nextInterval * slowDownFactor) + ")");

  // Write time to RTC only if we could get some valid.
  // e.g., no ntp works during first boot of the system
  // then goes to sleep writing time to RTC
  // When it wakes up it would restore time as if it was good
  // => you end up in the 1970s
  if (ntpInitialSync) {
    writeTimeToRTC(now(), nextInterval);
  }

  // with WAKE_RF_DEFAULT, deep sleep current ~8.2mA
  // with WAKE_NO_RFCALL, deep sleep current ~8.2mA
  // with WAKE_RF_DISABLED, no wifi on return from sleep, and deep sleep current ~8.2mA
  ESP.deepSleep(nextInterval * slowDownFactor * 1e6, WAKE_NO_RFCAL);

  return;
}
