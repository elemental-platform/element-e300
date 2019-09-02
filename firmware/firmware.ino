// Element E300 v1.0.4 firmware

// Developed by AKstudios
// Updated: 07/10/2019

#include <RFM69.h>  //  https://github.com/LowPowerLab/RFM69
#include <SPI.h>
#include <Arduino.h>
#include <Wire.h> 
#include <SoftwareSerial.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

// define node parameters
//#define NODEID              23
uint16_t NODEID =             23; // same as above, but supports 10bit addresses (up to 1023 node IDs)
#define NETWORKID           20
#define ROOM_GATEWAYID      20
#define GATEWAYID           1
#define GATEWAY_NETWORKID   1
#define FREQUENCY     RF69_915MHZ //Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define ENCRYPTKEY    "Tt-Mh=SQ#dn#JY3_" //has to be same 16 characters/bytes on all nodes, not more not less!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define LED           9 // led pin
#define POWER         4

// define objects
RFM69 radio;

// define S8 global variables
SoftwareSerial mySerial(6, 5); // RX, TX
byte readCO2[] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};
byte response[] = {0,0,0,0,0,0,0}; //create an array to store the response

char dataPacket[150];
int wake_interval = 0;

ISR(WDT_vect)  // Interrupt Service Routine for WatchDog Timer
{
  wdt_disable();  // disable watchdog
}


void setup()
{
  pinMode(10, OUTPUT); // Radio SS pin set as output

  Serial.begin(115200);
  Serial.println("Setup");

  pinMode(POWER, OUTPUT);
  pinMode(LED, OUTPUT);  // pin 9 controls LED
  
  mySerial.begin(9600);

  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);

  fadeLED();
  Serial.println("Ready");
  delay(10);
}


void sleep()
{
  Serial.flush(); // empty the send buffer, before continue with; going to sleep
  radio.sleep();
  digitalWrite(POWER, LOW);
  delay(1);
  
  cli();          // stop interrupts
  MCUSR = 0;
  WDTCSR  = (1<<WDCE | 1<<WDE);     // watchdog change enable
  WDTCSR  = 1<<WDIE | (1<<WDP3) | (0<<WDP2) | (0<<WDP1) | (1<<WDP0); // set  prescaler to 8 second
  sei();  // enable global interrupts

  byte _ADCSRA = ADCSRA;  // save ADC state
  ADCSRA &= ~(1 << ADEN);

  asm("wdr");
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();       

  sleep_enable();  
  //sleep_bod_disable();
  sei();       
  sleep_cpu();   
    
  sleep_disable();   
  sei();  

  ADCSRA = _ADCSRA; // restore ADC state (enable ADC)
  delay(1);
}


void loop() 
{
  sleep(); 
  
  if(wake_interval == 4)    // if enough time has passed (~54 seconds), take measurements and transmit
  {
    readSensors();
    
    Serial.println(dataPacket);
    delay(10);
    
    radio.sendWithRetry(ROOM_GATEWAYID, dataPacket, strlen(dataPacket));  // send data

    dataPacket[0] = (char)0; // clearing first byte of char array clears the array
  
    digitalWrite(LED, HIGH);
    delay(5);
    digitalWrite(LED, LOW);
    //fadeLED();

    wake_interval = 0;    // reset wake interval to 0
  }
  else
    wake_interval++;    // increment no. of times node wakes up
}


void readSensors()
{
  while(!mySerial.available()) //keep sending request until we start to get a response
  {
   mySerial.write(readCO2, 7);
   delay(10);
  }
  
  int timeout=0; //set a timeout counter
  while(mySerial.available() < 7 ) //Wait to get a 7 byte response
  {
   timeout++;
   if(timeout > 10) //if it takes too long there was probably an error
   {
     while(mySerial.available()) //flush whatever we have
     mySerial.read();
     break; //exit and try again
   }
   delay(50);
  }
  
  for (int i=0; i < 7; i++)
  {
    response[i] = mySerial.read();
  }

  int high = response[3]; //high byte for value is 4th byte in packet in the packet
  int low = response[4]; //low byte for value is 5th byte in the packet
  unsigned long val = high*256 + low; //Combine high byte and low byte with this formula to get value
  
  // define character arrays for all variables
  char _i[3];
  char _c[7];
  
  // convert all flaoting point and integer variables into character arrays
  dtostrf(NODEID, 1, 0, _i);
  dtostrf(val, 1, 0, _c); 
  delay(10);
  
  dataPacket[0] = 0;  // first value of dataPacket should be a 0
  
  // create datapacket by combining all character arrays into a large character array
  strcat(dataPacket, "i:");
  strcat(dataPacket, _i);
  strcat(dataPacket, ",c:");
  strcat(dataPacket, _c);
  delay(10);
}


void fadeLED()
{
  int brightness = 0;
  int fadeAmount = 5;
  for(int i=0; i<510; i=i+5)  // 255 is max analog value, 255 * 2 = 510
  {
    analogWrite(9, brightness);  // pin 9 is LED
  
    // change the brightness for next time through the loop:
    brightness = brightness + fadeAmount;  // increment brightness level by 5 each time (0 is lowest, 255 is highest)
  
    // reverse the direction of the fading at the ends of the fade:
    if (brightness <= 0 || brightness >= 255)
    {
      fadeAmount = -fadeAmount;
    }
    // wait for 20-30 milliseconds to see the dimming effect
    delay(10);
  }
  digitalWrite(9, LOW); // switch LED off at the end of fade
}

// bruh