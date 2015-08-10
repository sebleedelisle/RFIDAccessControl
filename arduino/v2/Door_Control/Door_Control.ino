/*
LCD Display
EEPROM
Door entry system - refactored version
*/

#include <avr/wdt.h>
#include <aJSON.h>
#include "pitches.h"
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <Time.h>
#include <Timezone.h>
#include <aJSON.h>
#include <Metro.h>
#include "WifiLogin.h"
//Local Database
#include <EDB.h>
#include <EEPROM.h>
#include "Database.h"


#define DEVICE "new-door"
#define SERVICE "entry"

#define DISPLAY_TIME



//Network
#include "BBWiFi.h"

#include "AccessControl.h"


// Pin Setup
const int debugLedPin = 9;
const int wifiCsPin = 10;
const int wifiIrqPin = 3;
const int wifiVbenPin = 4;
const int relayPin = 15;
const int miscSwitch = A11;
const int rfidPowerPin = 23;
const int rfidSerialPin = 7;
const int buzzerPin = 5;
const int onlinePin = 6;
const int LEDPin = 2;


//Time tracking
unsigned long heartbeatTimer;
unsigned long isOnlineTimer = 0;
boolean forceScreenRefresh;
boolean bootMessageSent = false;
unsigned long setupDelayTimer = 0;
unsigned long uploadDelayTimer = 0;


//Delay timers
Metro fiveMinutes = Metro(1000);
//Metro fiveMinutes = Metro(1000);

//Daylight savings
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        //British Summer Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         //Standard Time
Timezone UK(BST, GMT);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t utc, local;


//LCD Screen
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
//LiquidCrystal_I2C lcd(0x20,  2, 1, 0, 4, 5, 6, 7, 3, NEGATIVE);


//Network connection
BBWiFi WiFi(wifiCsPin, wifiIrqPin, wifiVbenPin, onlinePin, WIFI_SSID, WIFI_PASSWORD);

//General functions for the access control system
AccessControl accessControl(relayPin, buzzerPin, LEDPin, debugLedPin);

//The received tag id
char tagString[13];
int rfidResetPin = 23;

void setup() {
  
    Serial.begin(9600);
    
    //delay(2000);
  
    Serial.println("Starting");
  
  
    // ==== Pin Setup ====
    
    Serial.println("Setting up pins");
    pinMode(miscSwitch, INPUT_PULLUP);
    
     
    // ==== Time Setup ====
    Serial.println("Setting the time");
    setSyncProvider(getTeensy3Time);
    delay(100);
    if (timeStatus()!= timeSet) {
      Serial.println("Unable to sync with the RTC");
    } else {
      Serial.println("RTC has set the system time");
    }
    digitalClockDisplay();
    
    
    
    
    // ==== RFID =====
    Serial3.begin(9600);
    pinMode(rfidResetPin, OUTPUT);
    digitalWrite(rfidResetPin, true);

    // ==== Screen Setup ====
    Wire.begin();
    Serial.println("Setting up display");
    lcd.begin(16,2);

    showMessage("Booting", "Please wait..."); 
        
    // ==== Network Setup ====

    Serial.println("Setting up network");
    //WiFi.reset();
    //delay(500);
    WiFi.setMode(1);

    

    
    // ==== Database Setup ====
    
    Serial.println("Setting up database");
    //database create method specifies the start address
    // we need two db's so if they span a wide enough gap they shouldnt interfeer
    
    
    //resetMemberDB();
    //resetLogDB();
    
    //Open dbs starting at location 0
    openDBs();
    
    Serial.print("Local Member Records : ");
    Serial.println(countLocalMemberRecords());
    Serial.print("Local Log Entry Records : ");
    Serial.println(countLocalLogRecords());
    
    heartbeatTimer = 300000;
    
    forceScreenRefresh = true;
 
    //Setup the network on boot
    //setupDelayTimer = millis() + 300000;
}


