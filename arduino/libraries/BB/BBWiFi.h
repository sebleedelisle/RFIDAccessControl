#pragma once

#define SERVER_TIMEOUT_MILS 10000
#define IDLE_TIMEOUT_MS 3000

#include <Arduino.h>
#include <Adafruit_CC3000.h>
#include <aJSON.h>

class BBWiFi
{
  public:
    BBWiFi(int csPin, int irqPin, int vbenPin);
    BBWiFi(int csPin, int irqPin, int vbenPin, int onlineLedPin, const char* ssid, const char* password);
    bool isNetworkReady();
    bool isNetworkSetup();
    void checkConfigure();
    void reset();
    void setMode(int mode);
    bool sendBootMessage(const char *service, const char *device);
    bool sendHeartbeat(const char *service, const char *device);
    bool sendData(char *stringToSend, char httpResponse[]);
    bool sendLookup(const char *service, const char *device, char *keyFob);
    bool sendLogEntry(const char *service, const char *device, char *keyFob, time_t time);
    void decodeResponse(char *stringToSend, char *message);
    
    time_t getTime();
    bool hasTime();

    void clearBuffer();
    //boolean isBusy();

    char serverResponse[150];
    int receiveString();
    //boolean waitForResponse();

    char cmd[50];
    bool hasCmd();
    void clearCmd();
    
  private:
  
    void onlineLed(int online);
    void setupNetwork();
    bool displayConnectionDetails();
    void fetchResponse(char httpResponse[]);
    bool lookupIp();
    
    int _csPin;
    int _irqPin;
    int _vbenPin;
    int _onlineLedPin;
    int _networkSetup;
    int _hasReset;
    int _mode;
    
    
    bool _wifi_initialized = false;
    bool _wifi_connected = false;
    bool _dhcp_lookup_complete = false;
    
    time_t _time;
    bool _has_time = false;

    bool _has_cmd = false;
    
    char _wifi_ssid[33];
    char _wifi_pass[64];
    int _wifi_mode;
    
    uint32_t _target_ip = 0;
    char _website[50];
    char _url[50];
    
    const unsigned long _dhcpTimeout = 60L * 1000L;
    const unsigned long connectTimeout = 5000;
          
    Adafruit_CC3000 _cc3000;
    Adafruit_CC3000_Client _client;
};

