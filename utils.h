#ifndef PM25_NODEMCU_UTILS_H
#define PM25_NODEMCU_UTILS_H

// Will rely on ntpConfig->ntpTZOffset to compute proper UTC time
// e.g. 2017-01-26 at 15:00:00 Paris Time, it is 2017-01-26T15:00:00.000Z == 2017-01-26T15:00:00.000+01:00
String date_ISO8601(time_t date) {
  // 2016-11-18T11:04:15.790Z
  char iso8601[24];
  time_t utcDate = date - (SECS_PER_HOUR * ntpConfig->ntpTZOffset);
  sprintf(iso8601, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", year(utcDate), month(utcDate), day(utcDate), hour(utcDate), minute(utcDate), second(utcDate));
  return String(iso8601);
}

bool safelyWriteNewFile(String fileName, String fileContent) {
  /* try to read it from a file */
  bool mounted = SPIFFS.begin();
  int bytesWrote = -1;

  if (!mounted) {
    serialUdpIntDebug("FS: Unable to mount");
  } else {
    // Previous file does not exists, let us populate it
    if (!SPIFFS.exists(fileName)) {
      File file = SPIFFS.open(fileName, "w");
      if (!file) {
        serialUdpIntDebug("FS: Unable to write-open: " + String(fileName));
      } else {
        bytesWrote = file.println(fileContent);
        file.close();
        serialUdpIntDebug("FS: Wrote " + String(bytesWrote) + " to " + String(fileName));
      }
    }

    if (bytesWrote == 0) {
      serialUdpIntDebug("FS: Remove empty " + String(fileName));
      SPIFFS.remove(fileName);
    }
  }

  SPIFFS.end();
  return (bytesWrote > 0);
}

bool safelyRemoveFile(String fileName) {
  bool mounted = SPIFFS.begin();
  bool rv = false;

  if (!mounted) {
    serialUdpIntDebug("FS: Unable to mount");
  } else {
    serialUdpIntDebug("FS: Remove empty " + String(fileName));
    rv = SPIFFS.remove(fileName);
  }

  SPIFFS.end();
  return rv;
}

String getSensorName() {
  return "NodeMCU-PMS3003";
}

String getSensorPackageName() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return getSensorName() + ":" + mac;
}

String getMeasureType() {
  return "Particulate Matter";
}

String getMeasureShortType() {
  return "PM";
}

String getUnitSymbol() {
  return "µg/m³";
}

String getUnitName() {
  return "micrograms per cubic meter";
}

String getSessionDescription() {
  return "PM2.5 autonomous Sensor, based on PMS3003";
}

String getSessionTags() {
  return "pm2.5";
}

String getSessionTitle() {
  return "PMS3003-PM2.5";
}

#endif // PM25_NODEMCU_UTILS_H
