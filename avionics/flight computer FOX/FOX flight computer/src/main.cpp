#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <Wifi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>


MPU6050 imu;

Adafruit_BMP280 bmp;
Servo canards[4];
Preferences flash;

#define OUTPUT_READABLE_YAWPITCHROLL
#define INTERRUPT_PIN 34
// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vecto



/*
float kp = 0.5;
float ki = 0.1;
float kd = 0.1;

float kproll = 1;
float kiroll = 0.1;
float kdroll = 0.1;

float preverror = 0.1;
float accumulatederror = 0.1;



int rolloffset = 0;
// x+ x- y+ y-
int canardpos[4];
int canardpins[4] = {25, 26, 14, 13};
int canardsdefualtpos[4] = {90,  90, 90, 90};
*/
int detectiontries = 1;

unsigned long uptimemillis;
unsigned long missiontimemillis;


float prevbaroalt;

float drift[3] = {0.55,0.52,0.62};

int buzzerpin = 16;
int battpin = 32;

float battvoltage;

float mpucalibratedgyros[3];
float mpucalibratedaccell[3];
float bmpcalibratedaltidue;

float accelbias[3] = {0.6378775,0.0223087,-1.2146027};
float gyrobias[3] = {-0.0389415,0.0166735,0.0245533};

bool sendserial = false;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int state = 0;

struct intervals
{
  int datalogging;
  int sendtelem;
  int getdata;
  int sendserial;
};


struct datatolog
{
  int state;
  unsigned long uptimemillis;
  unsigned long missiontimemillis;

  float roll;
  float pitch;
  float yaw;

  float roll_rate;
  float pitch_rate;
  float yaw_rate;

  float accel_x;
  float accel_y;
  float accel_z;
  float absaccel;

  float velocity;
  float vertical_velocity;

  float imu_temp;
  float baro_temp;

  float baro_alt;
  float baro_pressure;

  int canardstatusx1;
  int canardstatusx2;

  int canardstatusy1;
  int canardstatusy2;

  int command;

};

struct datatotransmit
{
  int state;
  unsigned long uptimemillis;
  unsigned long missiontimemillis;

  float roll;
  float pitch;
  float yaw;

  float roll_rate;
  float pitch_rate;
  float yaw_rate;

  float accel_x;
  float accel_y;
  float accel_z;

  float vel_x;
  float vel_y;
  float vel_z;

  float absaccel;
  float absvel;

  float vertical_vel;
  float baro_alt;

  float batteryvolt;
};

struct datanew
{
  int command;
};

struct sensordata
{
  float accel_x;
  float accel_y;
  float accel_z;

  float gyro_x;
  float gyro_y;
  float gyro_z;

  float imu_temp;
  float baro_temp;

  float baro_alt;
  float baro_pressure;

  float vertical_vel;

  float absaccel;

  float pitch;
  float yaw;
  float roll;
  
  float vel_x;
  float vel_y;
  float vel_z;

  float absvel;
};


struct prevmillllis
{
  unsigned long cycle;
  unsigned long getdata;
  unsigned long serial;
  unsigned long pid;
  unsigned long controlcycle;
  unsigned long telemtransmit;
  unsigned long detectionprevmillis;
};


prevmillllis prevmilliss;
sensordata currentdata;
datanew grounddata;

intervals stateintervals[7] = {
  {2000,1000,100,200}, //ground idle
  {50,50,20,100}, // ready to launch
  {50,20,20,100}, // powered ascent
  {50,20,20,100}, // unpowered ascent
  {50,20,20,100}, // ballistic decsent
  {50,20,20,100}, // retarded descent
  {1000,20,100,200} // landed
};


datatotransmit transmitdata;

esp_now_peer_info_t peerInfo;
/*
float PID(float setpoint, float input, float kp, float ki, float kd) {
  
  float deltatime = (millis() - prevmilliss.pid) / 1000;
  float error = setpoint - input;
  float deltaerror = error - preverror;
  accumulatederror += error * deltatime;
  float p = kp * error;
  float i = ki * accumulatederror;
  float d = kd * deltaerror;
  preverror = error;
  prevmilliss.pid = uptimemillis;
  return p;
  
}
*/

//mpu interrupt deteteciton routine

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}

// function definintions
void initbaro() {
 Serial.println(F("BMP280 test"));
  unsigned status;
  //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  status = bmp.begin(0x76);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
}
}

