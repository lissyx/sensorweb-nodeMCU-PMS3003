#ifndef PM25_NODEMCU_AIRCASTINGCONFIG_H
#define PM25_NODEMCU_AIRCASTINGCONFIG_H

class AirCastingConfig : public JsonConfig
{
  public:
    double coordinates[2] = { 0.0, 0.0 };
    String location;

    // API
    String login;
    String session;
    String data;

    // Credentials
    String user;
    String pass;

    static AirCastingConfig* getInstance();

    AirCastingConfig();

  private:
    void parseConfig(String conf);
};

AirCastingConfig::AirCastingConfig() {
  String conf = this->readConfigFile(AIRCASTING_CONFIG);
  if (conf.length() > 0) {
    this->parseConfig(conf);
  } else {
    return;
  }
}

AirCastingConfig* AirCastingConfig::getInstance() {
  static AirCastingConfig* _instance;

  if (!_instance) {
    _instance = new AirCastingConfig();
  }

  return _instance;
}

void AirCastingConfig::parseConfig(String conf) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& confRoot = jsonBuffer.parseObject(conf.c_str());

  // DEBUG_SERIAL("AirCastingConfig: JSON: " + conf);

  if (!confRoot.success()) {
    DEBUG_SERIAL("AirCastingConfig: Parsing failure");
    return;
  }

  this->location = confRoot.get<String>("location");

  JsonArray& coords    = confRoot.get<JsonVariant>("coordinates");
  this->coordinates[0] = coords.get<double>(0);
  this->coordinates[1] = coords.get<double>(1);

  JsonObject& api = confRoot.get<JsonVariant>("api");
  this->login     = api.get<String>("login");
  this->session   = api.get<String>("session");
  this->data      = api.get<String>("data");

  JsonObject& creds = confRoot.get<JsonVariant>("credentials");
  this->user        = creds.get<String>("user");
  this->pass        = creds.get<String>("pass");

  return;
}

#endif // PM25_NODEMCU_AIRCASTINGCONFIG_H
