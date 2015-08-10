
//External EEPROM
#define EXTERNAL_EEPROM_ADDR 0x50  //eeprom address




// Use the external EEPROM as storage
#define TABLE_SIZE 32768 // Arduino 24LC256

//Setup the structure of the stored data
struct AccessControlData {
  char tag[13];
  char name[21];
} 
accessControlData;

struct LogRecord {
  char tag[13];
  time_t time;
} 
logRecord;




//External EEPROM
void writeEEPROM(unsigned int eeaddress, byte data ) 
{
  Wire.beginTransmission(EXTERNAL_EEPROM_ADDR);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(data);
  Wire.endTransmission();
 
  delay(5);
}
 
byte readEEPROM(unsigned int eeaddress ) 
{
  byte rdata = 0xFF;
 
  Wire.beginTransmission(EXTERNAL_EEPROM_ADDR);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
 
  Wire.requestFrom(EXTERNAL_EEPROM_ADDR,1);
 
  if (Wire.available()) rdata = Wire.read();
 
  return rdata;
}




// The read and write handlers for using the EEPROM Library
void writer(unsigned long address, byte data) {
    writeEEPROM(address, data);     //external
}
byte reader(unsigned long address) {
    return readEEPROM(address);     //external
}
// Create an EDB object with the appropriate write and read handlers
EDB memberDb(&writer, &reader);
EDB logDb(&writer, &reader);


