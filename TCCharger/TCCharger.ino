//Based on Code from Lennart O.

//
//#include <CAN.h>
#include <mcp_can.h> 
#include <SPI.h>
#include <SimpleTimer.h>

// #define Serial Serial1 // uncomment if you use Thinary Nano every 4808 "USB serial hack"
#define SPI_CS_PIN 10 //CS Pin = 10 when using arduino nano
//#define SPI_CS_PIN 8 //CS Pin = 8 when using Thinary Nano every 4808
// Basically, as long as you identify the SPI interface pins and connect the MOSI, MISO and SCK the SPI library
// should figure out the correct config autmagically for those signals. Only the CS pin and INT pin might have 
// to be changed depending on which pins are available.
// Connect INT pin of can bus card to D2 when using Thinary Nano every 4808 or arduino nano. 
// D2 will probably work for most arduino boards but I have only tested on the two above
// INT is used for receiving messages from the can bus interface

// Potentiometers should be connected to +5V, anlog pin and GND.
#define AMP_ADJ 0 //Potentiometer connected to A0 for adjusting charge current
#define VOLT_ADJ 1 //Potentiometer connected to A1 for setting target SOC voltage 
#define MAX_VOLTAGE 1162 // set max voltage to 116,2V (offset = 0,1)

//***********************************************************
// select max charge current based on which charger you have (offset = 0,1)
#define MAX_CURRENT 160 // set max charge current to 16A (TC1800)
//#define MAX_CURRENT 320 // set max charge current to 32A (TC3300)
//***********************************************************

word outputcurrent = 0; // initial current when starting up
word outputvoltage = MAX_VOLTAGE; // set initial voltage to MAX_VOLTAGEV (offset = 0,1)

float ampadj = 0;
float voltadj = MAX_VOLTAGE;

unsigned long int sendId = 0x1806E5F4;
unsigned char voltamp[8] = {highByte(outputvoltage), lowByte(outputvoltage), highByte(outputcurrent), lowByte(outputcurrent), 0x00,0x00,0x00,0x00};

unsigned char len = 0; // Length of CAN recieve message 
unsigned char buf[8]; //Buffer for data from CAN message
unsigned long int receiveId; //Senders ID 

//Object declarations 

MCP_CAN CAN(SPI_CS_PIN); //Set CS Pin for SPI

SimpleTimer timer1; //Provide timer Object

//Functions

/************************************************
** Function name:           canRead
** Descriptions:            read CAN message
*************************************************/

void canRead(){

  if(CAN_MSGAVAIL == CAN.checkReceive()){ //check for message

    CAN.readMsgBuf(&len, buf); // read data, len: data length, buf: data buffer

    receiveId = CAN.getCanId(); //Read CAN-ID

    if(receiveId == 0x18FF50E5){ //CAN Bus ID from charger

      Serial.println("CAN Data from charger received!");

      Serial.print("CAN ID: ");
      Serial.print(receiveId, HEX); //Print ID

      Serial.print(" / CAN data: ");
      for(int i = 0; i<len; i++){ // print the data

        if( buf[i] < 0x10){ // add leading zero if only one digit value
          Serial.print("0");
        }

        Serial.print(buf[i],HEX);
        Serial.print(" ");          // Space

      }

      Serial.println(); //New line

      Serial.print("Charge voltage: ");
      float pv_voltage = (((float)buf[0]*256.0) + ((float)buf[1]))/10.0; //highByte/lowByte + offset
      Serial.print(pv_voltage);
      Serial.print(" V / Charge current: ");
      float pv_current = (((float)buf[2]*256.0) + ((float)buf[3]))/10.0; //highByte/lowByte + offset
      Serial.print(pv_current);
      Serial.println(" A"); //New line

      switch (buf[4]) { // Read error code

        case 0b0000001: Serial.println("Error: Hardware fail");break;
        case 0b0000010: Serial.println("Error: Overheating");break;
        case 0b0000100: Serial.println("Error: Mains voltage out of spec.");break;
        case 0b0001000: Serial.println("Error: Battery not connected");break;
        case 0b0010000: Serial.println("Error: CAN-Bus error");break;
        case 0b0001100: Serial.println("Error: Mains disconnected");break;

      }

    }

  }

}

/************************************************
** Function name:           canWrite
** Descriptions:            write CAN message
*************************************************/

