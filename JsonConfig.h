#ifndef PM25_NODEMCU_JSONCONFIG_H
#define PM25_NODEMCU_JSONCONFIG_H

class JsonConfig
{
  public:
    JsonConfig();
    String readConfigFile(String file);

  private:
    virtual void parseConfig(String conf);
};

JsonConfig::JsonConfig() { }

String JsonConfig::readConfigFile(String file) {
  SPIFFS.begin();

  if (!SPIFFS.exists(file)) {
    DEBUG_SERIAL("JsonConfig: No wifi config file '" + file + "', aborting.");
    SPIFFS.end();
    return "";
  }

  File fd = SPIFFS.open(file, "r");
  if (!fd) {
    DEBUG_SERIAL("JsonConfig: Unable to read-open: '" + file + "'");
    SPIFFS.end();
    return "";
  }

  String conf = "";
  while(fd.available()) {
      conf += fd.readStringUntil('\n');
  }
  fd.close();

  SPIFFS.end();

  return conf;
}

#endif // PM25_NODEMCU_JSONCONFIG_H
