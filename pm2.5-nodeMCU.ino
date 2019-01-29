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
unsigned long rtcSystemTime  = 0;
unsigned long startupMillis = 0;

#include "debug.h"
#include "config.h"
#include "LEDs.h"
#include "PMS3003.h"
// #include "SensorWeb.h"

// used by date_ISO8601() in utils.h
#include "utils.h"

#include "AirCasting.h"
#include "RTC.h"

bool ntpInitialSync = false;
bool readyForNTP = false;
bool ntpRunning = false;
unsigned int ntpErrors = 0;
double ntpStartTime = 0;
double ntpWaitTime  = 0;
ESP8266WiFiMulti* WiFiMulti;

void ntpSyncEventHandler(NTPSyncEvent_t error) {
  if (!ntpRunning) {
    serialUdpDebug("NTPSyncEvent: Stale event");
    return;
  }

  ntpRunning = false;

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
      // serialUdpDebug("NTPSyncEvent: set ntpInitialSync");
      ntpInitialSync = true;
    }

    ntpWaitTime = millis() - ntpStartTime;

    // If we receive a ntpSyncEvent for sleepWakeCycles=1 then we
    // are there to compute slowdown factor.
    if (sleepWakeCycles == sleepWakeCyclesSlowDownCompute) {
      double actualSystemTime   = NTP.getLastNTPSync();
      double actualRuntime      = getCurrentExecutionTime();
      double expectedSystemTime = rtcSystemTime + actualRuntime;
      double deltaTime = expectedSystemTime - actualSystemTime + (bootTime / 1000.0) + (ntpWaitTime / 1000.0);
      slowDownFactor = 1.0 + (deltaTime / execInterval);

      serialUdpDebug("NTPSyncEvent: actualRuntime=" + String(actualRuntime) + " -- bootTime=" + String(bootTime));
      serialUdpDebug("NTPSyncEvent: actualSystemTime=" + date_ISO8601(actualSystemTime) + " -- expectedSystemTime=" + date_ISO8601(expectedSystemTime));
      serialUdpDebug("NTPSyncEvent: deltaTime=" + String(deltaTime, 5) + " => slowDownFactor=" + String(slowDownFactor, 5));
    }
  }
}

void startNtp() {
  if (!ntpRunning) {
    ntpRunning = true;
    NtpConfig* ntpConfig = NtpConfig::getInstance();
    NTP.begin(ntpConfig->ntpServer, ntpConfig->ntpTZOffset, ntpConfig->ntpDayLight);
    serialUdpIntDebug("NTP: NTP.begin(" + ntpConfig->ntpServer + ", " + ntpConfig->ntpTZOffset + ", " + ntpConfig->ntpDayLight + ")");
    ntpStartTime = millis();
    NTP.setInterval(ntpFirstSync, ntpInterval);
  } else {
    serialUdpIntDebug("NTP: not starting because ntpRunning=true");
  }
}

void stopNtp() {
  ntpRunning = false;
  NTP.stop();
}

void wifiHasIpAddress(WiFiEventStationModeGotIP evt) {
  serialUdpIntDebug("UP: " + VERSION + ":" + (BUILD_DATE + " " + BUILD_TIME) + "@" + evt.ip.toString());

  // Only schedule NTP sync when we do need it.
  if (sleepWakeCycles <= sleepWakeCyclesSlowDownCompute) {
    readyForNTP = true;
  }
}

void wifiConnected(WiFiEventStationModeConnected evt) {
  serialUdpIntDebug("Connected to SSID: " + evt.ssid);
}

void wifiDisconnected(WiFiEventStationModeDisconnected evt) {
  DEBUG_SERIAL("Disconnected from SSID: " + evt.ssid + "; reason: " + evt.reason);
  DEBUG_SERIAL("Reason: " + evt.reason);

  readyForNTP = false;

  if (sleepWakeCycles <= sleepWakeCyclesSlowDownCompute) {
    stopNtp();
  }
}

