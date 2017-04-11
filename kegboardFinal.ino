/* This is just a test to see if this whole thing will work*/
#include "kegboard.h"
#include "KegboardPacket.h"

#define FIRMWARE_VERSION 18
#define SERIAL_NUMBER_SIZE_BYTES 9

static KegboardPacket gInputPacket;

//FOR TESTING
#define TIME_BETWEEN_POURS (30 * 1000)
#define TIME_OF_POUR (20*1000)


/* NOTE: D2 and A0 cannot be used at the same time b/c external Interrupt
* line is shared */
int tempPin = A0;
/* NOTE: D1 and A4 cannot be used at the same time b/c external Interrupt
* line is shared */
int meter0Pin = D1;
/* NOTE: D3 and DAC cannot be used at the same time b/c external Interrupt
* line is shared */
int meter1Pin = D3;
int meter2Pin = D5;
/* this is the ?? LED which indicates when meter 0 is being poured */
int meter0LEDPin = D0;
/* this is the ?? LED which indicates when meter 1 is being poured */
int meter1LEDPin = D2;
/* this is the ?? LED which indicates when meter 2 is being poured */
int meter2LEDPin = D4;


// Structure to keep information about this device's uptime.
typedef struct {
  unsigned long uptime_ms;
  unsigned long uptime_days;
  unsigned long last_uptime_ms;
  unsigned long last_meter_event;
  unsigned long last_heartbeat;
  unsigned long last_temp_event;
} UptimeStat;

static UptimeStat gUptimeStat;

static uint8_t gSerialNumber[SERIAL_NUMBER_SIZE_BYTES] = "FORGE001";

/* Only supporting 2 meters per kegboard at the moment*/
static unsigned long volatile gMeters[] = {0, 0};
static unsigned long volatile gLastMeters[] = {0, 0};

static uint16_t gProtocolVersion = 1;

// Interrupts

#define INCREMENT_METER(meter_index) gMeters[meter_index] += 1;

void meter0Interrupt(void)
{
  INCREMENT_METER(0);
  digitalWrite(blueLEDPin, HIGH);
}

void meter1Interrupt(void)
{
  INCREMENT_METER(1);
  digitalWrite(redLEDPin, HIGH);
}
void meter2Interrupt(void)
{
  
}

//
// Serial I/O
//

void writeHelloPacket()
{
  uint16_t firmware_version = FIRMWARE_VERSION;
  KegboardPacket packet;
  packet.SetType(KBM_HELLO_ID);
  packet.AddTag(KBM_HELLO_TAG_PROTOCOL_VERSION, sizeof(gProtocolVersion), (char*)&gProtocolVersion);
  packet.AddTag(KBM_HELLO_TAG_FIRMWARE_VERSION, sizeof(firmware_version), (char*)&firmware_version);
  packet.AddTag(KBM_HELLO_TAG_SERIAL_NUMBER, SERIAL_NUMBER_SIZE_BYTES, (char*)gSerialNumber);
  packet.AddTag(KBM_HELLO_TAG_UPTIME_MILLIS, sizeof(gUptimeStat.uptime_ms), (char*)&gUptimeStat.uptime_ms);
  packet.AddTag(KBM_HELLO_TAG_UPTIME_DAYS, sizeof(gUptimeStat.uptime_days), (char*)&gUptimeStat.uptime_days);
  packet.Print();
}

void writeMeterPacket(int channel)
{
  char name[5] = "flow";
  unsigned long status = gMeters[channel];
  if (status == gLastMeters[channel])
  {
    if (0 == channel)
    {
      digitalWrite(blueLEDPin, LOW);
    }
    else
    {
      digitalWrite(redLEDPin, LOW);
    }
    return;
  }
  else
  {
    gLastMeters[channel] = status;
  }

  name[4] = 0x30 + channel;
  KegboardPacket packet;
  packet.SetType(KBM_METER_STATUS);
  packet.AddTag(KBM_METER_STATUS_TAG_METER_NAME, 5, name);
  packet.AddTag(KBM_METER_STATUS_TAG_METER_READING, sizeof(status), (char*)(&status));
  packet.Print();
}

void writeThermoPacket()
{
  /* in C*/
  float tempC = 0;
  float tempF = 0;
  char name[8] = "thermo";
  float voltage = (analogRead(tempPin) * 3.3)/4095;

  tempC = (voltage - 0.5) * 100;

  KegboardPacket packet;
  packet.SetType(KBM_THERMO_READING);
  packet.AddTag(KBM_THERMO_READING_TAG_SENSOR_NAME, sizeof(name), name);
  packet.AddTag(KBM_THERMO_READING_TAG_SENSOR_READING, sizeof(tempC), (char*)(&tempC));
  packet.Print();

}

void setup()
{
  /* clearing out stored wifi creds */
  WiFi.clearCredentials();
  Serial.begin(115200);
  memset(&gUptimeStat, 0, sizeof(UptimeStat));
  pinMode(tempPin, INPUT);
  pinMode(meter0Pin, INPUT_PULLUP);
  pinMode(meter1Pin, INPUT_PULLUP);
  pinMode(blueLEDPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  attachInterrupt(meter0Pin, meter0Interrupt, FALLING);
  attachInterrupt(meter1Pin, meter1Interrupt, FALLING);
  writeHelloPacket();

}

void updateTimekeeping() {
  // TODO(mikey): it would be more efficient to take control of timer0
  unsigned long now = millis();
  gUptimeStat.uptime_ms += now - gUptimeStat.last_uptime_ms;
  gUptimeStat.last_uptime_ms = now;

  if (gUptimeStat.uptime_ms >= MS_PER_DAY) {
    gUptimeStat.uptime_days += 1;
    gUptimeStat.uptime_ms -= MS_PER_DAY;
  }

  if ((now - gUptimeStat.last_heartbeat) > KB_HEARTBEAT_INTERVAL_MS) {
    gUptimeStat.last_heartbeat = now;
    writeHelloPacket();
  }
}

void writeMeterPackets() {
  unsigned long now = millis();

  // Forcibly coalesce meter updates; we want to be responsive, but sending
  // meter updates at every opportunity would cause too many messages to be
  // sent.
  if ((now - gUptimeStat.last_meter_event) > KB_METER_UPDATE_INTERVAL_MS) {
    gUptimeStat.last_meter_event = now;
  } else {
    return;
  }

  writeMeterPacket(0);
  writeMeterPacket(1);

}

void writeTempPackets()
{
  unsigned long now = millis();
  if ((now - gUptimeStat.last_temp_event) > KB_TEMP_UPDATE_INTERVAL_MS) {
    gUptimeStat.last_temp_event = now;
  } else {
    return;
  }
  writeThermoPacket();

}


void loop()
{
  updateTimekeeping();
  writeMeterPackets();
  writeTempPackets();

}
