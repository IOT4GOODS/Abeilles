
#include "HX711.h" //Library created by bogde
#include "openscale.h" //Contains EPPROM locations for settings
#include <Wire.h> //Needed to talk to on board TMP102 temp sensor
#include <EEPROM.h> //Needed to record user settings
#include <OneWire.h> //Needed to read DS18B20 temp sensors
#include <LowPower.h> //https://github.com/rocketscream/Low-Power
#include <DallasTemperature.h>
//15 ms, 30 ms, 60 ms, 120 ms, 250 ms, 500 ms, 1 s, 2 s, 4 s, 8 s, and forever

#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/power.h> //Needed for powering down perihperals such as the ADC/TWI and Timers
//#include <Akeru.h>

#define ATSIGFOXTX "AT$SS="
#define N 70 //nombre de mesure moyénné avant envoi.
#define A -0.0553 // coefficient de correction
#define B 10.063  // correction

//#define SERIAL_DEBUG 1
#define ONE_WIRE_BUS 4

const byte statusLED = 13;  //Flashes with each reading

HX711 scale(DAT, CLK); //Setup interface to scale
//OneWire remoteSensor(4);  //Setup reading one wire temp sensor on pin 4 (a 4.7K resistor is necessary)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//byte remoteSensorAddress[8];
//boolean remoteSensorAttached = true;
boolean FirstRead = true; // permet d'envoyer un message sigfox au démarrage du programme

unsigned int count = 0;
float currentReading=0;
float LocalTemperature=0;
float scalesum=0;
float tempsum=0;
float RTempsum=0;
char first=1;
float scales ;
float temp ;
float scale_comp;
String LTemps_hex;
String RTemps_hex;
String poids_g_hex;
String poids_comp_g_hex;
String msg;
String ATCommand ;
float RemoteTemp;


void setup()
{
  pinMode(statusLED, OUTPUT);

  //During testing reset everything
  //for(int x = 0 ; x < 30 ; x++)
  //{
  //  EEPROM.write(x, 0xFF);
  //}
  
  Wire.begin();
  sensors.begin();

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Power down everything, wake up from WDT
  sleep_enable();

  //Shut off Timer2, Timer1, ADC
  ADCSRA &= ~(1<<ADEN); //Disable ADC
  ACSR = (1<<ACD); //Disable the analog comparator
  DIDR0 = 0x3F; //Disable digital input buffers on all ADC0-ADC5 pins
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); //Disable digital input buffer on AIN1/0

  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();
  power_spi_disable();

  pinMode(AMP_EN, OUTPUT);
  digitalWrite(AMP_EN, LOW); //Turn on power to HX711
  //Setup UART
  Serial.begin(9600);

  #ifdef SERIAL_DEBUG
    Serial.println(F("Test1"));
  #endif
  scale.set_scale(11218); //Calibrate scale from EEPROM value
  scale.set_offset(84191); //Zero out the scale using a previously known zero point
  #ifdef SERIAL_DEBUG
    Serial.println(F("Test2"));
  #endif
}


