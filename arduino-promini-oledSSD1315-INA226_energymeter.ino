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

#define USE_DISPLAY

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
float capacity_mAs;
float energy_mWs;
uint32_t lastIntegrationTime;

/* integrates the current and the power over the time */
void integrateEnergy(void) {
  uint32_t deltaT_ms, tNow;
  float f_mAs, f_mWs;
  tNow = millis();
  deltaT_ms = tNow - lastIntegrationTime;
  f_mAs = current_mA * (float)deltaT_ms / 1000;
  capacity_mAs += f_mAs; /* accumulate the mAs */
  f_mWs = f_mAs * busVoltage_V;
  energy_mWs += f_mWs; /* accumulate the energy */
  lastIntegrationTime = tNow;
}

void setup(void) {
  Serial.begin(9600);
  Serial.println("INA226 Current Sensor Example Sketch - Continuous");
  delay(100);

  Wire.begin();
  ina226.init();

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
  ina226.setAverage(AVERAGE_512); // choose mode and uncomment for change of default

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
  ina226.setConversionTime(CONV_TIME_140); //choose conversion time and uncomment for change of default
  
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
  #define SHUNT_OHMS (0.00101f) /* for demonstration: ~80mm of 1.5mm² copper wire */
  #define MAXAMPERE (0.08f/SHUNT_OHMS)
  ina226.setResistorRange(SHUNT_OHMS, MAXAMPERE);
  
  //ina226.waitUntilConversionCompleted(); //if you comment this line the first data might be zero
  lastIntegrationTime = millis();
}

void loop(void) {
  ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();
  loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);
  #ifdef USE_SERIAL_PRINT
  Serial.print("Shunt Voltage [mV]: "); Serial.println(shuntVoltage_mV);
  Serial.print("Bus Voltage [V]: "); Serial.println(busVoltage_V);
  Serial.print("Load Voltage [V]: "); Serial.println(loadVoltage_V);
  Serial.print("Current[mA]: "); Serial.println(current_mA);
  Serial.print("Bus Power [mW]: "); Serial.println(power_mW);
  if(!ina226.overflow){
    Serial.println("Values OK - no overflow");
  }
  else{
    Serial.println("Overflow! Choose higher current range");
  }
  Serial.println();
  #endif

  /* integrate the current and the power over time, to get energy and capacity */
  integrateEnergy();

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

    dtostrf(busVoltage_V,6,2, stringBuffer);  u8x8.drawString(1, 0, stringBuffer);
    dtostrf(current_mA,7,0, stringBuffer);    u8x8.drawString(2, 1, stringBuffer);
    dtostrf(power_mW,6,1, stringBuffer);      u8x8.drawString(2, 2, stringBuffer);
    dtostrf(capacity_mAs,5,0, stringBuffer);  u8x8.drawString(3, 3, stringBuffer);
    dtostrf(energy_mWs,5,0, stringBuffer);    u8x8.drawString(3, 4, stringBuffer);
    dtostrf(capacity_mAs/3600.0/1000.0,5,2, stringBuffer);  u8x8.drawString(3, 5, stringBuffer);
    dtostrf(energy_mWs/3600.0/1000.0,5,2, stringBuffer);    u8x8.drawString(3, 6, stringBuffer);
  #endif
  delay(100); /* wait a little bit, just to avoid fast flickering display. */
}