void initimu(){
  // initlizing and testing the imu
  Serial.println("IMU init");
  imu.initialize();
  pinMode(INTERRUPT_PIN,INPUT);

  Serial.println(F("Testing device connections..."));
  Serial.println(imu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

  devStatus = imu.dmpInitialize();

  // gyro offsets
  imu.setXGyroOffset(220);
  imu.setYGyroOffset(76);
  imu.setZGyroOffset(-85);
  imu.setZAccelOffset(1788); // 1688 factory default for my test chip

  if (devStatus == 0)
  { 
    // calibrate accels and gyros
    imu.CalibrateAccel(6);
    imu.CalibrateGyro(6);
    imu.PrintActiveOffsets();

    //enable dmp
    imu.setDMPEnabled(true);

    // enable inturrupts
    Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
    Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
    Serial.println(F(")..."));
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = imu.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use it
    Serial.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = imu.dmpGetFIFOPacketSize();
  }
  
}

void calibratebaro(){
  int targetiterations = 50;
  float alt;
  for (int i = 0; i < targetiterations; i++)
  {
    alt += bmp.readAltitude(1013.25);
    Serial.print(alt);
    Serial.print(" iter ");
    Serial.println(i);
    delay(50);
  }
  alt = alt/targetiterations;
  bmpcalibratedaltidue = alt;
}

sensordata getsensordata(){
  sensordata data;
  if (imu.dmpGetCurrentFIFOPacket(fifoBuffer))
  {
    imu.dmpGetQuaternion(&q, fifoBuffer);
    imu.dmpGetGravity(&gravity, &q);
    imu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    imu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
    data.pitch = ypr[1];
    data.yaw = ypr[0];
    data.roll = ypr[2];

    data.accel_x - aaReal.x;
    data.accel_y - aaReal.y;
    data.accel_z - aaReal.z;

    data.absaccel = aaReal.getMagnitude();

    
  }

  data.baro_alt = bmp.readAltitude() - bmpcalibratedaltidue;
  data.baro_pressure = bmp.readPressure();
  data.baro_temp =  bmp.readTemperature();
  
  return data;
}

datatotransmit preptelemetry(sensordata data){
  datatotransmit message;
  message.state = state;
  message.accel_x = data.accel_x;
  message.accel_y = data.accel_y;
  message.accel_z = data.accel_z;

  message.vel_x = data.vel_x;
  message.vel_y = data.vel_y;
  message.vel_z = data.vel_z;

  message.absvel = data.absvel;

  message.pitch_rate = data.gyro_x;
  message.yaw_rate = data.gyro_y;
  message.roll_rate= data.gyro_z;

  message.pitch = data.pitch;
  message.yaw = data.yaw;
  message.roll = data.roll;

  message.baro_alt = data.baro_alt;
  message.vertical_vel = data.vertical_vel;

  message.uptimemillis = uptimemillis;

  message.absaccel = data.absaccel;

  message.missiontimemillis = missiontimemillis;

  message.batteryvolt = battvoltage;
  return message;
}

datatolog prepdatalog(sensordata data){
  datatolog message;
  message.uptimemillis = uptimemillis;
  message.missiontimemillis = missiontimemillis;
  message.state = state;
  message.accel_x = data.accel_x;
  message.accel_y = data.accel_y;
  message.accel_z = data.accel_z;

  message.pitch_rate = data.gyro_x;
  message.yaw_rate = data.gyro_y;
  message.roll_rate= data.gyro_z;

  message.pitch = data.pitch;
  message.yaw = data.yaw;
  message.roll = data.roll;

  message.baro_alt = data.baro_alt;
  message.baro_pressure = data.baro_pressure;

  message.imu_temp = data.imu_temp;
  message.baro_temp = data.baro_temp;
  /*
  message.canardstatusx1 = canardpos[0];
  message.canardstatusx2 = canardpos[1];

  message.canardstatusy1 = canardpos[2];
  message.canardstatusy2 = canardpos[3];
  */
  message.command = grounddata.command;

  return message;
}

void onsendtelem(const uint8_t *mac_addr, esp_now_send_status_t status){
  
}

void onrecivetelem(const uint8_t *macAddr, const uint8_t *data, int dataLen){
  datanew* telemetrytemp = (datanew*) data;
  grounddata = *telemetrytemp;
  Serial.print("Received");
  tone(10000,50);
  
}


void broadcast(const datatotransmit &message)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &message, sizeof(message));
  // Print results to serial monitor
  /*
  if (result == ESP_OK)
  {
    Serial.print("Broadcast message success ");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_INIT)
  {
    Serial.println("ESP-NOW not Init.");
  }
  else if (result == ESP_ERR_ESPNOW_ARG)
  {
    Serial.println("Invalid Argument");
  }
  else if (result == ESP_ERR_ESPNOW_INTERNAL)
  {
    Serial.println("Internal Error");
  }
  else if (result == ESP_ERR_ESPNOW_NO_MEM)
  {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
  {
    Serial.println("Peer not found.");
  }
  else
  {
    Serial.println("Unknown error");
  }
  */
}