void loop()
{
 if(count>N) 
 {
  count=0;
  scales = scalesum/N;
  temp = tempsum/N;
  scale_comp = scales + A*(B-temp)*(scales/10);
   
  poids_g_hex=uinttoHex((unsigned short)(scales*1000)); // temperature non compense en gramme
  poids_comp_g_hex = uinttoHex((unsigned short)(scale_comp*1000));
  sensors.requestTemperatures(); 
  RemoteTemp=sensors.getTempCByIndex(0);
  RTemps_hex = uinttoHex((short)(RemoteTemp*100));
  LTemps_hex = uinttoHex((short)(temp*100));
  msg = (String)ATSIGFOXTX + poids_g_hex + poids_comp_g_hex+ LTemps_hex + RTemps_hex; // Put everything together
  
  ATCommand = "";
  ATCommand.concat(msg);
  ATCommand.concat("\r\n");
  //Serial.print((String)"\n>> " + ATCommand);
  Serial.print(ATCommand); 
  Serial.flush();
  scalesum = 0;
  tempsum =0;
  RTempsum = 0;
 } 
 LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
                SPI_OFF, USART0_OFF, TWI_OFF);
 scale.power_up();
 currentReading = scale.get_units(4);
 scalesum = scalesum + currentReading;
 scale.power_down();
 LocalTemperature = getLocalTemperature();
 tempsum = tempsum + LocalTemperature;
 sensors.requestTemperatures(); 
 RemoteTemp=sensors.getTempCByIndex(0);
 RTempsum = RTempsum + RemoteTemp;
 count++; 
 
 //RemoteTemp = getRemoteTemperature();
 if(FirstRead)
 {
    FirstRead = false;
    scales = currentReading;
    temp = getLocalTemperature();
    // correction de la mesure du poids en fonction de la temperature
    scale_comp = scales + A*(B-temp)*(scales/10); 
    poids_g_hex=uinttoHex((unsigned short)(scales*1000)); // temperature non compense en gramme
    poids_comp_g_hex = uinttoHex((unsigned short)(scale_comp*1000));
    sensors.requestTemperatures(); 
    RemoteTemp=sensors.getTempCByIndex(0);
    RTemps_hex = uinttoHex((short)(RemoteTemp*100));
    LTemps_hex = uinttoHex((short)(temp*100));
    
    msg = (String)ATSIGFOXTX + poids_g_hex + poids_comp_g_hex+ LTemps_hex + RTemps_hex; // Put everything together
    ATCommand = "";
    ATCommand.concat(msg);
    ATCommand.concat("\r\n");
    Serial.print(ATCommand); 
    Serial.flush();
 }
 #ifdef SERIAL_DEBUG
    Serial.println(F("Readings: Poids="));
    Serial.print(currentReading, 2);
    Serial.print(F("kg"));
    Serial.print(F(",LocalTemp="));
    Serial.print(LocalTemperature, 2);
    Serial.print(F(",RemoteTemp="));
    Serial.print(RemoteTemp, 2);
    Serial.println();
    Serial.flush();
    Serial.println(F("Moyenne:"));
    Serial.print(scalesum/count, 2);
    Serial.print(F("kg"));
    Serial.print(F(","));
    Serial.print(tempsum/count, 2);
    Serial.print(F(",count:"));
    Serial.print(count);
    Serial.println();
    Serial.flush();
  #endif
}

//Read the on board TMP102 digital temperature sensor
//Return celsius
//Code comes from bildr
float getLocalTemperature()
{
  Wire.requestFrom(tmp102Address, 2);

  byte MSB = Wire.read();
  byte LSB = Wire.read();

  //It's a 12bit int, using two's compliment for negative
  int TemperatureSum = ((MSB << 8) | LSB) >> 4;

  float celsius = TemperatureSum * 0.0625;
  return celsius;
}

String inttoHex(int i)
{
  byte * b = (byte*) & i;
  
  String bytes = "";
  for (int j=0; j<2; j++)
  {
    if (b[j] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[j], 16));
  }
  return bytes;
}

String uinttoHex(unsigned int ui)
{
  byte * b = (byte*) & ui;
  
  String bytes = "";
  for (int i=0; i<2; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String longtoHex(long l)
{
  byte * b = (byte*) & l;
  
  String bytes = "";
  for (int i=0; i<4; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String ulongtoHex(unsigned long ul)
{
  byte * b = (byte*) & ul;
  
  String bytes = "";
  for (int i=0; i<4; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String floattoHex(float f)
{
  byte * b = (byte*) & f;

  String bytes = "";
  for (int i=0; i<4; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String doubletoHex(double d)
{
  byte * b = (byte*) & d;

  String bytes = "";
  for (int i=0; i<4; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}

String chartoHex(char c)
{
  byte *b = (byte*) & c;
  
  String bytes = "";
  if (b[0] <= 0xF) // single char
  {
    bytes.concat("0"); // add a "0" to make sure every byte is read correctly
  }
  bytes.concat(String(b[0], 16));
  return bytes;
}

String stringtoHex(char *c, int length)
{
  byte * b = (byte*) c;
  
  String bytes = "";
  for (int i=0; i<length; i++)
  {
    if (b[i] <= 0xF) // single char
    {
      bytes.concat("0"); // add a "0" to make sure every byte is read correctly
    }
    bytes.concat(String(b[i], 16));
  }
  return bytes;
}



