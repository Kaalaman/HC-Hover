//Version 1.0.0
// Bibliotheken einbinden
#include <Arduino.h>
#include <SPI.h>
#include "RF24.h"
#include "printf.h"
#include <Servo.h>
#include <LiquidCrystal.h>

#include <Wire.h>


//
//---Anschlüsse und Konstante Festlegen--------------------------------------------
//
#define BATTERIE_1 A1
#define BATTERIE_2 A2
#define BATTERIE_3 A3

//
//---Angeschlossene Obekte definieren----------------------------------------------
//
Servo SteuerServo;
Servo AuftriebESC;
Servo SchubESC;

LiquidCrystal lcd (8, 7, 6, 5, 4, 3);
RF24 hcRadio (48 , 53); // "hcRadio" is the identifier you will use in following methods
//
//---Globale-Variablen deklarieren -------------------------------------------------
//
byte addresses[][6] = {"1Node", "2Node"}; // Create address for 1 pipe.

// Minimale und Maxima le Werte für Motoren und Servo
byte MotorMin = 17;
byte MotorGrenze = 22;
byte SchubMax = 55;
byte AuftriebMax = 80;
byte SKM = 84;
byte SKA = 27;



//Für I2C-Kommunikation
const byte MASTER_ADRESS = 42;
const byte SLAVE_DISPLAY_ADRESS = 24;

//Alle Variablen für Zeitnehmung
unsigned long timer;
unsigned long timerBatterie;
unsigned long timerRunde = 0;


byte LeistungMotorSchub;
byte LeistungMotorAuftrieb;
byte StellungSteuerServo;


//TEst Datenübertragung
//volatile byte buf [2];
//
struct infoStruct {
  int BatU;
  //int Strom Motor 1;
  //int Strom Motor 2;
} hcInfo;


struct dataStruct {
  int Spoti;
  int Xposition;          // The Joystick position values
  int Yposition;
  bool switchOn;          // The Joystick push-down switch
  int zaehler;
} myData;

//
//--- Setup, wird einmal durchlaufen----------------------------------------------
//
void setup()   /****** SETUP: RUNS ONCE ******/
{
  Serial.begin(115200);

  SteuerServo.attach(30);  //Servo für Steuerklappen auf Pin 30 legen
  AuftriebESC.attach(31);  // ESC für Auftriebsmotor wird an Pin 31 abgeschlossen
  SchubESC.attach(32); // ESC für Motor-Schub an Pin 32

  Wire.begin();  //Vorbereitung für I2C

  Serial.println(F("START"));
  myData.zaehler = 0; // Arbitrary known data to transmit. Change it to test...

  hcRadio.begin();  // Start up the physical nRF24L01 Radio
  hcRadio.setChannel(108);  // Above most Wifi Channels
  hcRadio.setPALevel(RF24_PA_LOW);
  hcRadio.setRetries(2,15);
  hcRadio.openWritingPipe( addresses[0]);
  hcRadio.openReadingPipe(1,addresses[1]);

  hcRadio.enableAckPayload();
  hcRadio.enableDynamicPayloads();

  //hcRadio.writeAckPayload(1,&counter,1);          // Pre-load an ack-paylod into the FIFO buffer for pipe 1
  //hcRadio.setDataRate(RF24_2MBPS);
  //hcRadio.setCRCLength(RF24_CRC_8);

  lcd.clear();
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.print("Nr.: ");  //11
  lcd.setCursor (0,1);
  lcd.print("len= ");  //3
  lcd.setCursor (0,2);
  lcd.print("Auf= ");  //3
  lcd.setCursor (0,3);
  lcd.print("Sch= ");  //3
  lcd.setCursor (11,2);
  lcd.print("PL:");


}//Ende Setup



