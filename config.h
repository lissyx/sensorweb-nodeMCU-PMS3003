#ifndef PM25_NODEMCU_CONFIG_H
#define PM25_NODEMCU_CONFIG_H

// #define ENABLE_LEDS 1

#define VERSION String("1.1")
#define BUILD_DATE String(__DATE__)
#define BUILD_TIME String(__TIME__)

// How often (in secs) should we perform measures
const unsigned int execInterval = 300;

// How long should we let the sensor's up. Give some time
// to its fan to start, and ventilate a bit ...
const unsigned int pmsWarmUp    = 10;

// Force resync every 30 mins, value in number if cycles
const unsigned int forceResync  = (3 * SECS_PER_HOUR) / execInterval;

// What is the limit in sleepWakeCycles to use to compute slow down factor
const unsigned int sleepWakeCyclesSlowDownCompute = 1;

const unsigned int ntpFirstSync = 5;
const unsigned int ntpInterval  = 1800;

// Experimental slowdown factor, as it looks like ESP.deepSleep()
// is not very accurate over time.
// Cf. http://www.esp8266.com/viewtopic.php?f=13&t=6620
//
// Update after running a couple of measures accross time and
// check how long does it takes between each
// Observations POST request ...
//
// e.g., execInterval = 60, but wireshark shows packets every
// ~57.3-57.4s => 60/57.35 =~ 1.0465
// 60÷((58.40+58.36+58.49+58.47+58.46+58.10+58.14+58.11+58.48)÷9) =~ 1.02855
// This can also be approached by measuring time between two
// flashes of the onboard ESP LED: it flashes when wakeup pulse
// is being triggered.
//
// Might be sensitive to temp? At least rtctime module from nodeMCU
// provides a solution to this and states this is due to temperature :(
//
// Experiments running on battery every 60s, forcing NTP re-sync
// every 30 cycles (i.e., 30min, from forceResync above) shows
// good enough accuracy, drifting less than one second between
// each resync ; without, drifting would be ~1min every 10 min.
// const double slowDownFactor     = 1.0549; // for execInterval=60, use 1.0425
double slowDownFactor = 1.0;

#define MIN(x,y) ((x <= y) ? x : y)
#define MAX(x,y) ((x <= y) ? y : x)

#define WIFI_CREDENTIALS  "/wifi.json"
#define NTP_CONFIG        "/ntp.json"
// #define SENSORWEB_CONFIG "/sensorweb.json"
#define AIRCASTING_CONFIG  "/aircasting.json"
#define AIRCASTING_SESSION "/aircasting.sess"

#include "JsonConfig.h"
#include "NtpConfig.h"
// #include "SensorWebConfig.h"
#include "WifiConfig.h"

#endif // PM25_NODEMCU_CONFIG_H
