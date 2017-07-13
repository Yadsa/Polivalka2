
/* DHT22*/
#define DHTPIN D3  
#define DHTTYPE DHT22 

/* Sensors */
#define PROXIMITY_TRIGGER D4
#define PROXIMITY_ECHO    D5

#define PAYLOAD_SIZE 48

#define DATA_PIN  D6
//#define VCC_PIN   A5
//#define GND_PIN   A3
//#define LED_PIN   13

#define PULSE_WIDTH_SMALL  500

// Button ID (payload1) values.  There are 4 values for 4 channels, organised as
// ch1_btn1, ch1_btn2, ch1_btn3, ch1_btn4, ch2_btn1, etc.
long buttons[] = {
  859124533L,
  861090613L,
  892547893L,
  1395864373L,
  859124563L,
  861090643L,
  892547923L,
  1395864403L,
  859125043L,
  861091123L,
  892548403L,
  1395864883L,
  859132723L,
  861098803L,
  892556083L,
  1395872563L  
};



