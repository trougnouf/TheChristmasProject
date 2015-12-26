/*
 * The Christmas Project
 * by Benoit "trougnouf" Brummer
 * 
 * Two columns of 6 snowflakes fall down (12 white LEDs),
 * snowman forms as snow falls (3 white LEDs on PWM, top layer dims)
 * when snowman is formed, snow stops, sun appears, snowman melts as sun brightens (1 yellow LED on PWM)
 * 
 * States:
 *  snowing:
 *    forming snowman. ends when snowman is complete (smTopVal = GBWMAXDC), switch to sunrise
 *  sunrise:
 *    sun brightens, all 3 parts of snowman start melting, ends when sunVal = YELLOWMAXDC
 *  sunset:
 *    sun dims, all 3 parts of snowman end melting, ends when sunVal = 0, switch to snowing
 *    
 *  sky just randomly changes intensity
 *  
 *  Arduino IDE does not use standard ATMEGA pin names and PB6 and PB7 are not defined by default
 *  

Pinout:

                --------
PWM D9  PB1  15|SKY  LF0|14 PB0 D8
PWM D10 PB2  16|SUN  LF1|13 PD7 D7
PWM D11 PB3  17|x    SMt|12 PD6 D6 PWM
    D12 PB4  18|LF2  SMm|11 PD5 D5 PWM
    D13 PB5  19|LF3     |10 PB7      // modify Arduino library to use PB7
        AVCC 20|+       |9  PB6      // modify Arduino library to use PB6
        AREF 21|       -|8  GND
        GND  22|-      +|7  VCC
    A0  PC0  23|RF0  LF4|6  PD4 D4
    A1  PC1  24|RF1  SMb|5  PD3 D3 PWM
    A2  PC2  25|RF2  LF5|4  PD2 D2
    A3  PC3  26|RF3     |3  PD1 D1 P // probably safe to use
    A4  PC4  27|RF4     |2  PD0 D0 P // probably safe to use
    A5  PC5  28|RF5     |1  PC6    P // probably safe to use
                ---/\---
unsigned char flakePins[] = {8, 7, 12, 13, 4, 2, A0, A1, A2, A3, A4, A5};
*/

#include <avr/wdt.h>

#define SUNPIN  10
#define SKYPIN  9
#define SMBOTPIN  3
#define SMMIDPIN  5
#define SMTOPPIN  6
// sane values: GRAVITY = 200, SUNTIMER = 2000. testing values: 10, 50
#define GRAVITY 200
#define SUNTIMER 2000


/*
 * max duty cycle:
 * Yellow: 2.1 V / 5V = 107.1 / 255
 * Green/blue/white: 3.4 V / 5 V =  173.4
 * 
 * Using a voltmeter on the PWM (which should not work),  225/255 = 1.89V (yellow)
 *                                                        
 */
/*
// safe values
#define YELLOWMAXDC 107
#define GBWMAXDC  173
*/
// danger zone (= 250)
#define YELLOWMAXDC 225
#define GBWMAXDC  225

unsigned char flakePins[] = {8, 7, 12, 13, 4, 2, A0, A1, A2, A3, A4, A5};
unsigned char swPWMList[] = {0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,1}; // for green, blue, and white LED, used on white flakes
unsigned char activeFlakeLeft, activeFlakeRight;
unsigned char curSWPWM;

typedef enum {SNOWING, SUNRISE, SUNSET} weather;
weather curWeather;

unsigned char skyIntensity;
unsigned long timeToFallL;
unsigned long timeToFallR;
unsigned long timeToUpdateSky;
unsigned short snowmanLevel;
unsigned char sunshineLevel;


void setup() {
  /*
   * Green, Blue, White LED: 3.2 - 3.4 V
   * 3.4 / 5 = 17/25
   * {0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,1}
   * 
   * Max DC current to GND pin: 200.0mA: drive 10 LEDs w/o using GPIO as ground.
   * Max DC current per I/O pin: 40.0mA
   */
  wdt_enable(WDTO_8S);
  activeFlakeLeft = 6, activeFlakeRight = 12;



  int i;
  for(i=0;i<12; i++)  pinMode(flakePins[i], OUTPUT);
  
  pinMode(SUNPIN, OUTPUT);
  pinMode(SKYPIN, OUTPUT);
  pinMode(SMBOTPIN, OUTPUT);
  pinMode(SMMIDPIN, OUTPUT);
  pinMode(SMTOPPIN, OUTPUT);

  // arbitrary starting values
  curSWPWM = 0;
  curWeather = SNOWING;
  skyIntensity = GBWMAXDC;
  //timer = 0;
  timeToFallL = 0, timeToFallR = 0, timeToUpdateSky = 0;
  snowmanLevel = 0;
  sunshineLevel = 0;
  analogWrite(SKYPIN, 167);
  Serial.begin(9600);
}

void loop() {
  // TESTING: turn everything on
  /*
    analogWrite(SUNPIN, 200);
    analogWrite(SKYPIN, 200);
    analogWrite(SMTOPPIN, 200);
    analogWrite(SMMIDPIN, 200);
    analogWrite(SMBOT/PIN, 200);
    int i;
    for(i=0; i<16; i++)
    {
      digitalWrite(flakePins[i], swPWMList[curSWPWM]);
      curSWPWM = (curSWPWM == 24)?0:curSWPWM+1;
    }
  */

  
  //wdt_reset();
  switch(curWeather)
  {
    case SNOWING:
    snow();
    break;
    
    case SUNRISE:
    riseSun();
    break;
    
    case SUNSET:
    setSun();
    break;
    
    default:
    errorOut();
    break;
  }
  
  if(millis() > timeToUpdateSky)  updateSky();
  //wdt_reset();
  

}

