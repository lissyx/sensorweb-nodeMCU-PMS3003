#ifndef PM25_NODEMCU_NTPCONFIG_H
#define PM25_NODEMCU_NTPCONFIG_H

class NtpConfig : public JsonConfig
{
  public:
    String ntpServer;
    int    ntpTZOffset;
    bool   ntpDayLight;
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

void NtpConfig::parseConfig(String conf) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& confRoot = jsonBuffer.parseObject(conf.c_str());

  DEBUG_SERIAL("NtpConfig: JSON: " + conf);
  if (!confRoot.success()) {
    DEBUG_SERIAL("NtpConfig: Parsing failure");
    return;
  }

  this->ntpServer   = confRoot.get<String>("server");
  this->ntpTZOffset = confRoot.get<int>("tzoffset");
  this->ntpDayLight = confRoot.get<bool>("daylight");

  return;
}

#endif // PM25_NODEMCU_NTPCONFIG_H
