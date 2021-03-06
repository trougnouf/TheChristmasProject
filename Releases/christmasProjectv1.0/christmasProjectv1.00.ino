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
//                  +-\/-+
//P           PC6  1|    |28  PC5 (AI 5)
//P     (D 0) PD0  2|    |27  PC4 (AI 4)
//P     (D 1) PD1  3|    |26  PC3 (AI 3)
//      (D 2) PD2  4|    |25  PC2 (AI 2)
// PWM+ (D 3) PD3  5|    |24  PC1 (AI 1)
//      (D 4) PD4  6|    |23  PC0 (AI 0)
//            VCC  7|    |22  GND
//            GND  8|    |21  AREF
//            PB6  9|    |20  AVCC
//            PB7 10|    |19  PB5 (D 13)
// PWM+ (D 5) PD5 11|    |18  PB4 (D 12)
// PWM+ (D 6) PD6 12|    |17  PB3 (D 11) PWM
//      (D 7) PD7 13|    |16  PB2 (D 10) PWM
//      (D 8) PB0 14|    |15  PB1 (D 9) PWM
//                  +----+

 */
#include <avr/wdt.h>

#define SUNPIN  5
#define SKYPIN  6
#define SMBOTPIN  9
#define SMMIDPIN  10
#define SMTOPPIN  11
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

unsigned char flakePins[] = {2, 4, 7, 8, 12, 13, A0, A1, A2, A3, A4, A5};
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
