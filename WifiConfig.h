#ifndef PM25_NODEMCU_WIFICONFIG_H
#define PM25_NODEMCU_WIFICONFIG_H

#include <vector>

class WifiCreds
{
  public:
    String ssid();
    String pass();
    WifiCreds(String ssid, String pass);

  private:
    String NetworkSSID;
    String NetworkPASS;
};

String WifiCreds::ssid() {
  return NetworkSSID;
}

String WifiCreds::pass() {
  return NetworkPASS;
}

WifiCreds::WifiCreds(String ssid, String pass) {
  this->NetworkSSID = ssid;
  this->NetworkPASS = pass;
}

class WifiConfig : public JsonConfig
{
  public:
    std::vector<WifiCreds> creds;
    WifiConfig();

  private:
    void parseConfig(String conf);
};

WifiConfig::WifiConfig() {
  String conf = this->readConfigFile(WIFI_CREDENTIALS);
  if (conf.length() > 0) {
    this->parseConfig(conf);
  } else {
    return;
  }
}

void WifiConfig::parseConfig(String conf) {  
  DynamicJsonBuffer jsonBuffer;
  JsonArray& confRoot = jsonBuffer.parseArray(conf.c_str());

  // DEBUG_SERIAL("WifiConfig: JSON: " + conf);

  if (!confRoot.success()) {
    DEBUG_SERIAL("WifiConfig: Parsing failure");
    return;
  }

  for (int i = 0; i < confRoot.size(); i++) {
    JsonObject& net = confRoot[i];

    if (!net.containsKey("SSID") || !net.containsKey("PASS")) {
      DEBUG_SERIAL("WifiConfig: Missing SSID and/or PASS entry at " + String(i));
      return;
    }

    WifiCreds _creds(net.get<String>("SSID"), net.get<String>("PASS"));
    this->creds.push_back(_creds);
    // DEBUG_SERIAL("WifiConfig: added " + _creds.ssid() + " as " + String(i));
  }
  
  return;
}

#endif // PM25_NODEMCU_WIFICONFIG_H
