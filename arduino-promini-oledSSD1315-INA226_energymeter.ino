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
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);  // High speed I2C
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);    //Low spped I2C
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);
#endif

float f;
char stringBuffer[20];
char bigBuffer[20];

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
  u8x8.drawString(1, 1, "Hello World!");
  //delay(2000);
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
  ina226.setAverage(AVERAGE_16); // choose mode and uncomment for change of default
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
  //ina226.setConversionTime(CONV_TIME_1100); //choose conversion time and uncomment for change of default
  
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
  
  
  
  //ina226.waitUntilConversionCompleted(); //if you comment this line the first data might be zero  
}

void loop(void) {
  #ifdef USE_DISPLAY

  #endif
  
    float shuntVoltage_mV = 0.0;
  float loadVoltage_V = 0.0;
  float busVoltage_V = 0.0;
  float current_mA = 0.0;
  float power_mW = 0.0; 
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
  dtostrf(busVoltage_V,6,2, stringBuffer);  sprintf(bigBuffer, "%sV", stringBuffer);   u8x8.draw2x2String(0, 0, bigBuffer);
  dtostrf(current_mA,6,2, stringBuffer);  sprintf(bigBuffer, "%smA", stringBuffer);   u8x8.draw2x2String(0, 2, bigBuffer);
  //delay(3000);
  delay(200);
}