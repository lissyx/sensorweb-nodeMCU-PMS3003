#ifndef PM25_NODEMCU_RTC_H
#define PM25_NODEMCU_RTC_H

// Address in RTC RAM where we save the time
#define RTC_TIME_ADDR 0

// Structure for saving/restoring time
// We save the time when we went to sleep and for how long
// So at startup, we just need to setTime(timeAtSleep) and
// then adjustTime(timeSleeping)
struct sysTime {
  uint32_t     crc32;
  time_t       timeAtSleep;  // Store the current time_t when we went to sleep
  double       timeSleeping; // Store the amount of time (secs) we went to sleep
  unsigned int cycles;         // Counter of wake/sleep cycles
  double       slowDownFactor; // How much does time slows down while asleep
};

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

// writeTimeToRTC should be the very last call just before going into deepSleep()
void writeTimeToRTC(time_t currentTime, double sleepTime) {
  struct sysTime saver;
  saver.timeAtSleep    = currentTime;
  saver.timeSleeping   = sleepTime;
  saver.cycles         = sleepWakeCycles;
  saver.slowDownFactor = slowDownFactor;
  saver.crc32 = calculateCRC32(((uint8_t*) &saver) + 4, sizeof(saver) - 4);

  DEBUG_SERIAL("RTC: writing: timeAtSleep=" + String(saver.timeAtSleep));
  if (!ESP.rtcUserMemoryWrite(RTC_TIME_ADDR, (uint32_t*) &saver, sizeof(saver))) {
    DEBUG_SERIAL("RTC: Unable to write data");
    return;
  }
}

bool readTimeFromRTC() {
  struct sysTime reader;

  if (!ESP.rtcUserMemoryRead(RTC_TIME_ADDR, (uint32_t*) &reader, sizeof(reader))) {
    DEBUG_SERIAL("RTC: Unable to write data");
    return false;
  }

  uint32_t crc = calculateCRC32(((uint8_t*) &reader) + 4, sizeof(reader) - 4);
  if (crc != reader.crc32) {
    DEBUG_SERIAL("RTC: Invalid time CRC");
    return false;
  }

  
  unsigned long wakeUpTime = millis();

  /* Add extra time to account for wakeup time of the device
   * (experimental value, estimated from drift of seconds on a serie
   * of measures, and checking how long it takes for power up LED
   * to blink and system to boot).
   *
   * So we write current time at date T, ask for a wakeup in T+D
   * hence, there will be interrupt signal on RST pin at T+D, but
   * it will take a few extra time to actually reach the setup() call
   * which will restore time.
   *
   * So the time we need to set is:
   *    time at sleep          (T)
   *  + time spent sleeping    (D)
   *  + time to wake up device (w)
   */

  double adjustement = (wakeUpTime + bootTime) / 1000.0;
  double correctTime = reader.timeAtSleep + reader.timeSleeping + adjustement;
  setTime((time_t)round(correctTime));
  sleepWakeCycles = (reader.cycles) + 1;
  rtcSystemTime   = round(correctTime);
  slowDownFactor  = reader.slowDownFactor;

  DEBUG_SERIAL("RTC: setting correctTime=" + String(correctTime) + " and sleepWakeCycles=" + sleepWakeCycles);
  return true;
}

#endif // PM25_NODEMCU_RTC_H
