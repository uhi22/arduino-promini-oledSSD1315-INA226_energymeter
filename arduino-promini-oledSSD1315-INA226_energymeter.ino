/* Example found here: https://wiki.seeedstudio.com/Grove-OLED-Display-0.96-SSD1315/ */
/* The u8g2 library for the display is well documented here: https://github.com/olikraus/u8g2/wiki/u8g2reference */

/* Font names and examples are available here: https://github.com/olikraus/u8g2/wiki/fntlist12 */

/* There are two ways to use the display:
 - u8g2 offers much flexibility, but needs a lot of RAM. This costs more RAM than the Arduino has. So we cannot use this.
 - u8x8 offers just fix 8x8 fonts and no graphics. It needs much less RAM.
 Details are explained here: https://github.com/olikraus/u8g2/wiki
 */

/* voltage and current measurement via INA226.
   https://wolles-elektronikkiste.de/ina226
   */

#include <Arduino.h>

#define USE_SERIAL_DATAOUT
#define SERIAL_TRANSMISSION_CYCLE_TIME_MS 250

#define USE_INA226

#define USE_DISPLAY

#define PIN_NOT_UNDERVOLTAGE 7

#ifdef USE_DISPLAY
  #include <U8x8lib.h>
#else
  #include <Wire.h>
#endif

#include <INA226_WE.h>
#define I2C_ADDRESS_INA226 0x40

INA226_WE ina226 = INA226_WE(I2C_ADDRESS_INA226);

#ifdef USE_DISPLAY
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);
#endif

char stringBuffer[20];

float shuntVoltage_mV = 0.0;
float loadVoltage_V = 0.0;
float busVoltage_V = 0.0;
float current_mA = 0.0;
float power_mW = 0.0; 
float capacity_mAs, capacity_mAs_hiRes;
float energy_mWs, energy_mWs_hiRes;
uint32_t lastIntegrationTime;



/* integrates the current and the power over the time */
/* For the integration of very small portions to a huge value there is the risk, that the
   float variable has too less resolution to correctly add a small amount to a huge number.
   That's why we use two variables: a high-resolution variable which correctly accumulates the
   small portions, and if this accumulated value reaches a certain threshold (in positive or
   negative direction), we add the big portion to the low-resolution variable and subtract it
   from the high-resolution variable.
*/

#define HI_RES_QUANTUM_mAs (36000.0f) /* 36000 mAs = 36As = 0.01Ah */
#define HI_RES_QUANTUM_mWs (360000.0f) /* 360000 mWs = 360Ws = 0.1Wh */ 

void integrateEnergy(void) {
  uint32_t deltaT_ms, tNow;
  float f_mAs, f_mWs;
  tNow = millis();
  deltaT_ms = tNow - lastIntegrationTime;
  f_mAs = current_mA * (float)deltaT_ms / 1000;
  capacity_mAs_hiRes += f_mAs; /* accumulate the mAs in the hi-resolution variable */
  while (capacity_mAs_hiRes>HI_RES_QUANTUM_mAs) {
    capacity_mAs_hiRes-=HI_RES_QUANTUM_mAs;
    capacity_mAs+=HI_RES_QUANTUM_mAs;
  }
  while (capacity_mAs_hiRes<-HI_RES_QUANTUM_mAs) {
    capacity_mAs_hiRes+=HI_RES_QUANTUM_mAs;
    capacity_mAs-=HI_RES_QUANTUM_mAs;
  }
  f_mWs = f_mAs * busVoltage_V;
  energy_mWs_hiRes += f_mWs; /* accumulate the energy in the hi-resolution variable */
  while (energy_mWs_hiRes>HI_RES_QUANTUM_mWs) {
    energy_mWs_hiRes-=HI_RES_QUANTUM_mWs;
    energy_mWs+=HI_RES_QUANTUM_mWs;
  }
  while (energy_mWs_hiRes<-HI_RES_QUANTUM_mWs) {
    energy_mWs_hiRes+=HI_RES_QUANTUM_mWs;
    energy_mWs-=HI_RES_QUANTUM_mWs;
  }
  lastIntegrationTime = tNow;
}

