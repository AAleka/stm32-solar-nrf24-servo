#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <STM32LowPower.h>
#include <Servo.h>

#define CE_PIN  PB0
#define CSN_PIN PA4
#define IRQ_PIN PA0
#define SERVO_PIN PA1
#define ADC_PIN A9

#define VREF 3300
#define DIV 2
#define BAT_CALIB 96
#define SAMPLES 16

#define MAX_TEXT_SIZE 9

Servo servo;

RF24 radio(CE_PIN, CSN_PIN);

const byte address[][6] = {"1Node", "2Node"};
volatile bool radioInterrupt = false;

volatile bool radioOff = false;
int sleep_min = 0;

void setupRadio();
void onRadioIRQ() { radioInterrupt = true; }

uint32_t getBattery();

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Serial.println("Started");

  analogWriteFrequency(50);
  analogReadResolution(12);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(IRQ_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), onRadioIRQ, FALLING);

  LowPower.begin();

  if (!radio.begin())
  {
    while (1);
  }
  setupRadio();
  delay(500);
  
  servo.attach(SERVO_PIN);
  delay(100);
  servo.write(90);
  delay(1000);
  servo.detach();
}

void loop()
{
  if (radioOff)
  {
    sleep_min = 0;
    radioOff = false;

    radio.powerUp();
    setupRadio();
    delay(2000);

    radio.startListening();
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Radio up");
  }

  Serial.flush();
  delay(100);
  if (!radio.available())
  {
    LowPower.deepSleep();
  }

  if (radioInterrupt)
  {
    radioInterrupt = false;

    if (radio.available())
    {
      char text[MAX_TEXT_SIZE+1] = "";

      uint8_t len = 0;
      len = radio.getDynamicPayloadSize(); 
      
      if (len > MAX_TEXT_SIZE) len = MAX_TEXT_SIZE; 

      radio.read(&text, len);
      text[len] = '\0';
      Serial.printf("Received: %s\n", text);

      String reply = "";
      if (strncmp(text, "servo ", 6) == 0)
      {
        int angle = atoi(text + 6);
        angle = constrain(angle, 0, 180);
        
        servo.attach(SERVO_PIN);
        delay(200);
        servo.write(angle);
        delay(1000);
        servo.detach();

        reply = "servo " + String(angle);
      } 
      else if (strcmp(text, "on") == 0)
      {
        digitalWrite(LED_BUILTIN, LOW);
        reply = "LEDOn";
      } 
      else if (strcmp(text, "off") == 0)
      {
        digitalWrite(LED_BUILTIN, HIGH);
        reply = "LEDOff";
      }
      else if (strcmp(text, "btlvl") == 0)
      {
        reply = String(getBattery());
      }
      else if (strncmp(text, "rdoff ", 6) == 0)
      {
        sleep_min = atoi(text + 6);
        radioOff = true;
        reply = "rdoff " + String(sleep_min);
      }

      Serial.println(reply);
      radio.stopListening();
      radio.write(reply.c_str(), reply.length() + 1);
      radio.startListening();

      if (radioOff)
      {
        Serial.println("Radio down");
        servo.detach();
        radio.powerDown();
        digitalWrite(LED_BUILTIN, LOW);
        LowPower.deepSleep(sleep_min * 60 * 1000);
      }
    }
  }
}

void setupRadio()
{
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setRetries(3, 5);
  radio.openWritingPipe(address[0]);
  radio.openReadingPipe(1, address[1]);
  radio.startListening();
}

uint32_t getBattery()
{
  uint32_t sum = 0;
  for (int i = 0; i < SAMPLES; ++i)
  {
    sum += analogRead(ADC_PIN);
    delay(100);
  }

  return (sum / SAMPLES) * VREF * DIV * BAT_CALIB / (409500);
}