String canWrite(unsigned char data[8], unsigned long int id){

  byte sndStat = CAN.sendMsgBuf(id, 1, 8, data); // Send message (ID, extended Frame, data lenght, Data)

  if(sndStat == CAN_OK) //Statusbyte for transmission
    return "CAN message transmission OK";
  else
    return "Transmission error";

}

/************************************************
** Function name:           setVoltage
** Descriptions:            set target voltage
*************************************************/

void setVoltage(int t_voltage) { //can be used to set desired voltage to i.e. 80% SOC

  if(t_voltage >= 980 && t_voltage <= MAX_VOLTAGE){
    
    outputvoltage = t_voltage;
    
  }

 }

/************************************************
** Function name:           readVoltAdj
** Descriptions:            read voltadj to adjust voltage
*************************************************/

void readVoltAdj() {
// 99,12 - 116,2 

  voltadj = analogRead(VOLT_ADJ); // Read value from VoltAdj potentiometer connected to A1
  Serial.print("volt: ");
  Serial.println(voltadj);
  // some clever math to have minimum target voltage = 99.1V and max target voltage = MAX_VOLTAGE corresponding to target SOC from aproximatly 35% to 100%. 
  // Note the scale isn't linear so marking the potentiometer should be done by setting a position and take note of SOC when charging stops
  int voltage = (int) (MAX_VOLTAGE*((voltadj+5944)/6958));
    
  if(voltage > MAX_VOLTAGE){

   voltage = MAX_VOLTAGE; // ensure that charge voltage doesn't go over MAX_VOLTAGE

  }  
  setVoltage(voltage); 

 }

/************************************************
** Function name:           setCurrent
** Descriptions:            set target current
*************************************************/

void setCurrent(int t_current) { //can be used to reduce or adjust charging speed

  if(t_current >= 0 && t_current <= MAX_CURRENT){
    
    outputcurrent = t_current;
    
  }

 }

/************************************************
** Function name:           readAmpAdj
** Descriptions:            read ampadj to adjust current
*************************************************/

void readAmpAdj() {

  ampadj = analogRead(AMP_ADJ); // Read value from AmpAdj potentiometer connected to A0
  Serial.print("amp: ");
  Serial.println(ampadj);
  int current = (int) (MAX_CURRENT*((ampadj+200)/1120.0)); // some clever math to have minimum charge curret > 0A.
    
  if(current > MAX_CURRENT){

   current = MAX_CURRENT; // ensure that charge current doesn't go over MAX_CURRENT

  }  
  setCurrent(current); 

 }

/************************************************
** Function name:           myTimer1
** Descriptions:            function of timer1
*************************************************/

void myTimer1() { // Function called repeatedly by the timer

  Serial.print("Charge current setting: ");
  Serial.print((float)outputcurrent/10.0); // Print the current setting
  Serial.println(" A");
  Serial.print("Voltage target setting: ");
  Serial.print((float)outputvoltage/10.0); // Print the current setting
  Serial.println(" V");
  
  unsigned char voltamp[8] = {highByte(outputvoltage), lowByte(outputvoltage), highByte(outputcurrent), lowByte(outputcurrent), 0x00,0x00,0x00,0x00}; // Prepare new CAN bus message
  Serial.println(canWrite(voltamp, sendId)); // Send message and print the result
  
  canRead(); // Call the read CAN function
  readAmpAdj(); // Read the current adjust potentiometer (comment this out if you don't have a current adjust potentiometer connected)
  readVoltAdj(); // Read the voltage adjust potentiometer (comment this out if you don't have a voltage adjust potentiometer connected)
  Serial.println(); //New line

}

/************************************************
** Function name:           setup
** Descriptions:            Arduino setup
*************************************************/

void setup() {

  Serial.begin(115200); // Start serial interface

  while(CAN_OK != CAN.begin(CAN_250KBPS, MCP_8MHz)){ //CAN Bus initialization

    Serial.println("CAN Initialization error, Retrying...");
    delay(200);

  }

  Serial.println("CAN Initialization successful");

  timer1.setInterval(950, myTimer1); // Configre the timer function

}

/************************************************
** Function name:           loop
** Descriptions:            Arduino loop
*************************************************/

void loop() {

  timer1.run(); //Start the timer function

}