void controlUndervoltagePin(void) {
  if (busVoltage_V<21.0) {
    digitalWrite(PIN_NOT_UNDERVOLTAGE, 0);
  }
  if (busVoltage_V>=22.0) {
    digitalWrite(PIN_NOT_UNDERVOLTAGE, 1);
  }
}

void setup(void) {
  Serial.begin(19200);
  Serial.println("INA226 Current Sensor Example Sketch - Continuous");
  delay(100);

  Wire.begin();
  #ifdef USE_INA226
  ina226.init();
  #endif

#ifdef USE_DISPLAY
  u8x8.begin();
  //u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.setFont(u8x8_font_artossans8_r);   // Schriftart auswählen
  u8x8.draw2x2String(0, 0, "Hello");
  u8x8.draw2x2String(0, 2, "World");
  u8x8.draw2x2String(0, 4, "INA226");
  u8x8.drawString(   0, 6, "energy meter");
  delay(1000);
  u8x8.clearDisplay();
#endif
  /* Set Number of measurements for shunt and bus voltage which shall be averaged
  * Mode *     * Number of samples *
  AVERAGE_1            1 (default)
  AVERAGE_4            4
  AVERAGE_16          16
  AVERAGE_64          64
  AVERAGE_128        128
  AVERAGE_256        256
  AVERAGE_512        512
  AVERAGE_1024      1024
  */

/* Design decision: We choose a long averaging (512 sample) together with a fast samping (140µs). This results
in an update-interval of 2*140µs*512=143ms. This has the following benefits:
- gives still a fast reaction on the display
- catches ripples quite good due to the fast sampling and long averaging.
- has relaxed requirements regarding integrating the measured values to Ah and Wh. 100ms integration cycle is fine without data lost.
*/
  #ifdef USE_INA226
  ina226.setAverage(AVERAGE_512); // choose mode and uncomment for change of default
  #endif

  /* Set conversion time in microseconds
     One set of shunt and bus voltage conversion will take: 
     number of samples to be averaged x conversion time x 2
     
     * Mode *         * conversion time *
     CONV_TIME_140          140 µs
     CONV_TIME_204          204 µs
     CONV_TIME_332          332 µs
     CONV_TIME_588          588 µs
     CONV_TIME_1100         1.1 ms (default)
     CONV_TIME_2116       2.116 ms
     CONV_TIME_4156       4.156 ms
     CONV_TIME_8244       8.244 ms  
  */
  #ifdef USE_INA226
  ina226.setConversionTime(CONV_TIME_140); //choose conversion time and uncomment for change of default
  #endif
  /* Set measure mode
  POWER_DOWN - INA226 switched off
  TRIGGERED  - measurement on demand
  CONTINUOUS  - continuous measurements (default)
  */
  //ina226.setMeasureMode(CONTINUOUS); // choose mode and uncomment for change of default
  
  /* If the current values delivered by the INA226 differ by a constant factor
     from values obtained with calibrated equipment you can define a correction factor.
     Correction factor = current delivered from calibrated equipment / current delivered by INA226
  */
  // ina226.setCorrectionFactor(0.95);

  /* configure the shunt resistance */
  #define SHUNT_OHMS (0.00114f) /* for demonstration: ~230mm of 4mm² copper wire */
  #define MAXAMPERE (0.08f/SHUNT_OHMS)
  #ifdef USE_INA226
  ina226.setResistorRange(SHUNT_OHMS, MAXAMPERE);
  #endif
  pinMode(PIN_NOT_UNDERVOLTAGE, OUTPUT);

  //ina226.waitUntilConversionCompleted(); //if you comment this line the first data might be zero
  lastIntegrationTime = millis();
}

void sendSerialData(void) {
  uint32_t tNow;
  static uint32_t tLastSent=0;
  tNow = millis();
  if ((tNow-tLastSent)>SERIAL_TRANSMISSION_CYCLE_TIME_MS) {
    tLastSent=tNow;
    dtostrf(busVoltage_V,1,2, stringBuffer);               Serial.print("U="); Serial.print(stringBuffer); Serial.println();
    dtostrf(current_mA/1000.0,1,2, stringBuffer);          Serial.print("I="); Serial.print(stringBuffer); Serial.println();
    dtostrf(power_mW/1000.0,1,1, stringBuffer);            Serial.print("P="); Serial.print(stringBuffer); Serial.println();
    dtostrf(capacity_mAs/3600.0/1000.0,1,2, stringBuffer); Serial.print("C="); Serial.print(stringBuffer); Serial.println();
    dtostrf(energy_mWs/3600.0/1000.0,1,2, stringBuffer);   Serial.print("E="); Serial.print(stringBuffer); Serial.println();
    dtostrf(millis()/1000,1,0, stringBuffer);              Serial.print("t="); Serial.print(stringBuffer); Serial.println();
  }
}

