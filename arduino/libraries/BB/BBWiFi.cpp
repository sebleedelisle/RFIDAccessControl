#include <Arduino.h>
#include "BBWiFi.h"
#include <Adafruit_CC3000.h>
#include <aJSON.h>

BBWiFi::BBWiFi(int csPin, int irqPin, int vbenPin):_cc3000(csPin, irqPin, vbenPin, SPI_CLOCK_DIVIDER)
{
  _csPin = csPin;
  _irqPin = irqPin;
  _vbenPin = vbenPin;
  _hasReset = false;

  onlineLed(false);
}

BBWiFi::BBWiFi(int csPin, int irqPin, int vbenPin, int onlineLedPin, const char *ssid, const char *password):_cc3000(csPin, irqPin, vbenPin, SPI_CLOCK_DIVIDER)
{
  _csPin = csPin;
  _irqPin = irqPin;
  _vbenPin = vbenPin;
  _onlineLedPin = onlineLedPin;

  //Reset pin has weak pull-up - pull low to reset
  _hasReset = true;

  pinMode(onlineLedPin, OUTPUT);

  onlineLed(false);
  

  strcpy(_wifi_ssid, ssid);
  strcpy(_wifi_pass, password);
  _wifi_mode = 3;
  
  //strcpy(_website, "iot.arthurguy.co.uk");
  //strcpy(_url, "/ping_back");
  
  strcpy(_website, "bbms.buildbrighton.com");
  strcpy(_url, "/acs");
  
  
}

//Regular check to make sure the network is configured
void BBWiFi::checkConfigure()
{
  // isNetworkSetup simply returns _networkSetup
  // which is set to true if setupNetwork works
  // or false if isNetworkReady fails. 

  if (!isNetworkSetup()) {
    setupNetwork();
  }
}


//The online LED
void BBWiFi::onlineLed(int online)
{
  digitalWrite(_onlineLedPin, online);
}

void BBWiFi::reset()
{
  if (_hasReset == false) {
    return;
  }
  
}

//Is the network ready and online
bool BBWiFi::isNetworkReady() {
	
	bool online = false;

	if (_cc3000.getStatus() == STATUS_DISCONNECTED) {
		_networkSetup = false;
		_wifi_connected = false;
	} else if (_cc3000.getStatus() == STATUS_SCANNING) {
  	
	} else if (_cc3000.getStatus() == STATUS_CONNECTING) {
  	
	} else if (_cc3000.getStatus() == STATUS_CONNECTED) {
		online = true;
	} else {
		Serial.print("Unknown CC3000 Status: ");
   		Serial.println(_cc3000.getStatus());
	}
  
	onlineLed(online);
	return online;
}

//Has the network been setup yet
bool BBWiFi::isNetworkSetup() {
  return _networkSetup;
}

// 1 = main door
// 2 = device control
void BBWiFi::setMode(int mode) {
  _mode = mode;
}

//If the network is ready configure it
void BBWiFi::setupNetwork() {
	
	uint32_t t;
  
	if (!_wifi_initialized) {
		Serial.print(F("Initialising the CC3000 ... "));
		if (_cc3000.begin()) {
			_wifi_initialized = true;
			Serial.println(F("OK"));
		} else {
			Serial.println(F("Error initialising the CC3000"));
			return;
		}
	}
  
  
	if (!_wifi_connected) {
		Serial.print(F("Attempting to connect to the wifi network "));
		Serial.println(_wifi_ssid);
  
		if (_cc3000.connectToAP(_wifi_ssid, _wifi_pass, _wifi_mode)) {
			_wifi_connected = true;
			Serial.println(F(" - Connected!"));
		} else {
			Serial.println(F(" - Failed!"));
			return;
		}
	}
  
  
	if (!_dhcp_lookup_complete) {
		Serial.print(F("Requesting address from DHCP server... "));
		for(t=millis(); !_cc3000.checkDHCP() && ((millis() - t) < _dhcpTimeout); delay(100));
		if(_cc3000.checkDHCP()) {
			_dhcp_lookup_complete = true;
			Serial.println(F("- OK"));
		} else {
			Serial.println(F("- DHCP Timeout"));
			return;
		}

		// Display the IP address DNS, Gateway, etc. 
		while (! displayConnectionDetails()) {
			delay(1000);
		}
	}
    

  //The network has been setup
  _networkSetup = true;
}

// //Is the network adapter busy
// boolean BBWiFi::isBusy() {
//   //TODO: Fix this
//   return false;
// }

//Clear the serial buffer - usefull for getting clean reads
void BBWiFi::clearBuffer() {
  while (Serial.available()) {
        Serial.read();
    }
}
// 
// boolean BBWiFi::waitForResponse() {
//   unsigned long genericTimeCounter = millis();
//   while (isBusy()) {
//     if (millis() > (genericTimeCounter + 10000)) {
//       return false;
//     }
//   }
//   //delay needed for the response to be ready - need to improve
//   delay(500);
//   return true;
// }

