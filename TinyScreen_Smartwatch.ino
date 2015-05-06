//-------------------------------------------------------------------------------
//  TinyCircuits TinyScreen/NRF8001 Smartwatch Example Sketch
//  Last Updated 3 April 2015
//  
//  This demo sets up the NRF8001 for Nordic's BLE virtual UART connection, then
//  accepts a date setting string with the format "Dyyyy MM dd k m s" or a
//  notification string starting with a 1 or 2(for line 1 or 2) followed by the 
//  short text string to display. Time, date, notifications are displayed, and a
//  simple sleep mode is implemented to save some power.
//
//  Written by Ben Rose, TinyCircuits http://Tiny-Circuits.com
//
//-------------------------------------------------------------------------------

#include <Wire.h>
#include <SPI.h>
#include <TinyScreen.h>
#include "lib_aci.h"
#include "aci_setup.h"
#include "uart_over_ble.h"
#include "services.h"
#include <Time.h>

#define BLE_DEBUG 1
uint8_t ble_rx_buffer[21];
uint8_t ble_rx_buffer_len = 0;

//when using this project in the Arduino IDE, delete the following include and rename UART.h to UART.ino
#include "UART.h"


TinyScreen display = TinyScreen(0);


unsigned long lastReceivedTime=0;

unsigned long lastDisplayUpdate=0;
unsigned long displayUpdateInterval=1000;


unsigned long sleepTimer=0;
unsigned long sleepTimeout=5000;
int displayState=0;
int buttonReleased=1;

char notificationLine1[20]=" Sync with phone";
char notificationLine2[20]="      to set time.";

void setup(void)
{
  Serial.begin(115200);
  Wire.begin();
  display.begin();
  display.setFlip(1);
  display.setBrightness(3);
  display.setFont(liberationSans_10ptFontInfo);
  BLEsetup();
  delay(500);
  requestScreenOn();
}

void loop() {
  aci_loop();//Process any ACI commands or events from the NRF8001- main BLE handler, must run often. Keep main loop short.
  if(ble_rx_buffer_len){
    if(ble_rx_buffer[0]=='D'){
      //expect date/time string- example: D2015 03 05 11 48 42
      lastReceivedTime=millis();
      updateTime(ble_rx_buffer+1);
      notificationLine1[0]='\0';
      memcpy(notificationLine2,"  No notifications.",strlen("  No notifications."));
      displayDate();
      displayTime();
      displayNotifications();
      requestScreenOn();
    }
    if(ble_rx_buffer[0]=='1'){
      memcpy(notificationLine1,ble_rx_buffer+1,ble_rx_buffer_len-1);
      notificationLine1[ble_rx_buffer_len-1]='\0';
      displayNotifications();
      requestScreenOn();
    }
    if(ble_rx_buffer[0]=='2'){
      memcpy(notificationLine2,ble_rx_buffer+1,ble_rx_buffer_len-1);
      notificationLine2[ble_rx_buffer_len-1]='\0';
      displayNotifications();
      requestScreenOn();
    }
    ble_rx_buffer_len=0;
  }
  
  if(displayState && millis() > lastDisplayUpdate+displayUpdateInterval){
  	//timeStatus()==timeSet
    lastDisplayUpdate=millis();
    displayDate();
    displayTime();
  }
  byte buttons = display.getButtons();
  if(buttonReleased && buttons){
  	if(displayState){
      notificationLine1[0]='\0';
      memcpy(notificationLine2,"  No notifications.",strlen("  No notifications."));
      displayNotifications();
  	}
    requestScreenOn();
    buttonReleased=0;
  }
  if(!buttonReleased && !(buttons&0x0F)){
    buttonReleased=1;
  }
  if(millis() > sleepTimer+sleepTimeout){
    if(displayState){
      displayState=0;
      display.off();
    }
  }
}

int requestScreenOn(){
  sleepTimer=millis();
  if(!displayState){
    displayState=1;
    displayDate();
    displayBattery();
    displayTime();
    displayNotifications();
    display.on();
    return 1;
  }
  return 0;
}

void updateTime(uint8_t * b){
  int y,M,d,k,m,s;
  char * next;
  y=strtol((char *)b,&next,10);
  M=strtol(next,&next,10);
  d=strtol(next,&next,10);
  k=strtol(next,&next,10);
  m=strtol(next,&next,10);
  s=strtol(next,&next,10);
  setTime(k,m,s,d,M,y);
}

void displayDate(){
  display.setFont(liberationSans_10ptFontInfo);
  display.setCursor(2,1);
  display.print(dayShortStr(weekday()));
  display.print(' ');
  display.print(monthShortStr(month()));
  display.print(' ');
  display.print(day());
  display.print("  ");
}

void displayTime(){
  display.setFont(liberationSans_22ptFontInfo);
  display.setCursor(0,14);
  int h=hour(),m=minute(),s=second();
  if(h<10)display.print('0');
  display.print(h);
  display.print(':');
  if(m<10)display.print('0');
  display.print(m);
  display.print(':');
  if(s<10)display.print('0');
  display.print(s);
  display.print(' ');
}

void displayNotifications(){
  display.setFont(liberationSans_10ptFontInfo);
  display.setCursor(0,37);
  display.print(notificationLine1);
  for(int i=0;i<40;i++)display.write(' ');
  display.setCursor(0,50);
  display.print(notificationLine2);
  for(int i=0;i<40;i++)display.write(' ');
}

void displayBattery(){
  //http://forum.arduino.cc/index.php?topic=133907.0
  const long InternalReferenceVoltage = 1100L;
  ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);
  delay(10);
  ADCSRA |= _BV( ADSC );
  while( ( (ADCSRA & (1<<ADSC)) != 0 ) );
  int result = (((InternalReferenceVoltage * 1024L) / ADC) + 5L) / 10L;
  //Serial.println(result);
  //if(result>440){//probably charging
  result=constrain(result-300,0,120);
  int x=72;
  int y=2;
  int height=8;
  int length=20;
  int amtActive=(result*length)/110;
  int red,green,blue;
  for(int i=0;i<length;i++){
    if(i<amtActive){
    red=63-((63/length)*i);
    green=((63/length)*i);
    blue=0;
    }else{
      red=32;
      green=32;
      blue=32;
    }
    display.drawLine(x+i,y,x+i,y+height,red,green,blue);
  }
}
