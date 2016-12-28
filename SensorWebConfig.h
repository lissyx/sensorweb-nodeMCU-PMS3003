#ifndef PM25_NODEMCU_SENSORWEBCONFIG_H
#define PM25_NODEMCU_SENSORWEBCONFIG_H

// Fingerprint:
// echo | openssl s_client -showcerts -host api.bewrosnes.org -port 443 2>/dev/null| openssl x509 -fingerprint -sha1 -noout | cut -d'=' -f 2 | tr ':' ' '

class SensorWebConfig : public JsonConfig
{
  public:
    double coordinates[2] = { 0.0, 0.0 };
    String apiDatastreams;
    String apiObservations;
    String apiFingerprint;

    static SensorWebConfig* getInstance();

    SensorWebConfig();

  private:
    void parseConfig(String conf);
};

SensorWebConfig::SensorWebConfig() {
  String conf = this->readConfigFile(SENSORWEB_CONFIG);
  if (conf.length() > 0) {
    this->parseConfig(conf);
  } else {
    return;
  }
}

SensorWebConfig* SensorWebConfig::getInstance() {
  static SensorWebConfig* _instance;

  if (!_instance) {
    _instance = new SensorWebConfig();
  }

  return _instance;
}

void SensorWebConfig::parseConfig(String conf) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& confRoot = jsonBuffer.parseObject(conf.c_str());

  DEBUG_SERIAL("SensorWebConfig: JSON: " + conf);

  if (!confRoot.success()) {
    DEBUG_SERIAL("SensorWebConfig: Parsing failure");
    return;
  }

  JsonArray& coords = confRoot.get<JsonVariant>("coordinates");
  this->coordinates[0] = coords.get<double>(0);
  this->coordinates[1] = coords.get<double>(1);

  JsonObject& api = confRoot.get<JsonVariant>("api");
  this->apiDatastreams  = api.get<String>("datastreams");
  this->apiObservations = api.get<String>("observations");
  this->apiFingerprint  = api.get<String>("fingerprint");

  return;
}

#endif // PM25_NODEMCU_SENSORWEBCONFIG_H