//Decode the returned json data
void BBWiFi::decodeResponse(char *serverResponse, char *message) {

    aJsonObject* jsonObject = aJson.parse(serverResponse);

    //device status only on device calls
    if (strcmp(message, "boot") == 0) {

        aJsonObject* deviceStatus = aJson.getObjectItem(jsonObject, "deviceStatus");
        Serial.print("Device Status Received:");
        Serial.println(deviceStatus->valuestring); //currently being sent as a string
        //TODO: Status handling
    }

    //Extract the time from the message
    aJsonObject* time = aJson.getObjectItem(jsonObject, "time");
    _time = time->valueint;
    _has_time = true;


    aJsonObject* jsonCmd = aJson.getObjectItem(jsonObject, "cmd");
    memset(&cmd[0], 0, sizeof(cmd));
    sprintf(cmd, "%s", jsonCmd->valuestring);
    //Serial.print("Command Received:"); Serial.println(cmd);

    if (sizeof(cmd) > 0) {
        _has_cmd = true;
    } else {
        _has_cmd = false;
    }
}

//Send a message home reporting that we have booted up
bool BBWiFi::sendBootMessage(const char *service, const char *device) {
  Serial.println("BBWiFi::sendBootMessage");
  char serverResponse[1500];

  //Clear the array for the new string
  memset(&serverResponse[0], 0, sizeof(serverResponse));

  char stringToSend[150];
  sprintf(stringToSend, "{\"device\":\"%s\", \"service\":\"%s\", \"message\":\"boot\"}", device, service);
  if (sendData(stringToSend, serverResponse)) {
	Serial.print("server response : "); Serial.println(serverResponse);
    decodeResponse(serverResponse, "boot");

    return true;
  }
  return false;
}

bool BBWiFi::sendHeartbeat(const char *service, const char *device) {
  Serial.println("sending heartbeat message");
  char serverResponse[150];
  char stringToSend[150];
  //Clear the array for the new string
  memset(&serverResponse[0], 0, sizeof(serverResponse));

  sprintf(stringToSend, "{\"device\":\"%s\", \"service\":\"%s\", \"message\":\"heartbeat\"}", device, service);
  if (sendData(stringToSend, serverResponse)) {
    decodeResponse(serverResponse, "heartbeat");
	return true; 
  }
	else return false; 
}

//Lookup a key fob
bool BBWiFi::sendLookup(const char *service, const char *device, char *keyFob) {
  Serial.println("sending lookup message");
  //char serverResponse[150];
  char stringToSend[150];
  sprintf(stringToSend,"{\"device\":\"%s\", \"service\":\"%s\", \"message\":\"lookup\", \"tag\":\"%s\"}", device, service, keyFob);
  //Serial.println(stringToSend);

  //Clear the array for the new string
  memset(&serverResponse[0], 0, sizeof(serverResponse));

  //Send the request
  if (sendData(stringToSend, serverResponse)) {
    //Serial.println("sendLookup received: ");
    //Serial.println(serverResponse);
    decodeResponse(serverResponse, "lookup");

    //Serial.println("Returning from lookup");
    return true;
  }
  return false;
}

bool BBWiFi::sendLogEntry(const char *service, const char *device, char *keyFob, time_t time) {
    Serial.println("Sending log entry");

	//TODO error check data? 

    char serverResponse[150];
    char stringToSend[150];
    sprintf(stringToSend,"{\"device\":\"%s\", \"service\":\"%s\", \"message\":\"lookup\", \"tag\":\"%s\", \"time\":\"%d\"}", device, service, keyFob, time);
    Serial.println(stringToSend);

    //Clear the array for the new string
    memset(&serverResponse[0], 0, sizeof(serverResponse));

    //Send the request
    if (sendData(stringToSend, serverResponse)) {
        Serial.print("sendLogEntry received: ");
        Serial.println(serverResponse);
        decodeResponse(serverResponse, "lookup");
        return true;
    }
    return false;
}