void loop() {
  
  //Network setup process - ensures it's setup if the first attempt fails
  //Will try and connect every 5 minutes
  if (!WiFi.isNetworkSetup() && (((setupDelayTimer + 300000) < millis()) || setupDelayTimer == 0)) {
    
      showMessage("WiFi Connecting", "Please wait");
      
      WiFi.checkConfigure();
      
      setupDelayTimer = millis();
      
      forceScreenRefresh = true;
      
  }
  
  //setTime(WiFi.getTime());
  //Teensy3Clock.set(WiFi.getTime());
  
  //If the time hasn't been set and the wifi class has it set the time
  if (!accessControl.isTimeSet() && WiFi.hasTime()) {
    syncTime();
  }
 
  
  
  //Are we connected?
  //read the connection status from the wifi module - not internet connectivity
  if ((isOnlineTimer + 5000) < millis()) {
      isOnlineTimer = millis();
      accessControl.systemOnline = WiFi.isNetworkReady();
  }
  

  
  //Update every 5 minutes as long as we arent active
  if (!accessControl.isSystemActive() && fiveMinutes.check() == 1) {

      fiveMinutes.interval(300000);

      if (accessControl.systemOnline) {

          //uploadingMessage();

          //If we havent reported the boot yet try that
          if (!bootMessageSent) {
              showMessage("Sending boot", "message"); 
              
              if (WiFi.sendBootMessage(SERVICE, DEVICE)) {
                  bootMessageSent = true;
                  forceScreenRefresh = true;
                  showMessage("Boot message", "sent!"); 
              } else { 
                 showMessage("Boot message", "failed!"); 
              }
              //Serial.println("Boot message sent!"); 
              //} else if (countLocalLogRecords() > 0) {
              
              //    Serial.println("Uploading local records");
              //    uploadLogFiles();
              
          } else {
              showMessage("Sending heartbeat", ""); 
              Serial.println("Sending heartbeat");
              WiFi.sendHeartbeat(SERVICE, DEVICE);
              
          }

          forceScreenRefresh = true;
      }
  }
  
  
  //Dont run to fast - can probably be improved
  delay(200);

  
  //Was the lookup request successfull
  uint8_t success;
  
  //Has someone presented a tag?
  if (rfidReadTag()) {

      //Beep to let the user know the tag was read
      accessControl.ackTone();
      
      
      //Lookup the tag
      showMessage("Checking Local");
      success = searchLocalDB(tagString);
      if (success) {
          //Save the current tag and time in the logfile for upload
          copyString(logRecord.tag, tagString);
          logRecord.time = now();
          EDB_Status result = logDb.appendRec(EDB_REC logRecord);
              
      } else if (accessControl.systemOnline) {
          showMessage("Checking Remote");
          success = searchRemoteDB(tagString);
      }
      
      //Tag found
      if (success) {
        
          accessControl.setSystemActive();

          accessControl.successTone();
          
      } else {
          //Tag not found
          accessControl.failureTone();
          
          accessControl.deactivateSystem();
          
          showMessage("No Access");
          
          delay(1000);
          
          forceScreenRefresh = true;
          //set the timer so the screen will refresh in 3 seconds
          //screenLastRefreshed = millis() - 57000;
      }

      
  }
  
  //If a user has passed through update the display and keep the door open
  if (accessControl.isSystemActive()) {
    
      //Open the door
      accessControl.activateRelay(1);
           
      //Display the user on the screen
      showMessage("Welcome back", accessControl.activeUserName);
     
      //If its been 4 seconds turn off the node
      if ((accessControl.getSystemActiveAt() + 4000) < millis()) {
          accessControl.deactivateSystem();
          forceScreenRefresh = true;
      }
  } else {
      
      //Keep the door locked
      accessControl.activateRelay(0);
      
      
      accessControl.standbyLight();
      
      //Refresh the display message every 60 seconds
      //if (((screenLastRefreshed + 60000) < millis()) || forceScreenRefresh) {
      if (forceScreenRefresh) {
          //screenLastRefreshed = millis();
          forceScreenRefresh = false;
        
          displayReadyMessage();
      }
      
      
      #ifdef DISPLAY_TIME
      
      if (accessControl.isTimeSet()) {
          lcd.setCursor(4, 1);
          lcdTime();
      } else {
          lcd.setCursor (4, 1);
          lcd.print("Welcome");
      }
      
      #endif
      
      
      
      
      //Do we have anything to upload? Check every minute
      if (accessControl.systemOnline && (countLocalLogRecords() > 0) && ((uploadDelayTimer + 60000) < millis())) {
          uploadLogFiles();
          uploadDelayTimer = millis();
      }
      
      
      
      if (WiFi.hasCmd()) {
        Serial.print("Command received: "); Serial.println(WiFi.cmd);
        
        //action command
        
        if (strcmp(WiFi.cmd, "clear-memory") == 0) {
          Serial.println("Clearing local memory");
          resetMemberDB();
        }
        
        WiFi.clearCmd();
      }
      
      
  }
  
  //Reset the DB if the button is pressed
  //if (analogRead(miscSwitch) == 0) {
  //    resetDBs();
  //}

  
  //Flash the debug light so we know its still alive
  accessControl.flashDebugLight();
}

