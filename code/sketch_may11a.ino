#include <math.h>

int THERMISTOR_PIN = A0;
int MOSFET_PIN = 8;

enum ZONE {
  STANDBY,
  PREHEAT,
  CONSTANT,
  REFLOW_RISE,
  REFLOW_HOLD,
  COOLING
};
enum ZONE curr_zone = STANDBY;

double PREHEAT_RATE_CS = 1;
double PREHEAT_TARGET_C = 120;

double CONSTANT_TIME_S = 180;

double REFLOW_TARGET_C = 200;
double REFLOW_TIME_S = 60;

double COOLING_RATE_CS = 2;
double COOLING_TARGET_C = 50;

double temperature() {
  double adc = 0;
  int N = 100;
  for (int i = 0; i < N; i++) {
    adc += double(analogRead(THERMISTOR_PIN)) / N;
  }
  double voltage = (5.0 / 1024.0) * adc;
  double resistance = (5.0 / voltage) * 1000 - 1000;
  double temperature = (1 / ((log(resistance / 100000) / 3950) + (1 / 298.15))) - 273.15;


  return temperature;
}

void set_mosfet(double target) {
  double curr_temp = temperature();
  double THRESHHOLD = 2.5 ? curr_temp < 50.0 : 0.5;

  if (curr_zone == STANDBY) {
    digitalWrite(MOSFET_PIN, LOW);  
  } else if (curr_temp <= target - THRESHHOLD) {
    digitalWrite(MOSFET_PIN, HIGH);
  } else if (curr_temp >= target + THRESHHOLD) {
    digitalWrite(MOSFET_PIN, LOW);
  }
}

void plot() {
  Serial.print(double(millis()) / 1000.0);
  Serial.print(";");
  Serial.print(temperature());
  Serial.print(";");
  Serial.print(digitalRead(MOSFET_PIN));
  Serial.print(";");
  Serial.print(curr_zone);
  Serial.println();

  // Serial.print("T:");
  // Serial.print(temperature());
  // Serial.print(",");

  // Serial.print("MOSFET:");
  // Serial.print(digitalRead(MOSFET_PIN));
  // Serial.print(",");

  // Serial.print("ZONE:");
  // Serial.print(curr_zone);
  // Serial.println();
}

void setup() {
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, LOW);
  Serial.begin(9600);

  plot();
}

void loop() {
  unsigned long start_ms;

  unsigned long step_ms;
  double step_temp;

  double last_plot_ms;

  while (true) {
    while (Serial.available() > 0) {
      if (Serial.read() == 'c') {
        while (Serial.read() != -1);
        curr_zone = STANDBY;
      }
    }

    double target = 25;

    double curr_temp = temperature();
    unsigned long t = millis() - step_ms;

    switch (curr_zone) {
      case STANDBY:
      {
        set_mosfet(target);
        Serial.println("Ready! Press [g] to start");
        while (Serial.read() != 'g') {}
        curr_zone = PREHEAT;
        start_ms = millis();

        step_ms = start_ms;
        step_temp = temperature();

        last_plot_ms = 0;
      } break;
      case PREHEAT:
        {
          if (curr_temp >= PREHEAT_TARGET_C) {
            target = PREHEAT_TARGET_C;
            step_ms = millis();
            step_temp = temperature();
            curr_zone = CONSTANT;
          } else {
            target = PREHEAT_RATE_CS * (double(t) / 1000.0) + step_temp;
            if (target - curr_temp > 10.0) {
              Serial.println("readjust");
              step_ms = millis();
              step_temp = curr_temp + 5.0;
            }
          }
        }
        break;
      case CONSTANT:
        {
          target = PREHEAT_TARGET_C;
          if (t >= CONSTANT_TIME_S * 1000) {
            step_ms = millis();
            step_temp = temperature();
            curr_zone = REFLOW_RISE;
          }
        }
        break;
      case REFLOW_RISE:
        {
          target = REFLOW_TARGET_C;
          if (curr_temp >= REFLOW_TARGET_C) {
            step_ms = millis();
            step_temp = temperature();
            curr_zone = REFLOW_HOLD;
          }
        }
        break;
      case REFLOW_HOLD:
        {
          target = REFLOW_TARGET_C;
          if (t >= REFLOW_TIME_S * 1000) {
            step_ms = millis();
            step_temp = temperature();
            curr_zone = COOLING;
          }
        }
        break;
      case COOLING:
        {
          if (curr_temp <= COOLING_TARGET_C) {
            target = REFLOW_TARGET_C;
            step_ms = millis();
            step_temp = temperature();
            curr_zone = STANDBY;
          } else {
            target = - COOLING_RATE_CS * (double(t) / 1000.0) + step_temp;
            if (curr_temp - target > 10) {
              step_ms = millis();
              step_temp = curr_temp - 5.0;
            }
          }
        }
        break;
    }

    set_mosfet(target);

    if (millis() - last_plot_ms >= 1) {
      plot();
      // Serial.print("target:");
      // Serial.println(target);
      last_plot_ms = millis();
    }
  }

  Serial.println("DONE!");
}