bool BBWiFi::sendData(char *stringToSend, char httpResponse[]) {
	Serial.print("BBWiFi::sendData : "); Serial.println(stringToSend); 
	
  unsigned int t = millis();
  

  //Ensure we have the ip of the target server looked up
  if (!lookupIp()) {
  	Serial.println(F("IP Lookup Timeout"));
	Serial.println("BBWiFi::sendData - failed"); 
  	return false;
  }
  

  do {
    _client = _cc3000.connectTCP(_target_ip, 80);
  } while((!_client.connected()) && ((millis() - t) < connectTimeout));


  if (_client.connected()) {
    _client.fastrprint(F("POST "));
    _client.fastrprint(_url);
    _client.fastrprint(F(" HTTP/1.1\r\n"));
    _client.fastrprint(F("Host: ")); _client.fastrprint(_website); _client.fastrprint(F("\r\n"));
    _client.fastrprint(F("Content-Type: application/json\r\n"));
    _client.fastrprint(F("Accept: application/json\r\n"));
    _client.fastrprint(F("Connection: close\r\n"));
    _client.fastrprint(F("Content-Length: "));
    _client.print((strlen(stringToSend)));_client.fastrprint(F("\r\n"));
    _client.println();
    _client.print(stringToSend);

    //Serial.println(stringToSend);

    //Wait for the response to be ready
    unsigned long startTime = millis();
    boolean timedOut = false;

    // bit weird - seems like this is just a check that we receive anything. 
	// seems like this timeout should only be in fetchResponse? SEB
	
    while(_client.available()==0 && !timedOut) {
      if ((millis()-startTime) > SERVER_TIMEOUT_MILS){  
        Serial.println("Timed Out");
        timedOut = true;
        //status = false;
      }
    }

    if (timedOut == false) {
        fetchResponse(httpResponse);
        //Serial.print("sendData received: ");
        //Serial.println(httpResponse);
    } else {
    	_client.close();
		Serial.println("BBWiFi::sendData - timed out"); 
    	return false;
    }

  } else {
    _client.close();
	Serial.println("BBWiFi::sendData - connection failed"); 
    //Serial.println(F("Connect failed"));
    return false;
  }


  _client.close();


  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  /* the next time you try to connect ... */
  //Serial.println(F("\n\nClosing the connection"));
  //_cc3000.disconnect();
  
	Serial.println("Serial response from send data"); 
	Serial.println(httpResponse); 
	Serial.println("BBWiFi::sendData - success"); 
	
  	return true;
  
}



void BBWiFi::fetchResponse(char httpResponse[]) {
	
	Serial.println("BBWifi::fetchResponse()"); 
	
  //Process the response
    boolean lastCharacterEOL = true;
    boolean httpBody = false;
    boolean statusLine = false;
    char statusCode[4];
    int i = 0;
    int n = 0;
    unsigned long lastCharacterTime = millis();
    boolean receiving = true;

    while (receiving) {
      while(_client.available()) {
        
        char c = _client.read();
       	//Serial.print(c);
        
        //Fetch and check the status code which comes after the very first space
        if (c == ' ' && !statusLine) {
          //Status code is starting soon
          statusLine = true;
          
          //Clear the status code array ready for the incoming value
          memset(&statusCode[0], 0, sizeof(statusCode));
        }
        if (statusLine && i < 3 && c != ' '){
          statusCode[i] = c;
          i++;
        }
        if(i == 3){
		  // if we don't get 200 then freak out and give up
		  // (shouldn't we read the rest of the data? - SEB)
          if (strcmp(statusCode, "200") != 0) {
			Serial.print("Server returned code : "); 
            Serial.println(statusCode);
            return;
          } else {
            //Move along so we dont hit this statement again
            i++;
          }
        }
        
        if ((c == '\n')) {
          
          if (httpBody) {
            //Serial.println("End of the response?");
            //New line indicates the end of the response string
            receiving = false;
            //Serial.print(httpResponse);
          }
          
          //If we have two returns the body is starting
          if (lastCharacterEOL && !httpBody) {
            httpBody = true;
            //Serial.println("End of the header?"); 
          }

        }
        
        if (c == '\n') {
            lastCharacterEOL = true;
        } else if (c != '\r') {
            //Normal character received
            lastCharacterEOL = false;
        }
        
        //Start collecting the body message
        if (!lastCharacterEOL && receiving && httpBody){
          //Serial.print(n); Serial.print(":");
          //Serial.println(c);
          httpResponse[n] = c;
          n++;
        }
        
        //When was the last character received. - timeout detection
        lastCharacterTime = millis();
      }
		
	  // if we don't get anything for 250ms then assume that's everything received. 
	  // Seems crazy dangerous - SEB
      if ((millis() - lastCharacterTime) > IDLE_TIMEOUT_MS) {
        receiving = false;
        //Stop receiving and return what we have got
        Serial.println("BBWiFi::fetchResponse() - Timeout, assumed end of data ");
      }
    }
    //Serial.print(httpResponse);
    //return httpResponse;
}


bool BBWiFi::lookupIp()
{
	Serial.println("BBWifi::lookupIp()"); 
  unsigned int t = millis();
  //If we dont have the ip of the target server, fetch it
  while  (_target_ip  ==  0)  {
    Serial.print(F(_website)); Serial.print(F(" -> "));
    if  (!_cc3000.getHostByName(_website, &_target_ip))  {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
    if ((millis() - 5000) > t) {
      return false;
    }
    _cc3000.printIPdotsRev(_target_ip);
    Serial.println();
  }
  Serial.println("BBWifi::lookupIp() - success"); 
  return true;
  
}


bool BBWiFi::displayConnectionDetails()
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!_cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); _cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); _cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); _cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); _cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); _cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

bool BBWiFi::hasTime() {
	return _has_time;
}
time_t BBWiFi::getTime() {
	return _time;
}

bool BBWiFi::hasCmd() {
	return _has_cmd;
}

void BBWiFi::clearCmd() {
	_has_cmd = false;
}