void displayReadyMessage()
{
  #ifdef DISPLAY_TIME
    
      lcd.clear();
      lcd.setCursor (1, 0);
      lcd.print("Build Brighton");
      
  #else
  
      lcd.begin(8,2);
      lcd.clear();
      lcd.print("Welcome to");
      lcd.setCursor (0, 1);
      lcd.print("Build Brighton");
      
      lcd.setCursor (14, 1);
      if (accessControl.systemOnline) {
        //lcd.print(".");
      } else {
        lcd.print(".");
      }
      
  #endif
}

void uploadingMessage() {
    lcd.home();
    lcd.clear();
    lcd.print("Uploading");
    lcd.setCursor (0, 1);
    lcd.print("Please wait");
}

void showMessage(char const *line1) { 
  showMessage(line1, ""); 
}
void showMessage( char const *line1, char const *line2) { 

    lcd.home();
    lcd.clear();
    lcd.print(line1);
    lcd.setCursor (0, 1);
    lcd.print(line2);

  Serial.print("showMessage() "); 
  Serial.print(line1); 
  Serial.print(" "); 
  Serial.println(line2); 
}
void resetLogDB()
{
    //Create a table to store the access log data
    logDb.create(TABLE_SIZE/2, TABLE_SIZE, (unsigned int)sizeof(logRecord));
    delay(500);
}

void resetMemberDB()
{
    //create table at with starting address 0 - this wipes the db
    memberDb.create(0, TABLE_SIZE/2, (unsigned int)sizeof(accessControlData));
    delay(500);
}

void openDBs()
{
    memberDb.open(0);
    logDb.open(TABLE_SIZE/2);
}


boolean searchLocalDB(char tagString[13])
{
    for (unsigned int recno = 1; recno <= memberDb.count(); recno++)
    {
      EDB_Status result = memberDb.readRec(recno, EDB_REC accessControlData);
      if (result == EDB_OK)
      {
        Serial.print(recno); 
        Serial.print(" found member record : "); 
        Serial.println(accessControlData.name); 
        if (strcmp(accessControlData.tag, tagString) == 0)
        {
            //Copy the name for use later
            copyString(accessControl.activeUserName, accessControlData.name);
            //memberDb.deleteRec(recno);
            return true;
        }
      }
    }

    return false;
}



bool searchRemoteDB(char tagString[13])
{
  
    uint8_t index = 0;
    // why isn't this a bool? - SEB
    bool success = false;
    
    //Reset the user name variable and error variable
    memset(&accessControl.activeUserName[0], 0, sizeof(accessControl.activeUserName));
    
    //Clear the buffer before sending the tag - we want a clean buffer
    WiFi.clearBuffer();
    
    if (WiFi.sendLookup(SERVICE, DEVICE, tagString) == false)
    {
        return false;
    }
    else
    {
      Serial.println(WiFi.serverResponse);
        aJsonObject* jsonObject = aJson.parse(WiFi.serverResponse);
        aJsonObject* valid = aJson.getObjectItem(jsonObject, "valid");
        aJsonObject* cmd = aJson.getObjectItem(jsonObject, "cmd");
        
        
        if (valid->valuestring[0] == '1') {
            //get the loggedin members name
            aJsonObject* name = aJson.getObjectItem(jsonObject, "member");
            Serial.println(name->valuestring);
            
            //Copy the users name to a global variable
            copyString(accessControl.activeUserName, name->valuestring);
            //lcd.setCursor (0, 1);
            //lcd.print(name->valuestring);
            
            success = true;
            
        } else {
            //Keyfob not found
            success = 0;
        }
        
        //Check for a local record and update it
        uint8_t foundLocal = 0;
        if (countLocalMemberRecords() > 0)
        {
            for (int recno = 1; recno <= countLocalMemberRecords(); recno++)
            {
                //Serial.print("Recno: "); 
                //Serial.println(recno);
                EDB_Status result = memberDb.readRec(recno, EDB_REC accessControlData);
                if (result == EDB_OK)
                {
                    //Serial.print("Key: "); 
                    //Serial.println(accessControlData.tag);
                    if (strcmp(accessControlData.tag, tagString) == 0)
                    {
                        //Local record found
                        if (!success) {
                            //The local record needs to be removed
                            memberDb.deleteRec(recno);
                        }
                        foundLocal = 1;
                        Serial.println("Found existing local record");
                    }
                }
            }
        }
        
        
        //Add it to the local DB
        if (!foundLocal && success) {
          Serial.println("Adding record to the local db");
            copyString(accessControlData.tag, tagString);
            copyString(accessControlData.name, accessControl.activeUserName);
            EDB_Status result = memberDb.appendRec(EDB_REC accessControlData);
        }
        
        aJson.deleteItem(jsonObject);
    }
    
    return success;
}