void sendserialdata(sensordata data){
  int decimals = 4;
  Serial.print(uptimemillis);
  Serial.print(",\t");

  Serial.print(data.accel_x,decimals);
  Serial.print(", ");
  Serial.print(data.accel_y,decimals);
  Serial.print(", ");
  Serial.print(data.accel_z,decimals);

  Serial.print(",\t");
  Serial.print(data.gyro_x,decimals);
  Serial.print(", ");
  Serial.print(data.gyro_y,decimals);
  Serial.print(", ");
  Serial.print(data.gyro_z,decimals);

  Serial.print(",\t");
  Serial.print(data.pitch,decimals);
  Serial.print(", ");
  Serial.print(data.yaw,decimals);
  Serial.print(", ");
  Serial.print(data.roll,decimals);

  Serial.print(",\t");
  Serial.print(data.imu_temp);
  Serial.print(", ");
  Serial.print(data.baro_alt);
  Serial.print(", ");
  Serial.print(battvoltage);
  Serial.println(",");
}

void logdata(datatolog data){

}


void setup() {
  tone(2000, 100);
  uptimemillis = millis();
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  initimu();
  




  initbaro();
  tone(3000, 100);


  WiFi.mode(WIFI_MODE_STA);
  Serial.println("WiFi init ");
  Serial.print("Mac Address: ");
  Serial.println(WiFi.macAddress());
  WiFi.disconnect();

  tone(4000, 100);

  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(onrecivetelem);
    esp_now_register_send_cb(onsendtelem);
  }
  else
  {
    Serial.println("ESP-NOW Init Failed");
  }
  
  tone(3000, 100);
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  
  //calibratempu();
  calibratebaro();
  currentdata = getsensordata();
  flash.begin("data",false);
  tone(5000, 100);
  delay(100);
  tone(5000, 100);
  // put your setup code here, to run once:
}