void setup() {
  static WiFiEventHandler connectEvent, hasIpEvent, disconnectEvent;

  startupMillis = millis();

  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);

  // Measure how much power we can save there ...
  // According to ESP docs:
  // 802.11b: TX ~170mA, RX ~50mA
  // 802.11g: TX ~140mA, RX ~56mA
  // 802.11n: TX ~120mA, RX ~56mA
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);

  const rst_info * resetInfo = system_get_rst_info();
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) {
    if (readTimeFromRTC()) {
      // If we wake up from deep sleep AND sleepWakeCycles is one
      // it means we did a first NTP sync. Let's do a second one.
      if (sleepWakeCycles < forceResync) {
        if (sleepWakeCycles > sleepWakeCyclesSlowDownCompute) {
          ntpInitialSync  = true;
        }
      } else {
        sleepWakeCycles = 0;
        slowDownFactor  = 1.0;
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

  ntpRunning = false;
  NTP.onNTPSyncEvent(ntpSyncEventHandler);

  WiFiMulti = new ESP8266WiFiMulti();
  WifiConfig wifiConfig;
  for(int itCreds = 0 ; itCreds < wifiConfig.creds.size(); itCreds++ ) {
    WifiCreds c = wifiConfig.creds[itCreds];
    if (c.pass().length() > 0) {
      WiFiMulti->addAP(c.ssid().c_str(), c.pass().c_str());
      DEBUG_SERIAL("WifiConfig: Adding WiFi for protected " + c.ssid());
    } else {
      WiFiMulti->addAP(c.ssid().c_str());
      DEBUG_SERIAL("WifiConfig: Adding WiFi for open " + c.ssid());
    }
  }

  BLINK_LED_BOOT;
}

void loop() {
  // Let's take a few (extra) seconds and make sure brand new devices can connect.
  // Doing that always will cripple boot time:
  //  - with WiFiMulti->run() checking, we get IP ~7 secs after boot
  //  - without, we get IP ~2.9 secs after boot
  if (sleepWakeCycles == 0) {
    if (WiFiMulti->run() != WL_CONNECTED) {
      DEBUG_SERIAL("WifiConfig: not connected, waiting ...");
      delay(500);
      return;
    }
  }

  if (readyForNTP) {
    startNtp();
  }

  // Wait for NTP sync at least 3*45 secs
  // If we get nothing, go to sleep. Let's hope it is a transient issue.
  String timeNow = date_ISO8601(now());
  if (!ntpInitialSync && (ntpErrors < 3)) {
    serialUdpDebug("Loop: no NTP initial sync, waiting ... sleepWakeCycles=" + String(sleepWakeCycles) + " ntpErrors=" + String(ntpErrors));
    delay(1250);
    return;
  }

  // Do not try to collect or send data if we have not received any NTP sync event.
  if (ntpErrors < 3) {
    AirCasting ac;
    String sessionUUID = ac.getSessionUUID();

    serialUdpDebug("SessionUUID: " + sessionUUID);
    serialUdpDebug("NTP: " + timeNow);

    // Read values from the PMS3003 connected sensor
    collect_pms3003_sensor();

    // Send even if value is 0 ; it just means sensor detected nothing.
    if(pm2_5 < UINT32_MAX && sessionUUID.length() > 0) {
      bool sent = ac.push(timeNow, pm2_5);
      serialUdpDebug("NTP: " + timeNow + " PM2.5: " + String(pm2_5) + " UUID:" + sessionUUID + " sent:" + sent);
    }
  } else {
    serialUdpDebug("Loop: too many NTP errors. Aborting.");
  }

  double executionTime = getCurrentExecutionTime();
  double nextInterval  = ((double)execInterval - (double)executionTime) * 1.0;

  // Ensure that we have something within range of [1; execInterval]
  nextInterval = MAX(1, MIN(nextInterval, execInterval));

  serialUdpDebug("Loop: deepSleep: nextInterval=" + String(nextInterval) +
                 "; executionTime=" + String(executionTime) +
                 " ; slowDownFactor=" + String(slowDownFactor, 5) +
                 "; deepSleep(" + String(nextInterval * slowDownFactor) + ")");

  // Write time to RTC only if we could get some valid.
  // e.g., no ntp works during first boot of the system
  // then goes to sleep writing time to RTC
  // When it wakes up it would restore time as if it was good
  // => you end up in the 1970s
  if (ntpInitialSync) {
    writeTimeToRTC(now(), nextInterval);
  }

  // with WAKE_RF_DEFAULT, deep sleep current ~8.2mA
  // with WAKE_NO_RFCAL, deep sleep current ~8.2mA
  // with WAKE_RF_DISABLED, no wifi on return from sleep, and deep sleep current ~8.2mA
  ESP.deepSleep(nextInterval * slowDownFactor * 1e6, WAKE_NO_RFCAL);

  return;
}
