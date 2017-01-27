#ifndef PM25_NODEMCU_SENSORWEB_H
#define PM25_NODEMCU_SENSORWEB_H

// Will rely on ntpConfig->ntpTZOffset to compute proper UTC time
// e.g. 2017-01-26 at 15:00:00 Paris Time, it is 2017-01-26T15:00:00.000Z == 2017-01-26T15:00:00.000+01:00
String date_ISO8601(time_t date) {
  // 2016-11-18T11:04:15.790Z
  char iso8601[24];
  time_t utcDate = date - (SECS_PER_HOUR * ntpConfig->ntpTZOffset);
  sprintf(iso8601, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", year(utcDate), month(utcDate), day(utcDate), hour(utcDate), minute(utcDate), second(utcDate));
  return String(iso8601);
}

/*
String jsonToString(JsonObject& obj) {
  const unsigned int MAX_JSON = sizeof(char) * 2048;
  char* jsonChar = (char *)malloc(MAX_JSON);
  if (!jsonChar) {
    free(jsonChar);
    serialUdpDebug("Cannot allocate memory");
    return "";
  }

  obj.printTo(jsonChar, MAX_JSON);
  String rv(jsonChar);
  free(jsonChar);

  return rv;
}
*/

String extractJsonField(String field, String httpPayload) {
  DynamicJsonBuffer jsonReceiveBuffer;
  JsonObject& replyRoot = jsonReceiveBuffer.parseObject(httpPayload.c_str());

  if (!replyRoot.success()) {
    serialUdpDebug("JSON: Parsing failure");
    return "";
  }

  if (!replyRoot.containsKey(field)) {
    serialUdpDebug("JSON: No field named " + field + " in reply");
    return "";
  }

  return replyRoot.get<String>(field);
}

String extractIotId(String httpPayload) {
  return extractJsonField("@iot.id", httpPayload);
}

String extractErrno(String httpPayload) {
  return extractJsonField("errno", httpPayload);
}

String extractErrMsg(String httpPayload) {
  return extractJsonField("error", httpPayload);
}

String getDataStream() {
  DynamicJsonBuffer jsonSendBuffer;
  JsonObject& root              = jsonSendBuffer.createObject();

  JsonObject& unitOfMeasurement = root.createNestedObject("unitOfMeasurement");
  unitOfMeasurement["symbol"]     = "μg/m³";
  unitOfMeasurement["name"]       = "PM 2.5 Particulates (ug/m3)";
  unitOfMeasurement["definition"] = "http://unitsofmeasure.org/ucum.html";

  root["observationType"] = "http://www.opengis.net/def/observationType/OGC-OM/2.0/OM_Measurement";
  root["description"]     = "Air quality readings";
  root["name"]            = "air_quality_readings";

  JsonObject& Thing = root.createNestedObject("Thing");
  Thing["description"] = "A SensorWeb Thing";
  Thing["name"]        = "SensorWebThing";
  JsonObject& ThingProperties = Thing.createNestedObject("properties");
  ThingProperties["organization"] = "Mozilla";
  ThingProperties["owner"]        = "Mozilla";
  JsonArray& ThingLocations  = Thing.createNestedArray("Locations");
  JsonObject& MyLocation     = ThingLocations.createNestedObject();
  MyLocation["description"]  = "My Location Description";
  MyLocation["name"]         = "My Location Name";
  MyLocation["encodingType"] = "application/vnd.geo+json";
  JsonObject& Location       = MyLocation.createNestedObject("location");
  Location["type"]           = "Point";
  JsonArray& LocationCoordinates = Location.createNestedArray("coordinates");
  LocationCoordinates.add(SensorWebConfig::getInstance()->coordinates[0], 6);
  LocationCoordinates.add(SensorWebConfig::getInstance()->coordinates[1], 6);

  JsonObject& ObservedProperty = root.createNestedObject("ObservedProperty");
  ObservedProperty["name"]        = "PM 2.5";
  ObservedProperty["description"] = "Particle pollution, also called particulate matter or PM, is a mixture of solids and liquid droplets floating in the air.";
  ObservedProperty["definition"]  = "https://airnow.gov/index.cfm?action=aqibasics.particle";

  JsonObject& Sensor = root.createNestedObject("Sensor");
  Sensor["description"]  = "PM 2.5 sensor";
  Sensor["name"]         = "PM25sensor";
  Sensor["encodingType"] = "application/pdf";
  Sensor["metadata"]     = "http://particle-sensor.com/";

  String jsonPayload; root.printTo(jsonPayload);
  serialUdpDebug("JSON: " + jsonPayload);

  HTTPClient http;
  http.begin(SensorWebConfig::getInstance()->apiDatastreams, SensorWebConfig::getInstance()->apiFingerprint);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int rc = http.POST(jsonPayload);

  serialUdpDebug("HTTP:DS: Code " + String(rc));
  if (rc > 0) {
    if (rc == HTTP_CODE_OK || rc == HTTP_CODE_CREATED) {
      String payload = http.getString();
      serialUdpDebug("HTTP:DS: OK");
      serialUdpDebug("HTTP:DS: REPLY: " + payload);

      String iotId = extractIotId(payload);
      serialUdpDebug("HTTP:DS: @iot.id: " + iotId);
      return iotId;
    }
  }

  return "";
}

String pushObservation(String iotId, String date, unsigned int pm2_5) {
  DynamicJsonBuffer jsonSendBuffer;
  JsonObject& root       = jsonSendBuffer.createObject();
  root["phenomenonTime"] = date;
  root["resultTime"]     = date;
  root["result"]         = String(pm2_5); // Make it a string after API change broke it as an int.

/*
 * We can rely on auto-creation of FeatureOfInterest
 * 
  JsonObject& FeatureOfInterest = root.createNestedObject("FeatureOfInterest");
  FeatureOfInterest["name"]         = "PM2.5 Tours";
  FeatureOfInterest["description"]  = "PM2.5 Sensor in Tours";
  FeatureOfInterest["encodingType"] = "application/vnd.geo+json";
  JsonObject& Feature       = FeatureOfInterest.createNestedObject("feature");
  Feature["type"]           = "Point";
  JsonArray& FeatureCoordinates = Feature.createNestedArray("coordinates");
  FeatureCoordinates.add(SensorWebConfig::getInstance()->coordinates[0], 6);
  FeatureCoordinates.add(SensorWebConfig::getInstance()->coordinates[1], 6);
*/

  JsonObject& Datastream = root.createNestedObject("Datastream");
  Datastream["@iot.id"]  = iotId;

  String jsonPayload; root.printTo(jsonPayload);
  serialUdpDebug("JSON: " + jsonPayload);

  HTTPClient http;
  http.begin(SensorWebConfig::getInstance()->apiObservations, SensorWebConfig::getInstance()->apiFingerprint);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  int rc = http.POST(jsonPayload);

  String payload = http.getString();
  serialUdpDebug("HTTP:OS: REPLY: " + payload);

  // Pushing observation will return successfull 200/201 when it is okay.
  // If IotID is invalid, we get HTTP status 400 and error errno 102
  serialUdpDebug("HTTP:OS: Code " + String(rc));
  if (rc > 0) {
    if (rc == HTTP_CODE_OK || rc == HTTP_CODE_CREATED) {
      BLINK_LED_PUSH_OK;
      serialUdpDebug("HTTP:OS: OK");
      
      String replyIotId = extractIotId(payload);
      serialUdpDebug("HTTP:OS: @iot.id: " + replyIotId);
      return replyIotId;
    }
  } else {
    String _error = extractErrMsg(payload);
    String _errno = extractErrno(payload);
    serialUdpDebug("HTTP:OS: Code " + String(rc) + " with errno=" + _errno + ": '" + _error + "'");
  }
  // TODO: Handle error code, if errno 102 "Invalid association. Datastream with id X not found", then
  // remove iotid file and restart the sensor.
}

String maybeReadIotId() {
  String iotId = "";

  /* try to read it from a file */
  bool mounted = SPIFFS.begin();

  if (!mounted) {
    serialUdpIntDebug("FS: Unable to mount");
  } else {
    // Previous IoTID file exists, let us read it
    if (SPIFFS.exists(IOTID_FILE)) {
      serialUdpIntDebug("IoT-ID: Getting from FS");
      File iotIdFile = SPIFFS.open(IOTID_FILE, "r");
      if (!iotIdFile) {
        serialUdpIntDebug("FS: Unable to read-open: " + String(IOTID_FILE));
      } else {
        iotId = iotIdFile.readStringUntil('\r');
        iotIdFile.close();
        serialUdpIntDebug("FS: Read iotId=" + iotId + " (" + iotId.length() + " bytes) from " + String(IOTID_FILE));
      }
    }

    if (iotId.length() == 0) {
      serialUdpIntDebug("FS: Remove empty " + String(IOTID_FILE));
      SPIFFS.remove(IOTID_FILE);
    }
  }

  SPIFFS.end();
  return iotId;
}

String maybeWriteNewIotId(String iotId) {
  /* try to read it from a file */
  bool mounted = SPIFFS.begin();

  if (!mounted) {
    serialUdpIntDebug("FS: Unable to mount");
  } else {
    int bytesWrote = -1;

    // Previous IoTID file does not exists, let us populate it
    if (!SPIFFS.exists(IOTID_FILE)) {
      serialUdpIntDebug("IoT-ID: Writing '" + iotId + "' from API");
      File iotIdFile = SPIFFS.open(IOTID_FILE, "w");
      if (!iotIdFile) {
        serialUdpIntDebug("FS: Unable to write-open: " + String(IOTID_FILE));
      } else {
        bytesWrote = iotIdFile.println(iotId);
        iotIdFile.close();
        serialUdpIntDebug("FS: Wrote iotId=" + String(bytesWrote) + " to " + String(IOTID_FILE));
      }
    }

    if (bytesWrote == 0) {
      serialUdpIntDebug("FS: Remove empty " + String(IOTID_FILE));
      SPIFFS.remove(IOTID_FILE);
    }
  }

  SPIFFS.end();
  return iotId;
}

String ensureIotId() {
  static String iotId = "";

  /* if the value is there, skip and return it */
  if (iotId.length() > 0) {
    return iotId;
  } else {
    /**
     * We need to split into read/write each one doing a fs mount
     * because doing getDataStream() call while FS is open would
     * end up in weird behavior:
     *  - unable to open() for writing
     *  - writing 0 bytes
     *  - ...
     */
    iotId = maybeReadIotId();
    if (iotId.length() == 0) {
      // We have NOT been able to read from FS
      iotId = getDataStream();
      maybeWriteNewIotId(iotId);
    }
  }

  return iotId;
}

#endif // PM25_NODEMCU_SENSORWEB_H
