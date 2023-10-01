#if !defined(Lyrav2sensors)
#define Lyrav2sensors
#include <Arduino.h>
#include <Wire.h>

#include <macros.h>

#include <BMI088.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <Adafruit_LIS3MDL.h>


#include <generallib.h>


/* accelunit object */
Bmi088Accel accelunit(Wire1,0x18);
/* gyrounit object */
Bmi088Gyro gyrounit(Wire1,0x68);

Adafruit_BMP3XX bmp;
Adafruit_LIS3MDL mdl;

const float SEALEVELPRESSURE = 1015.4;



uint8_t scani2c(){
    byte error, address;
    int nDevices;
    Serial.print("Scanning..");
    
    nDevices = 0;
    for(address = 1; address < 127; address++ )
    {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire1.beginTransmission(address);
        error = Wire1.endTransmission();
        if (error == 0)
        {
        Serial.print("I2C device found at address 0x");
        if (address<16)
            Serial.print("0");
        Serial.print(address,HEX);
        Serial.println("  !");
    
        nDevices++;
        }
        else if (error==4)
        {
        Serial.print("Unknown error at address 0x");
        if (address<16)
            Serial.print("0");
        Serial.println(address,HEX);
        }    
    }
    if (nDevices == 0){
        Serial.println("No I2C devices found\n");
        return 1;
    }
    else
        Serial.println("done\n");
    
    return 0;
}
/*
Quaterniond intergrategyros(Quaterniond prevstate){
    Quaterniond result;

    return result;
}
*/
class IMU{

float bcali[3] = {0.02305,0.0063,0.003,};

Vector3d bcal;

float acali[3][3] = {
    {1.004078865,0,0},
    {0,1.003614894,0},
    {0,0,1.007061535}};

Matrix3d acal;

public:
    IMU(){
        
        bcal << 0.02305,0.0063,0.003;

        acal << 1.004078865,0,0,
                0,1.003614894,0,
                0,0,1.007061535;
        
    };
    IMUdata data;


    

    int init(){
        int status;
        status = accelunit.begin();

        if (status < 0 )
        {
            Serial.print("accelunit init failure, error code: ");
            Serial.println(status);
            return 1;
        }


        accelunit.setRange(accelunit.RANGE_24G);
        
        status = gyrounit.begin();
        if (status < 0 )
        {
            Serial.print("gyrounit init failure, error code: ");
            Serial.println(status);
            return 2;
        }
            Serial.println("IMU init success");
            return 0;
        }

    void read(int oversampling = 50){
        IMUdata _data;
        Vector3d accel;
        Vector3d gyro;
        



        
        for (int i = 0; i < oversampling; i++)
        {
            accelunit.readSensor();
            gyrounit.readSensor();

            accel.x() += accelunit.getAccelZ_mss();
            accel.y() += accelunit.getAccelY_mss();
            accel.z() += accelunit.getAccelX_mss();

            gyro.x() += -gyrounit.getGyroZ_rads();
            gyro.y() += -gyrounit.getGyroY_rads();
            gyro.z() += gyrounit.getGyroX_rads(); // when the radians to degrees calculation of 180/PI is done at runtime, it breaks but this works so 

            delayMicroseconds(500);
            gyro.x() < -737869746455707 || gyro.x() > 737869746455707 ? gyro.x() = data.gyro.x : gyro.x() = gyro.x();
            gyro.y() < -737869746455707 || gyro.y() > 737869746455707 ? gyro.y() = data.gyro.x : gyro.y() = gyro.y();
            gyro.z() < -737869746455707 || gyro.z() > 737869746455707 ? gyro.z() = data.gyro.x : gyro.z() = gyro.z();

            accel.x() < -737869746455707 || accel.x() > 737869746455707 ? accel.x() = data.accel.x : accel.x() = accel.x();
            accel.y() < -737869746455707 || accel.y() > 737869746455707 ? accel.y() = data.accel.x : accel.y() = accel.y();
            accel.z() < -737869746455707 || accel.z() > 737869746455707 ? accel.z() = data.accel.x : accel.z() = accel.z();
            
        }
        
        accel.x() /= oversampling;
        accel.y() /= oversampling;
        accel.z() /= oversampling;
        
        gyro.x() /= oversampling;
        gyro.y() /= oversampling;
        gyro.z() /= oversampling;
        
        //float currmeas[3] = {_data.accel.x-bcali[0],_data.accel.y-bcali[1],_data.accel.z-bcali[2]};
        //Serial.printf("%f, %f, %f gainadj: %f, %f, %f ",_data.accel.x,_data.accel.y,_data.accel.z,currmeas[0],currmeas[1],currmeas[2]);
        // _data.accel.x = acali[0][0]*currmeas[0]+acali[1][0]*currmeas[1]+acali[2][0]*currmeas[2];
        // _data.accel.y = acali[0][1]*currmeas[0]+acali[1][1]*currmeas[1]+acali[2][1]*currmeas[2];
        // _data.accel.z = acali[0][2]*currmeas[0]+acali[1][2]*currmeas[1]+acali[2][2]*currmeas[2];
        //
        
       accel = accel - bcal;

       accel = acal * accel;

       _data.accel = vector3tofloat(accel);
       _data.gyro = vector3tofloat(gyro);
        //Serial.printf("multiplied: %f, %f, %f \n",_data.accel.x,_data.accel.y,_data.accel.z);
        _data.temp = accelunit.getTemperature_C();

        data = _data;
        
        return;
        
    }

};


