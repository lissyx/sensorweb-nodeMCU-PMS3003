#ifndef PM25_NODEMCU_AIRCASTINGSESSION_H
#define PM25_NODEMCU_AIRCASTINGSESSION_H

class AirCastingSession : public JsonConfig
{
  public:
    String token;
    String uuid;

    static AirCastingSession* getInstance();

    AirCastingSession();
    bool writeToFile();
    bool removeFile();

  private:
    void parseConfig(String conf);
};

AirCastingSession::AirCastingSession() {
  String conf = this->readConfigFile(AIRCASTING_SESSION);
  if (conf.length() > 0) {
    this->parseConfig(conf);
  } else {
    return;
  }
}

AirCastingSession* AirCastingSession::getInstance() {
  static AirCastingSession* _instance;

  if (!_instance) {
    _instance = new AirCastingSession();
  }

  return _instance;
}

void AirCastingSession::parseConfig(String conf) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& confRoot = jsonBuffer.parseObject(conf.c_str());

  // DEBUG_SERIAL("AirCastingSession: JSON: " + conf);

  if (!confRoot.success()) {
    DEBUG_SERIAL("AirCastingSession: Parsing failure");
    return;
  }

  this->token = confRoot.get<String>("token");
  this->uuid  = confRoot.get<String>("uuid");

  return;
}

bool AirCastingSession::writeToFile() {
  // Ensure that we got something.
  if (this->token.length() == 0 || this->uuid.length() == 0) {
    return false;
  }

  DynamicJsonBuffer jsonSendBuffer;
  JsonObject& root = jsonSendBuffer.createObject();
  root["token"] = this->token;
  root["uuid"]  = this->uuid;

  String jsonPayload; root.printTo(jsonPayload);
  serialUdpDebug("AirCastingSession: WriteJSON: " + jsonPayload);

  return safelyWriteNewFile(AIRCASTING_SESSION, jsonPayload);
}

bool AirCastingSession::removeFile() {
  return safelyRemoveFile(AIRCASTING_SESSION);
}

#endif // PM25_NODEMCU_AIRCASTINGSESSION_H
