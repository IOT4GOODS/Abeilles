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

#define ATSIGFOXTX "AT$SS="
#define N 70 //Moyenne sur N valeurs entre deux envois
//#define SERIAL_DEBUG 1 // permet d'afficher des messages de debug 
#define ONE_WIRE_BUS 4

const byte statusLED = 13;  //Flashes with each reading
HX711 scale(DAT, CLK); //Setup interface to scale
OneWire oneWire(ONE_WIRE_BUS); // Setup interface for the DS18B20 temperature sensor
DallasTemperature sensors(&oneWire);// Setup interface for the DS18B20 temperature sensor

boolean FirstRead = true; // permet d'envoyer un message sigfox au démarrage du programme

unsigned int count = 0;
float currentReading=0;
float LocalTemperature=0;
float scalesum=0; // permet de faire l'accumulation du poids 
float tempsum=0; // permet de faire l'accumulation de la temperature exterieure
float RTempsum=0; // permet de faire l'accumulation de la temperature dans la ruche 
char first=1;
float scales ;
float temp ;
float rtemp ;
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
  scale.set_scale(11218); //Calibrate scale from EEPROM value
  scale.set_offset(84191); //Zero out the scale using a previously known zero point
}


void loop()
{
 // nouvelle mesure de poids de la ruche, temperature extérieure et temperature dans la ruche
 scale.power_up(); // sortie de veille de la balance
 currentReading = scale.get_units(4); //mesure du poids
 scale.power_down(); // mise en veille de la balance
 LocalTemperature = getLocalTemperature(); //mesure de la temperature exterieure
 sensors.requestTemperatures();  // lance une nouvelle mesure de temperature dans la ruche
 RemoteTemp=sensors.getTempCByIndex(0); // mesure de la temperature dans la ruche
 // pour faire une moyenne sur N points de mesure, on fait l'accumulation des points de mesures
 scalesum = scalesum + currentReading; // accumulation du poids
 tempsum = tempsum + LocalTemperature; // accumulation de la temperature exterieure
 RTempsum = RTempsum + RemoteTemp; // accumulation de la temperature dans la ruche 
 count++; 
 
 if(FirstRead) // pour la premiere lecture envoi d'un message sur le réseau Sigfox (test)
 {
    FirstRead = false; //     
    // correction de la mesure du poids en fonction de la temperature
    scale_comp = scales + A*(B-temp)*(scales/10);  // a revoir...
    //Pour l'envoi sur le réseau Sigfox il faut transformer les valeurs en chaine de carctere hexa
    poids_g_hex=uinttoHex((unsigned short)(scales*1000)); // poids non corrige en gramme
    poids_comp_g_hex = uinttoHex((unsigned short)(scale_comp*1000)); // poids corrige en gramme
    RTemps_hex = uinttoHex((short)(RemoteTemp*100)); 
    LTemps_hex = uinttoHex((short)(temp*100));
    // concatenation de toutes les données mesurées pour envoi sur le réseau Sigfox.
    msg = (String)ATSIGFOXTX + poids_g_hex + poids_comp_g_hex+ LTemps_hex + RTemps_hex; // Put everything together
    ATCommand = "";
    ATCommand.concat(msg);
    ATCommand.concat("\r\n");
    Serial.print(ATCommand); 
    Serial.flush();
 }
 // lorsque nous faisons l'acquisition de N points, on fait la moyenne et envoi sur le réseau Sigfox
 if(count>N) 
 {
  count=0;
  // calcuk des moyennes sur N points
  scales = scalesum/N;
  temp = tempsum/N;
  rtemp = RTempsum/N
   // correction de la mesure du poids en fonction de la temperature
  scale_comp = scales + A*(;B-temp)*(scales/10);
  //Pour l'envoi sur le réseau Sigfox il faut transformer les valeurs en chaine de carctere hexa
  poids_g_hex=uinttoHex((unsigned short)(scales*1000)); // temperature non compense en gramme
  poids_comp_g_hex = uinttoHex((unsigned short)(scale_comp*1000));
  RTemps_hex = uinttoHex((short)(RemoteTemp*100));
  LTemps_hex = uinttoHex((short)(temp*100));
  // concatenation de toutes les données mesurées pour envoi sur le réseau Sigfox.
  msg = (String)ATSIGFOXTX + poids_g_hex + poids_comp_g_hex+ LTemps_hex + RTemps_hex; // Put everything together
  ATCommand = "";
  ATCommand.concat(msg);
  ATCommand.concat("\r\n");
  Serial.print(ATCommand); 
  Serial.flush();
  scalesum = 0;
  tempsum =0;
  RTempsum = 0;
 } 
 //mise en veille du µC pendant 8 secondes
 LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
                SPI_OFF, USART0_OFF, TWI_OFF);

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