//
//---Schleife, Loop. Wird andauernt durchlaufen. Immer im Kries-------------------------
//
void loop()
{
  //timerRunde = micros();
  hcRadio.stopListening();

  Serial.print("\n");

  if (hcRadio.write( &hcInfo, sizeof (hcInfo))) {
    if (!hcRadio.available()) {
      Serial.print(F("Leere Rückmeldung. Rundendauer:"));
      Serial.print(micros()-timerRunde);
      Serial.print(" Microsekunden");

      timerRunde = micros();
    }

    else {
      while (hcRadio.available()) {
        hcRadio.read( &myData, sizeof (myData));

        timer = micros();
      } // Ende While - solange Daten von Ferbedienung kommen

     Serial.print(F("Rückmeldung: X = "));
     Serial.print(myData.Xposition);
     Serial.print(F(" Y="));


     if (myData.Yposition < 520) {
       myData.Yposition = 520;
     }
     Serial.print(myData.Yposition);

     if ( myData.switchOn == 1){  //Ausgabe ob Switch gedrück ist oder ed
       Serial.print(F(" ON "));}
     else{
       Serial.print(F(" OFF "));}

     Serial.print(myData.zaehler);
     //lcd.setCursor(4, 0); lcd.print(myData.zaehler); lcd.print("    ");

    //Ausgabe der Rundendauer
     Serial.print(F(" - Rundendauer: "));
     Serial.print(timer - timerRunde);
     Serial.print(F("µs"));

     timerRunde = micros();

   }//ENDE Else bei erfolgreicher Übertragung und einer Rückmekdung (Steuerdsaten ) von Ferbedienung

 }//ENDE Else bei erfolgreicher Übertragung (egal ob mit oder Ohne Steuerdsaten-Rückmeldung)
  else {
    Serial.print(F("Fehler: Safe-Fail: "));

    myData.Spoti = 0;
    myData.Xposition = 520;
    myData.Yposition = 0;
    myData.switchOn  = 0;

    Serial.print(F(" X = "));
    Serial.print(myData.Xposition);
    Serial.print(F(" Y="));
    Serial.print(myData.Yposition);

    if ( myData.switchOn == 1){  //Ausgabe ob Switch gedrück ist oder ed
      Serial.print(F(" ON "));}
    else{
      Serial.print(F(" OFF "));}


  }//ENDE Else, bei keiner Übertragung


  //Code zum Ansteuern der Motoren/ Servos --> muss immer
  //(unabhängig von Übertragung) ausgeführt werden
  Serial.print(F(" Mot-Daten: "));

  LeistungMotorSchub = map(myData.Yposition, 520, 1000, MotorMin, SchubMax);
  if (LeistungMotorSchub < MotorGrenze) {    LeistungMotorSchub = MotorMin ;  }
  LeistungMotorAuftrieb = map(myData.Spoti, 0, 1023, MotorMin, AuftriebMax);
  if (LeistungMotorAuftrieb < MotorGrenze) { LeistungMotorAuftrieb = MotorMin ;}
  StellungSteuerServo = map(myData.Xposition , 20, 1008, 111, 57);

  Serial.print(LeistungMotorSchub);
  Serial.print(" - ");
  Serial.print(LeistungMotorAuftrieb);
  Serial.print(" - ");
  Serial.print(StellungSteuerServo);

  SchubESC.write(LeistungMotorSchub);
  AuftriebESC.write(LeistungMotorAuftrieb);
  SteuerServo.write(StellungSteuerServo);

  //Inaktiv wegen Rundenzeit
  lcd.setCursor(4, 1); lcd.print(StellungSteuerServo); lcd.print("   ");
  lcd.setCursor(4, 2); lcd.print(LeistungMotorAuftrieb); lcd.print("   ");
  lcd.setCursor(4, 3); lcd.print(LeistungMotorSchub); lcd.print("   ");


  //Spannung an Batterie auslesen
  if (millis() - timerBatterie > 500) {

    hcInfo.BatU = random(820, 1023);

    Serial.print("Batterie: ");
    Serial.print(hcInfo.BatU);
    Serial.print(F(" - "));

    timerBatterie = millis();
  }

}//Ende Loop
