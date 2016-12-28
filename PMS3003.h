#ifndef PM25_NODEMCU_PMS3003_H
#define PM25_NODEMCU_PMS3003_H

#define PM_SERIAL Serial

// Using D0 triggers RST
// Using D1/D2 has some weird behavior during deep sleep
const int PMS3003_SET = D3;

void configure_pms3003_pin() {
  pinMode(PMS3003_SET, OUTPUT);
}

// To ensure low even in deep sleep, pin must be connected with
// NOT gate (2N2222) controlling +5V sent to SET pin on PMS3003
void power_pms3003(const bool power) {
  digitalWrite(PMS3003_SET, power ? LOW : HIGH);
}

void start_pms3003() {
  power_pms3003(true);
  // pmsWarmUp * 100 * 10
  for (int i = 0; i < 10; i++) {
    delay(pmsWarmUp * 100);
  }
  BLINK_LED_READ_READY;
}

void stop_pms3003() {
  power_pms3003(false);
}

void collect_pms3003_sensor() {
  int index = 0;
  char value;
  char previousValue;

  pm1 = UINT32_MAX; pm2_5 = UINT32_MAX; pm10 = UINT32_MAX;

  BLINK_LED_READ_START;
  start_pms3003();

  while (PM_SERIAL.available()) {
    value = PM_SERIAL.read();
    if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4d)) {
      serialUdpDebug("PMS3003: Cannot find the data header.");
      break;
    }

    if (index == 4 || index == 6 || index == 8 || index == 10 || index == 12 || index == 14) {
      previousValue = value;
    } else if (index == 5) {
      pm1 = 256 * previousValue + value;
    } else if (index == 7) {
      pm2_5 = 256 * previousValue + value;
    } else if (index == 9) {
      pm10 = 256 * previousValue + value;
    } else if (index > 15) {
      break;
    }

    index++;
  }

  stop_pms3003();
  BLINK_LED_READ_STOP;

  while(PM_SERIAL.available()) PM_SERIAL.read();
}

#endif // PM25_NODEMCU_PMS3003_H