void snow()
{
  // software PWM on active flake
  if(activeFlakeLeft != 6) digitalWrite(flakePins[activeFlakeLeft], swPWMList[curSWPWM]);
  if(activeFlakeRight != 12) digitalWrite(flakePins[activeFlakeRight], swPWMList[curSWPWM]);
  curSWPWM = (curSWPWM == 24)?0:curSWPWM+1; // update swPWM:

  // falling left
  if(millis() >= timeToFallL)
  {
    //wdt_reset();
    timeToFallL = millis() + GRAVITY;
    if(activeFlakeLeft <= 5)
    {
      digitalWrite(flakePins[activeFlakeLeft], LOW);
      activeFlakeLeft++;
      if(activeFlakeLeft == 6)
      {
        timeToFallL += random(0, GRAVITY*10);
        enlargeSnowman();
        wdt_reset();
      }
    }
    else  activeFlakeLeft = 0;
  }
  // falling right
  if(millis() >= timeToFallR)
  {
    //wdt_reset();
    timeToFallR = millis() + GRAVITY;
    if(activeFlakeRight <= 11)
    {
      digitalWrite(flakePins[activeFlakeRight], LOW);
      activeFlakeRight++;
      if(activeFlakeRight == 12)
      {
        enlargeSnowman();
        timeToFallR += random(0, GRAVITY*3);
        wdt_reset();
      }
    }
    else  activeFlakeRight = 6;
  }

  if(snowmanLevel >= GBWMAXDC*3)
  {
    Serial.print("\nSwitching to SUNRISE\n");
    int i;
    for(i=0; i<12; i++) digitalWrite(flakePins[i], LOW);  // turn all flakes off
    curWeather = SUNRISE;
  }
  /*
   * if time to fall L: fall down, repeat w/Right
   *  if all the way down, enlargeSnowman()
   */

   
}

void riseSun()
{
  //wdt_reset();
  shrinkSnowman();
  if(sunshineLevel <= YELLOWMAXDC)
  {
    sunshineLevel++;
    analogWrite(SUNPIN, sunshineLevel);
    Serial.print(sunshineLevel);
    Serial.print(" ");
  }
  if(snowmanLevel <= sunshineLevel)
  {
    curWeather = SUNSET;
    Serial.print("\nSwitching to SUNSET\n");
  }
  wdt_reset();
  delay(SUNTIMER);
}

void setSun()
{
  if(sunshineLevel > 1)
  {
    shrinkSnowman();
    sunshineLevel--;
    Serial.print(sunshineLevel);
    Serial.print(" ");
    analogWrite(SUNPIN, sunshineLevel);
    wdt_reset();
  }
  else if(snowmanLevel <= 1)
  {
    Serial.print("\nSwitching to SNOWING\n");
    curWeather = SNOWING;
    //errorOut();
  }
  
  
  delay(SUNTIMER);
}

void enlargeSnowman()
{
  if(snowmanLevel <= GBWMAXDC*3)  snowmanLevel++;
  Serial.print(snowmanLevel);
  Serial.print("\t");
  if(snowmanLevel <= GBWMAXDC) analogWrite(SMBOTPIN, snowmanLevel);
  else if(snowmanLevel <= GBWMAXDC*2)  analogWrite(SMMIDPIN, snowmanLevel-GBWMAXDC);
  else if(snowmanLevel > GBWMAXDC*2)   analogWrite(SMTOPPIN, snowmanLevel-GBWMAXDC*2);
}
void shrinkSnowman()
{
  
  if(snowmanLevel > 0)  snowmanLevel--;
  Serial.print(snowmanLevel);
  Serial.print("\t");
  if(snowmanLevel >= GBWMAXDC*2)     analogWrite(SMTOPPIN, snowmanLevel-GBWMAXDC*2);
  else if(snowmanLevel >= GBWMAXDC)  analogWrite(SMMIDPIN, snowmanLevel-GBWMAXDC);
  else                               analogWrite(SMBOTPIN, snowmanLevel);
}

void updateSky()
{
  char randomChange = random(-10,11); // sane: -4, 5. debug: -50, 50
  if((skyIntensity + randomChange) < GBWMAXDC && (skyIntensity + randomChange) > 0)
  {
    skyIntensity += randomChange;
    analogWrite(SKYPIN, skyIntensity);
  }
  timeToUpdateSky = millis() + 50;
}

void errorOut()
{
  Serial.print("\nError: undefined behavior\n");
  /*
  unsigned char i;
  for(i = 0; i<GBWMAXDC; i++)
  {
    analogWrite(SKYPIN, i);
    delay(10);
  }
  */
  for(;;)
  {
    digitalWrite(SKYPIN, LOW);
    delay(1000);
    digitalWrite(SKYPIN, HIGH);
    delay(1000);
  }
}

/*
 * Vin = 5V
 * 
 * Yellow: 1.9 - 2.1 V
 * 1.9/5 = 19/50
 * {0,0,1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,1,0,1}
 * useless, will use pwm for sun
 * 
 * Green, Blue, White LED: 3.2 - 3.4 V
 * 3.4 / 5 = 17/25
 * {0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,0,1,1,1}
 * 
 * max # of LED driven at once: 4 PWM (snowman, sun), 2 flakes, 1 sky
 */
