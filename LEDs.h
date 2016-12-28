#ifndef PM25_NODEMCU_LEDS_H
#define PM25_NODEMCU_LEDS_H

const int LED_PIN = D4;

void configure_leds() {
#ifdef ENABLE_LEDS
  pinMode(LED_PIN, OUTPUT);
#endif
}

void blinkLed(const int pin, const int delayOn, const int delayOff, const int loops) {
  for (int i = 0; i < loops * 2; i++) {
    digitalWrite(pin, LOW);
    delay(delayOn);
    digitalWrite(pin, HIGH);
    delay(delayOff);
  }

  // Always force off at the end.
  digitalWrite(pin, HIGH);
}

#ifdef ENABLE_LEDS
#define BLINK_LED_BOOT    blinkLed(LED_PIN, 500, 500, 3)
#define BLINK_LED_WIFI_OK blinkLed(LED_PIN, 100, 100, 8)
#define BLINK_LED_WIFI_KO blinkLed(LED_PIN, 200, 200, 8)
#define BLINK_LED_NTP_OK  blinkLed(LED_PIN, 50, 50, 3)
#define BLINK_LED_NTP_KO  blinkLed(LED_PIN, 75, 75, 3)

#define BLINK_LED_READ_READY blinkLed(LED_PIN, 100, 100, 1)
#define BLINK_LED_READ_START blinkLed(LED_PIN, 250, 250, 1)
#define BLINK_LED_READ_STOP  blinkLed(LED_PIN, 250, 250, 2)
#define BLINK_LED_PUSH_OK    blinkLed(LED_PIN, 250, 250, 4)
#else// ENABLE_LEDS
#define BLINK_LED_BOOT    delay(1)
#define BLINK_LED_WIFI_OK delay(1)
#define BLINK_LED_WIFI_KO delay(1)
#define BLINK_LED_NTP_OK  delay(1)
#define BLINK_LED_NTP_KO  delay(1)

#define BLINK_LED_READ_READY delay(1)
#define BLINK_LED_READ_START delay(1)
#define BLINK_LED_READ_STOP  delay(1)
#define BLINK_LED_PUSH_OK    delay(1)
#endif // ENABLE_LEDS

#endif // PM25_NODEMCU_LEDS_H
