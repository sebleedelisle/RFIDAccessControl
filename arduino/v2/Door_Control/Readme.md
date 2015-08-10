How it works : 

On startup, we connect to the WiFi. 
Once that's connected we send the boot message to let the server know that the device has been restarted. 

Every five minutes we send a heartbeat message to the server. 

If an RFID tag is recognised, then we do a local lookup for the tag id. If we find it in the local EEPROM members database, then we open the door. We add the access data to the log database. (Every minute we check to see if there are any access logs to upload, and if so, we upload them all). 

If it's not in the local EEPROM members database, then we send a lookup message to the server, wait for the response. If it responds positively, store the data in the local EEPROM members database and open the door. 

Suggestions : 

We keep track of the last message received from the server, either from a heartbeat, lookup, or any other message sent. That way we know how long it has been since the server spoke to us. If we don't hear from it for a while then we can do something about it. (Not sure what? Reconnect the WIFI? Send a message to somewhere else?) 

Nightly at 3am the device could ask for a list of all the valid member tags from the system and store it in the eeprom member database. 

Use the LED to show the connected status - Green for wifi connected, server happy, Orange for wifi connected, server not happy. Red for wifi disconnected. 