void loop() {
  uptimemillis = millis();
  if (uptimemillis - prevmilliss.getdata > stateintervals[state].getdata)
    {
      currentdata = getsensordata();
      battvoltage = (float(analogRead(battpin))-780)/180;
      prevmilliss.getdata = uptimemillis;
    }
  // put your main code here, to run repeatedly:
  
  if (uptimemillis - prevmilliss.serial > stateintervals[state].sendserial && sendserial == true)
  {

    sendserialdata(currentdata);
    prevmilliss.serial = uptimemillis;
  }
  if (Serial.available() > 0)
  {
      int incomingbyte = Serial.read();
      Serial.print("echo: ");
      Serial.println(incomingbyte);
      grounddata.command = incomingbyte;
    }
  
  if (uptimemillis - prevmilliss.telemtransmit > stateintervals[state].sendtelem)
  {
    datatotransmit telemetry = preptelemetry(currentdata);
    broadcast(telemetry);
    prevmilliss.telemtransmit = uptimemillis;
  }

  if (grounddata.command != 0)
  {
    switch (grounddata.command)
    {
    case 109:
      currentdata.pitch = 0;
      currentdata.yaw = 0;
      currentdata.roll = 0;
      currentdata.absvel = 0;
      currentdata.vel_x = 0;
      currentdata.vel_y = 0;
      currentdata.vel_z = 0;
      
      grounddata.command = 0;
      break;

    case 112:
      //calibratempunew();
      
      grounddata.command = 0;
      break;
    
    case 108:
      state = 1;
      grounddata.command = 0;
      break;
    
    case 98:
      calibratebaro();
      grounddata.command = 0;
      break;
    
    case 116:
      ESP.restart();
      grounddata.command = 0;
      break;
    
    case 119:
      tone(5000,200);
      delay(500);
      tone(5000,200);
      grounddata.command = 0;
      break;

    case 115:
      sendserial = !sendserial;
      Serial.write("toggleserial");
      grounddata.command = 0;
      break;

    default:
      break;
    }
  }
  
  if (uptimemillis - prevmilliss.detectionprevmillis < 100)
  {
    switch (state)
    {
    case 1:
      // detecting liftoff
      if (currentdata.absaccel > 20 && detectiontries <= 4)
      {
        detectiontries++;
      }
      else if (detectiontries >= 4){
        state = 2;
        detectiontries = 0;
      }
      else{detectiontries = 0;}
      break;
    case 2:
      // detecting burnout
      if (currentdata.absaccel < 2 && detectiontries <= 4)
      {
        detectiontries++;
      }
      else if (detectiontries >= 4){
        state = 3;
        detectiontries = 0;
      }
      else{detectiontries = 0;}
      break;
    
    case 3:
      // detecting apoogee
      if (currentdata.vertical_vel < 5 && detectiontries <= 4)
      {
        detectiontries++;
      }
      else if (detectiontries >= 4){
        state = 4;
        detectiontries = 0;
      }
      else{detectiontries = 0;}
      break;

    case 4:
      // detecting  parachute openeing
      if (currentdata.absaccel > 6 && detectiontries <= 4)
      {
        detectiontries++;
      }
      else if (detectiontries >= 4){
        state = 5;
        detectiontries = 0;
      }
      else{detectiontries = 0;}
      break;

    case 5:
      // detecting landing
      if (currentdata.vertical_vel > 3 && detectiontries <= 4)
      {
        detectiontries++;
      }
      else if (detectiontries >= 4){
        state = 6;
        detectiontries = 0;
      }
      else{detectiontries = 0;}
      break;
    
    default:
      break;
    }
  }
  
  
    
  prevmilliss.detectionprevmillis = uptimemillis;
  prevmilliss.cycle = uptimemillis;


  /*
  if (uptimemillis - prevmilliss.controlcycle > 20 && state == 2)
  {
    rolloffset = PID(0, currentorientation.angle_z, kp, ki, kd);
    canardpos[0] = (canardsdefualtpos[0] + PID(0, currentorientation.angle_x, kp, ki, kd));
    canardpos[1] = (canardsdefualtpos[1] + (PID(0, currentorientation.angle_x, kp, ki, kd))*-1);
    canardpos[2] = (canardsdefualtpos[2] + PID(0, currentorientation.angle_y, kp, ki, kd));
    canardpos[3] = (canardsdefualtpos[3] + (PID(0, currentorientation.angle_y, kp, ki, kd))*-1);
    
    canardpos[0] += rolloffset;
    canardpos[1] += rolloffset;
    canardpos[2] += rolloffset;
    canardpos[3] += rolloffset;


    canards[0].write(canardpos[0]);
    canards[1].write(canardpos[1]);
    canards[2].write(canardpos[2]);
    canards[3].write(canardpos[3]);
    prevmilliss.controlcycle = uptimemillis;
  }
  */

 /*
void calibratempu() {
  currentdata.pitch = 0;
  currentdata.yaw = 0;
  currentdata.roll = 0;
  currentdata.absvel = 0;
  currentdata.vel_x = 0;
  currentdata.vel_y = 0;
  currentdata.vel_z = 0;

  int targetiteratinos = 400;
  float accelvalues[3];
  float gyrovalues[3];

  for (int i = 0; i < targetiteratinos; i++)
  {
    sensors_event_t a, g, temp;
    imu.getEvent(&a, &g, &temp);

    accelvalues[0] += a.acceleration.x;
    accelvalues[1] += a.acceleration.y;
    accelvalues[2] += a.acceleration.z;

    gyrovalues[0] += g.gyro.x;
    gyrovalues[1] += g.gyro.y;
    gyrovalues[2] += g.gyro.z;

    delay(25);

  }
  accelvalues[0] = accelvalues[0]/targetiteratinos;
  accelvalues[1] = accelvalues[1]/targetiteratinos;
  accelvalues[2] = accelvalues[2]/targetiteratinos;

  Serial.print(accelvalues[0],7);
  Serial.print(",");
  Serial.print(accelvalues[1],7);
  Serial.print(",");
  Serial.print(accelvalues[2],7);
  Serial.println("\t");

  gyrovalues[0] = gyrovalues[0]/targetiteratinos;
  gyrovalues[1] = gyrovalues[1]/targetiteratinos;
  gyrovalues[2] = gyrovalues[2]/targetiteratinos;

  Serial.print(gyrovalues[0],7);
  Serial.print(",");
  Serial.print(gyrovalues[1],7);
  Serial.print(",");
  Serial.print(gyrovalues[2],7);
  Serial.println("\t");

}
*/

/*
  for (int i = 0; i < 3; i++)
  {
    canards[i].setPeriodHertz(50);
    canards[i].attach(canardpins[i], 500, 2400);
    canards[i].write(canardsdefualtpos[i]);
  }
  */
}
