#ifndef lyralibfile
#define lyralibfile

#include <Arduino.h>

#include <Wire.h>

#define REDLED PA3
#define GREENLED PA2
#define BLUELED PA1

struct intervals
{
  unsigned long serial;
  unsigned long fetchdata;
  unsigned long telemetry;
  unsigned long logdata;
};

union accelgyrobmp
{
  struct {
  int32_t accel_x;
  int32_t accel_y;
  int32_t accel_z;
  int32_t gyro_x;
  int32_t gyro_y;
  int32_t gyro_z;
  int32_t yaw;
  int32_t pitch;
  int32_t roll;
  int32_t imutemp;
  int32_t pressure;
  int32_t altitude;
  int32_t bmptemp;
  } readable;
  int32_t data[13];
};


void inttobytearray(int32_t value,uint8_t *buf){
  uint32_t adjvalue = value + 2147483647;
  buf[3] = adjvalue;
  buf[2] = adjvalue >> 8;
  buf[1] = adjvalue >> 16;
  buf[0] = adjvalue >> 24;
}

int32_t bytearraytoint(uint8_t *array){
  int32_t value;
  value = ((int32_t)array[0] << 24) | ((int32_t)array[1] << 16) | ((int32_t)array[2] << 8) | (int32_t)array[3];
  value = value - 2147483647;
  
  return value;
}

void scani2c(TwoWire Wire,HardwareSerial Seria){
    byte error, address;
    int nDevices;
 
    Seria.println("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Seria.print("I2C device found at address 0x");
      if (address<16)
        Seria.print("0");
      Seria.print(address,HEX);
      Seria.println("  !");
 
      nDevices++;
    }
    else if (error==4)
    {
      Seria.print("Unknown error at address 0x");
      if (address<16)
        Seria.print("0");
      Seria.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Seria.println("No I2C devices found\n");
  else
    Seria.println("done\n");
}


void setled(int color){
  switch (color)
  {
  case 0:
    digitalWrite(REDLED,HIGH);
    digitalWrite(GREENLED,LOW);
    digitalWrite(BLUELED,LOW);
    break;
  
  case 1:
    digitalWrite(REDLED,LOW);
    digitalWrite(GREENLED,HIGH);
    digitalWrite(BLUELED,LOW);
    break;
  
  case 2:
    digitalWrite(REDLED,LOW);
    digitalWrite(GREENLED,LOW);
    digitalWrite(BLUELED,HIGH);
    break;
  
  case 3:
    digitalWrite(REDLED,HIGH);
    digitalWrite(GREENLED,HIGH);
    digitalWrite(BLUELED,LOW);
    break;
  
  case 4:
    digitalWrite(REDLED,HIGH);
    digitalWrite(GREENLED,LOW);
    digitalWrite(BLUELED,HIGH);
    break;
  
  default:
    break;
  }
}


#endif