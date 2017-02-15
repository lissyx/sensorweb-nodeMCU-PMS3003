#ifndef PM25_NODEMCU_NTPCONFIG_H
#define PM25_NODEMCU_NTPCONFIG_H

class NtpConfig : public JsonConfig
{
  public:
    String ntpServer;
    double ntpTZOffset;
    bool   ntpDayLight;

    static NtpConfig* getInstance();

    NtpConfig();

  private:
    void parseConfig(String conf);
};

NtpConfig::NtpConfig() {
  String conf = this->readConfigFile(NTP_CONFIG);
  if (conf.length() > 0) {
    this->parseConfig(conf);
  } else {
    return;
  }
}


NtpConfig* NtpConfig::getInstance() {
  static NtpConfig* _instance;

  if (!_instance) {
    _instance = new NtpConfig();
  }

  return _instance;
}

void NtpConfig::parseConfig(String conf) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& confRoot = jsonBuffer.parseObject(conf.c_str());

  DEBUG_SERIAL("NtpConfig: JSON: " + conf);
  if (!confRoot.success()) {
    DEBUG_SERIAL("NtpConfig: Parsing failure");
    return;
  }

  this->ntpServer   = confRoot.get<String>("server");
  this->ntpTZOffset = confRoot.get<double>("tzoffset");
  this->ntpDayLight = confRoot.get<bool>("daylight");

  return;
}

#endif // PM25_NODEMCU_NTPCONFIG_H