void uploadLogFiles() {
  
    lcd.clear();
    lcd.print("Please Wait");
    lcd.setCursor (0, 1);
    lcd.print("Uploading...");
    Serial.println("uploadLogFiles()"); 
    
    Serial.print("number of records : "); Serial.println(countLocalLogRecords()); 
    
    for (int recno = 1; recno <= countLocalLogRecords(); recno++)
    {
        EDB_Status result = logDb.readRec(recno, EDB_REC logRecord);
        if (result == EDB_OK)
        {   
            if (WiFi.sendLogEntry(SERVICE, DEVICE, logRecord.tag, logRecord.time))
            {
                logDb.deleteRec(recno);
            }
            
            delay(200);
        }
    }
    Serial.print("Sent all the records - record count now : "); Serial.println(countLocalLogRecords()); 
    
    forceScreenRefresh = true;
}



void syncTime()
{
    Serial.println("Setting the time");
    //Update the rtc and processor clock
    setTime(WiFi.getTime());
    Teensy3Clock.set(WiFi.getTime());
    digitalClockDisplay();
    accessControl.recordTimeSet();
}



void copyString(char *p1, char *p2)
{
    while(*p2 !='\0')
    {
       *p1 = *p2;
       p1++;
       p2++;
     }
    *p1= '\0';
}





// utility functions

uint8_t countLocalMemberRecords()
{
  
    return memberDb.count();
}

uint8_t countLocalLogRecords()
{
    return logDb.count();
}

/*
void watchdogOn() {
  
  // Clear the reset flag, the WDRF bit (bit 3) of MCUSR.
  MCUSR = MCUSR & B11110111;
    
  // Set the WDCE bit (bit 4) and the WDE bit (bit 3) 
  // of WDTCSR. The WDCE bit must be set in order to 
  // change WDE or the watchdog prescalers. Setting the 
  // WDCE bit will allow updtaes to the prescalers and 
  // WDE for 4 clock cycles then it will be reset by 
  // hardware.
  WDTCSR = WDTCSR | B00011000; 
  
  // Set the watchdog timeout prescaler value to 1024 K 
  // which will yeild a time-out interval of about 8.0 s.
  WDTCSR = B00100001;
  
  // Enable the watchdog timer interupt.
  WDTCSR = WDTCSR | B01000000;
  MCUSR = MCUSR & B11110111;

}
*/
  
void lcdTime() {
  
  //Convert to local time (timezone adjustment)
  local = UK.toLocal(now(), &tcr);
  
  //lcd.home();
  lcd.print(hour(local));
  lcd.print(":");
  lcdPrintDigits(minute());
  lcd.print(":");
  lcdPrintDigits(second());
}

void digitalClockDisplay() {
  
  //Convert to local time (timezone adjustment)
  local = UK.toLocal(now(), &tcr);
  
  // digital clock display of the time
  Serial.print(hour(local));
  serialPrintDigits(minute());
  serialPrintDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}
void serialPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
void lcdPrintDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}





// RFID
uint8_t rfidReadTag()
{
    //Variablaes used to track the current character and the overall state
    uint8_t index = 0, reading = 0, tagFetching = 0, tagFetched = 0;
    
    //Was the lookup request successfull
    //uint8_t success;
    
    //Clear the current tag
    memset(&tagString[0], 0, sizeof(tagString));
    
    //Is anything coming from the reader?
    while(Serial3.available()){
        
        //Data format
        // Start (0x02) | Data (10 bytes) | Chksum (2 bytes) | CR | LF | End (0x03)
        
        char readByte = Serial3.read(); //read next available byte
        
        if(readByte == 2) reading = 1; //begining of tag
        if(readByte == 3) reading = 0; //end of tag
        
        //If we are reading and get a good character add it to the string
        if(reading && readByte != 2 && readByte != 10 && readByte != 13){
            //store the tag
            tagString[index] = readByte;
            index ++;
            tagFetching = 1;
        }
        //Have we reached the full tag length
        //This prevents this loop from trying to read two codes
        if ((reading == 0) && (index > 1)) {
            break;
        }
    }
    //If we have stoped reading and started in the frist place
    if (!reading && tagFetching) {
        tagFetched = 1;
        tagFetching = 0;
        tagString[index] = '\0';
        
        //Sometimes a random string is received - filter these out
        if (strcmp("000000000000", tagString) == 0) {
            return 0;
        }
    }

    //The RFID reader can send multiple coppies of the code, clear the buffer of any remaining codes
    while(Serial3.available()) {
        Serial3.read();
    }
    
    return tagFetched;
}

void rfidReset(uint8_t resetDevice) {
    digitalWrite(rfidResetPin, !resetDevice);
}

void rfidTogglePower() {
    rfidReset(1);
    delay(100);
    rfidReset(0);
    delay(200);
}