void loop(void) {
  #ifdef USE_INA226
    ina226.readAndClearFlags();
    shuntVoltage_mV = ina226.getShuntVoltage_mV();
    busVoltage_V = ina226.getBusVoltage_V();
    current_mA = ina226.getCurrent_mA();
    power_mW = busVoltage_V * current_mA;
    loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);
    #ifdef USE_SERIAL_PRINT
      Serial.print("Shunt Voltage [mV]: "); Serial.println(shuntVoltage_mV);
      Serial.print("Bus Voltage [V]: "); Serial.println(busVoltage_V);
      Serial.print("Load Voltage [V]: "); Serial.println(loadVoltage_V);
      Serial.print("Current[mA]: "); Serial.println(current_mA);
      Serial.print("Bus Power [mW]: "); Serial.println(power_mW);
      if(!ina226.overflow){
        Serial.println("Values OK - no overflow");
      } else {
       Serial.println("Overflow! Choose higher current range");
      }
      Serial.println();
    #endif
  #else /* no INA226, just demo */
    shuntVoltage_mV = 0;
    busVoltage_V = 12.34;
    current_mA = 5678;
    power_mW = busVoltage_V * current_mA;
    loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);
  #endif


  #ifdef USE_SERIAL_DATAOUT
  sendSerialData();
  #endif

  /* integrate the current and the power over time, to get energy and capacity */
  integrateEnergy();

  controlUndervoltagePin();

  /* show the results on the display */
  #ifdef USE_BIG_NUMBERS
    u8x8.drawString(0, 0, "V");
    u8x8.drawString(0, 2, "mA");
    u8x8.drawString(0, 4, "mW");
    u8x8.drawString(0, 6, "mAs");

    dtostrf(busVoltage_V,6,2, stringBuffer);  u8x8.draw2x2String(1, 0, stringBuffer);
    dtostrf(current_mA,6,2, stringBuffer);    u8x8.draw2x2String(2, 2, stringBuffer);
    dtostrf(power_mW,6,1, stringBuffer);      u8x8.draw2x2String(2, 4, stringBuffer);
    dtostrf(capacitymAs,5,0, stringBuffer);   u8x8.draw2x2String(3, 6, stringBuffer);
  #else
    u8x8.drawString(0, 0, "V");
    u8x8.drawString(0, 1, "mA");
    u8x8.drawString(0, 2, "mW");
    u8x8.drawString(0, 3, "mAs");
    u8x8.drawString(0, 4, "mWs");
    u8x8.drawString(0, 5, "Ah");
    u8x8.drawString(0, 6, "Wh");
    u8x8.drawString(0, 7, "s");

    dtostrf(busVoltage_V,6,2, stringBuffer);  u8x8.drawString(2, 0, stringBuffer);
    dtostrf(current_mA,7,0, stringBuffer);    u8x8.drawString(3, 1, stringBuffer);
    dtostrf(power_mW,6,1, stringBuffer);      u8x8.drawString(3, 2, stringBuffer);
    dtostrf(capacity_mAs,5,0, stringBuffer);  u8x8.drawString(4, 3, stringBuffer);
    dtostrf(energy_mWs,5,0, stringBuffer);    u8x8.drawString(4, 4, stringBuffer);
    dtostrf(capacity_mAs/3600.0/1000.0,5,2, stringBuffer);  u8x8.drawString(4, 5, stringBuffer);
    dtostrf(energy_mWs/3600.0/1000.0,5,2, stringBuffer);    u8x8.drawString(4, 6, stringBuffer);
    sprintf(stringBuffer, "%ld", millis()/1000);    u8x8.drawString(3, 7, stringBuffer);
  #endif
  delay(100); /* wait a little bit, just to avoid fast flickering display. */
}