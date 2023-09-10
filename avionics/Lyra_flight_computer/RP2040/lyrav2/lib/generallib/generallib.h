#if !defined(GENERALLIB)
#define GENERALLIB
#include <Arduino.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "Lyrav2sensors.h"
#include "macros.h"
#include "SPI.h"
#include "SD.h"

Sd2Card card;
File logfile;

union navpacket
{
    struct
    {
        uint32_t errorflag;
    } r;
    uint32_t data [sizeof(r)];
};




int waitfornextfifo(int timeoutmillis){
    uint32_t pushmillis = millis();
    int currentavalible = rp2040.fifo.available();
    while ((millis() - pushmillis) < timeoutmillis && rp2040.fifo.available() <= rp2040.fifo.available());
    if (rp2040.fifo.available() >= currentavalible)
    {
        return 0;
    }
    return 1;
}

int waitfornextfifo(){
    int currentavalible = rp2040.fifo.available();
    while (rp2040.fifo.available() <= rp2040.fifo.available());
    return 0;
}

uint32_t fifopop(){
    uint32_t data;
    rp2040.fifo.pop_nb(&data);
    return data;
}



class MPCORE{
    
    

    public:
        MPCORE(){};

        void setuppins(){
            pinMode(LEDRED,OUTPUT);
            pinMode(LEDGREEN,OUTPUT);
            pinMode(LEDBLUE,OUTPUT);
            pinMode(BUZZERPIN,OUTPUT);

            digitalWrite(LEDRED, LOW);
            digitalWrite(LEDGREEN, HIGH);
            digitalWrite(LEDBLUE, HIGH);
            return;
        }


        void beep(){
            tone(BUZZERPIN,2000,200);
            return;
        }

        void beep(int freq){
            tone(BUZZERPIN,freq,200);
            return;
        }

        void beep(int freq, unsigned int duration){
            tone(BUZZERPIN,freq,duration);
        return;
        }

        navpacket fetchnavdata(){
            navpacket recivedpacket;
            if (rp2040.fifo.available() <= 0)
            {
                return recivedpacket;
            }
            for (int i = 0; i < sizeof(recivedpacket.data)/sizeof(recivedpacket.data[0]); i++)
            {
                recivedpacket.data[i] = fifopop();
            }
            
            return recivedpacket;
        }

        int handshake(){
            uint32_t data;
            int connectiontries = 0;
            while (connectiontries <= 3)
            {
            
                if (waitfornextfifo(1000) == 1)
                {
                    Serial.println("NAV core timeout");
                    rp2040.restartCore1();
                    connectiontries++;
                    break;
                }

                rp2040.fifo.pop_nb(&data);
                if (!rp2040.fifo.push_nb(0xAB))
                {
                    Serial.println("unable to push to fifo");
                    break;
                }
                
                waitfornextfifo(500);
                rp2040.fifo.pop_nb(&data);
                if(data != 0xCD)
                {
                    Serial.print("NAV Handshake Failed: expected 0xCD, got 0x");
                    Serial.println(data,HEX);
                    rp2040.restartCore1();
                    connectiontries++;
                    break;
                }
                rp2040.fifo.push_nb(0xEF);
                Serial.println("NAV Handshake complete");
                return 0;
            }
            Serial.println("NAV Handshake failed");
            return 1;
        }

        void serialinit(){
            Serial.begin(115200);
            //Serial1.begin(115200);
            uint32_t serialstarttime = millis();
            while (!Serial && millis() - serialstarttime < 5000);
            delay(100);
            

            Serial.println("\n\nMP Serial init");
            //Serial1.println("\n\nMP Serial init");
            
            return;
        }

        int initsd(){
            if (!SD.begin(CS_SD))
            {
                Serial.println("SD init failure, card not present or not working");
                return 1;
            }
            Serial.println("SD card init succeess");
            return 0;
        }

        

        void setled(int color){
            switch (color)
            {
            case RED:
                digitalWrite(LEDRED, LOW);
                digitalWrite(LEDGREEN, HIGH);
                digitalWrite(LEDBLUE, HIGH);
                break;
            
            case GREEN:
                digitalWrite(LEDRED, HIGH);
                digitalWrite(LEDGREEN, LOW);
                digitalWrite(LEDBLUE, HIGH);
                break;

            case BLUE:
                digitalWrite(LEDRED, HIGH);
                digitalWrite(LEDGREEN, HIGH);
                digitalWrite(LEDBLUE, LOW);
                break;
            
            default:
                break;
            }
        }

};


class NAVCORE{
    
    

    public:
        NAVCORE(){};

        int sendpacket(navpacket datatosend){
            for (int i = 0; i < sizeof(datatosend.data)/sizeof(datatosend.data[0]); i++)
            {
                bool error = rp2040.fifo.push_nb(datatosend.data[i]);
                if (error == true)
                {
                    return 1;
                }
                
            }
            
            return 0;
        }

        int handshake(){
            uint32_t data;
            rp2040.fifo.push_nb(0xAA);
            data = rp2040.fifo.pop();
            if (data != 0xAB)
            {
                rp2040.fifo.push_nb(data);
                return 1;
            }
            rp2040.fifo.push_nb(0xCD);
            rp2040.fifo.pop();
            navpacket handshakepacket;
            handshakepacket.r.errorflag = 1;

            return 0;
        }

        int initi2c(){
            Wire1.setSCL(SCL);
            Wire1.setSDA(SDA);
            Wire1.setClock(10000);
            Wire1.begin();
            scani2c();
            return 0;
        }

        uint32_t sensorinit(){
            uint32_t errorflag;
            IMUinit();
            baroinit();
            maginit();
            return 0;
        }
};

#endif // GENERALLIB