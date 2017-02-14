#ifndef PM25_NODEMCU_AIRCASTING_H
#define PM25_NODEMCU_AIRCASTING_H

#include "AirCastingConfig.h"
#include "AirCastingSession.h"

#include <ESP8266TrueRandom.h>

#define SERIALIZED_CONTENT(v)     \
  String __serializedContent;     \
  v.printTo(__serializedContent); \
  return __serializedContent

class AirCasting
{
  public:
    AirCasting();
    String getSessionUUID();
    bool push(String date, unsigned int pm2_5);

  private:
    AirCastingConfig* conf;
    AirCastingSession* sess;

    String authenticate();
    String generateUUID();

    String createPayload(String target, String content);
    String makeFixedSession(String uuid);
    String makeMeasurement(String date, unsigned int pm2_5);

    String createFixedSession(String uuid);

    String extractJsonField(String field, String httpPayload);
};

AirCasting::AirCasting() {
  this->conf = new AirCastingConfig();
  this->sess = new AirCastingSession();
}

String AirCasting::extractJsonField(String field, String httpPayload) {
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

String AirCasting::authenticate() {
  HTTPClient http;
  http.begin(this->conf->login);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setAuthorization(this->conf->user.c_str(), this->conf->pass.c_str());
  int rc = http.GET();

  String payload = http.getString();
  serialUdpDebug("AC:Auth: Code " + String(rc));
  if (rc > 0) {
    if (rc == HTTP_CODE_OK) {
      String token = this->extractJsonField("authentication_token", payload);
      serialUdpDebug("AC:Auth: token: " + token);
      return token;
    }
  }
}

String AirCasting::createPayload(String target, String content) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  content.replace('"', '\"');
  root[target] = content;
  String jsonPayload; root.printTo(jsonPayload);
  // DEBUG_SERIAL("AirCasting::createPayload: jsonPayload=" + jsonPayload);
  return jsonPayload;
}

String AirCasting::makeFixedSession(String uuid) {
  DynamicJsonBuffer jsonBuffer;

  JsonObject& session   = jsonBuffer.createObject();
  session["uuid"]       = uuid;
  session["streams"]    = session.createNestedObject("streams");
  session["notes"]      = session.createNestedObject("notes");
  session["title"]      = getSessionTitle();
  session["tag_list"]   = getSessionTags();
  session["descrition"] = getSessionDescription();
  session["calibration"]  = 0;
  session["contribute"]   = false;
  session["os_version"]   = "ESP8266 0.1";
  session["phone_model"]  = "NodeMCU-1.0";
  session["offset_60_db"] = 0,
  session["location"]     = this->conf->location;
  session["deleted"]      = false;
  session["start_time"]   = date_ISO8601(now());
  session["end_time"]     = date_ISO8601(now() + 60);
  session["type"]         = "FixedSession";
  session["is_indoor"]    = false;
  session["latitude"]     = this->conf->coordinates[0];
  session["longitude"]    = this->conf->coordinates[1];

  SERIALIZED_CONTENT(session);
}

String AirCasting::generateUUID() {
  byte uuidNumber[16];
  String uuidStr = ESP8266TrueRandom.uuidToString(uuidNumber);
  return uuidStr;
}

String AirCasting::createFixedSession(String uuid) {
  HTTPClient http;
  http.begin(this->conf->session);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setAuthorization(this->sess->token.c_str(), "X");

  int rc = http.POST(this->createPayload("session", this->makeFixedSession(uuid)));

  String payload = http.getString();
  serialUdpDebug("AC:FixedSession: Code " + String(rc));
  if (rc > 0) {
    if (rc == HTTP_CODE_OK) {
      serialUdpDebug("AC:FixedSession: UUID: " + uuid);
      return uuid;
    }
  }

  return "";
}

String AirCasting::getSessionUUID() {
  /* if the value is there, skip and return it */
  if (this->sess->uuid.length() > 0) {
    return this->sess->uuid;
  } else {
    /**
     * We need to split into read/write each one doing a fs mount
     * because doing getDataStream() call while FS is open would
     * end up in weird behavior:
     *  - unable to open() for writing
     *  - writing 0 bytes
     *  - ...
     */
    if (this->sess->token.length() == 0 || this->sess->uuid.length() == 0) {
      String uuid = this->generateUUID();
      this->sess->token = this->authenticate();
      this->sess->uuid  = this->createFixedSession(uuid);
      if (this->sess->writeToFile()) {
        return this->sess->uuid;
      }
    }
  }

  return "";
}

String AirCasting::makeMeasurement(String date, unsigned int pm2_5) {
  DynamicJsonBuffer jsonBuffer;

  JsonObject& data = jsonBuffer.createObject();
  data["session_uuid"]           = this->sess->uuid;
  data["sensor_package_name"]    = getSensorPackageName();
  data["sensor_name"]            = getSensorName();
  data["measurement_type"]       = getMeasureType();
  data["measurement_short_type"] = getMeasureShortType();
  data["unit_symbol"]            = getUnitSymbol();
  data["unit_name"]              = getUnitName();
  data["threshold_very_low"]     = 0;
  data["threshold_low"]          = 25;
  data["threshold_medium"]       = 50;
  data["threshold_high"]         = 75;
  data["threshold_very_high"]    = 100;

  JsonArray& measurements = data.createNestedArray("measurements");
  JsonObject& measure     = measurements.createNestedObject();
  measure["latitude"]        = this->conf->coordinates[0];
  measure["longitude"]       = this->conf->coordinates[1];
  measure["time"]            = date;
  measure["timezone_offset"] = 0;
  measure["milliseconds"]    = 0;
  measure["value"]           = pm2_5;
  measure["measured_value"]  = pm2_5;

  SERIALIZED_CONTENT(data);
}

bool AirCasting::push(String date, unsigned int pm2_5) {
  HTTPClient http;
  http.begin(this->conf->data);
  http.addHeader("Cache-Control", "no-cache");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.setAuthorization(this->sess->token.c_str(), "X");

  int rc = http.POST(this->createPayload("data", this->makeMeasurement(date, pm2_5)));

  String payload = http.getString();
  serialUdpDebug("AC:push: Code " + String(rc));
  if (rc > 0) {
    if (rc == HTTP_CODE_OK) {
      return true;
    }
  }

  return false;
}

#endif // PM25_NODEMCU_AIRCASTING_H
