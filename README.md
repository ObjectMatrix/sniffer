## WiFi Sniffer 
knock knock - who left/entered home. Who is the intruder

## Channel Snipper
![snippers](images/snipper.png)

## Conventional WiFi Channels. 
If you’re using the 2.4 GHz band, channels 1, 6, and 11 are usually the best choices because they don’t overlap with each other.  On the other hand, if you’re using the 5 GHz band, there are 24 non-overlapping channels you can choose from.  
<hr /> 

![WiFi](images/channels.png)

## ESP8266 powered with battery 
Watching AP/WiFi, publish any changes to => MQTT => NODE-RED => Notify Email/SMS

Verify your channels 
```
> airport -s | grep "YOUR_SSID"

```
![Watch WiFi](images/esp8266-batt.png)