class BARO
{
float prevalt;
float prevverticalvel[5];
int address = 0;
uint64_t prevtime;

public:
    BARO(){};
    BAROdata data;

    int init(){
        if (!bmp.begin_I2C(0x76,&Wire1))
        {
            Serial.println("BMP init failure");
            return 1;
        }
        bmp.setPressureOversampling(BMP3_OVERSAMPLING_32X);
        Serial.println("BMP init success");
        return 0;
    }
    void readsensor(){
        BAROdata _data;

        _data.altitude = bmp.readAltitude(SEALEVELPRESSURE);
        _data.pressure = bmp.readPressure();
        _data.temp = bmp.readTemperature();

        float timestep = (micros() - prevtime)/1e6;
        //Serial.printf(">timestep: %f \n",timestep);
        //prevverticalvel[address] = ((data.altitude - prevalt)/timestep);
        float deltaaltitude = _data.altitude - prevalt;
        //Serial.printf(">dalt: %f \n",deltaaltitude);

        prevverticalvel[address] = ((deltaaltitude)/timestep);//prevverticalvel[address];

        int j = 0;

        for (int i = 0; i < 5; i++)
        {
            _data.verticalvel += prevverticalvel[j];
            j++;
        }
        
        _data.verticalvel /= 5;

        address < 4 ? address++ : address = 0;

        prevalt = _data.altitude;
        prevtime = micros();
        data = _data;
    }

};

class MAG{

float bcali[3] = {-11.1517,-51.38845,0};

float acali[3][3] = {
    {0.4181084228,0,0},
    {0,0.3914369911,0},
    {0,0,1}};



public:
    MAG(){};
    MAGdata data;
    int init(){
        if (!mdl.begin_I2C(0x1C,&Wire1))
        {
            Serial.println("MAG init fail");
            return 1;
        }

        Serial.println("MAG init success");
    return 0;
    }

    int read(){
        mdl.read();

        data.gauss.x = mdl.x;
        data.gauss.y = mdl.y;
        data.gauss.z = mdl.z;

        sensors_event_t event;
        
        mdl.getEvent(&event);

        data.utesla.x = event.magnetic.x;
        data.utesla.y = event.magnetic.y;
        data.utesla.z = event.magnetic.z;

        float currmeas[3] = {data.utesla.x-bcali[0],data.utesla.y-bcali[1],data.utesla.z-bcali[2]};
        //Serial.printf("%f, %f, %f gainadj: %f, %f, %f ",_data.accel.x,_data.accel.y,_data.accel.z,currmeas[0],currmeas[1],currmeas[2]);
        data.utesla.x = acali[0][0]*currmeas[0]+acali[1][0]*currmeas[1]+acali[2][0]*currmeas[2];
        data.utesla.y = acali[0][1]*currmeas[0]+acali[1][1]*currmeas[1]+acali[2][1]*currmeas[2];
        data.utesla.z = acali[0][2]*currmeas[0]+acali[1][2]*currmeas[1]+acali[2][2]*currmeas[2];

        //data.headingdeg = atan2(data.utesla.y,data.utesla.x)*(180/PI);
        return 0;
    } 


};




#endif // Lyrav2sensors
