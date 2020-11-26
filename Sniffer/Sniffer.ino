/**
 * reference and interesting links:  
 * http://www.hackster.io/rayburne/projects
 * Sketch: https://github.com/SensorsIot/Wi-Fi-S...
 * Ray Burnette's Sniffer: https://www.hackster.io/rayburne/esp8...
 * Various links concerning randimization:
 * https://www.crc.id.au/tracking-people...
 * http://blog.mojonetworks.com/ios8-mac...
 * https://www.airsassociation.org/airs-...
 * https://www.bleepingcomputer.com/news...
 * Shopping Tracking: https://sensalytics.net/en
 * MAC lookup: https://aruljohn.com/mac.pl
 */
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <set>
#include <string>
#include "./functions.h"
#include "./mqtt.h"

#define disable 0
#define enable  1
#define SENDTIME 30000
#define MAXDEVICES 60
#define JBUFFER 15+ (MAXDEVICES * 40)
#define PURGETIME 600000
#define MINRSSI -70

unsigned int channel = 1;
int clients_known_count_old, aps_known_count_old;
unsigned long sendEntry, deleteEntry;
char jsonString[JBUFFER];


String device[MAXDEVICES];
int nbrDevices = 0;
int usedChannels[15];

StaticJsonBuffer<JBUFFER>  jsonBuffer;

void setup() {
  Serial.begin(115200);
  Serial.printf("\n\nSDK version:%s\n\r", system_get_sdk_version());
  /**
   * The ESP8266 SDK API features a promiscuous mode which can be used to capture IEEE 802.11 packets in the air,
   * with some limitations though. It will only decode 802.11b/g/n HT20 packets (20Mhz channel bandwidth),
   * not supporting HT40 packets or LDPC.
   * For those, it will only return their length and other (scarce) low-level information,
   * but no additional decoding will be performed.
   */
  wifi_set_opmode(STATION_MODE);    
  wifi_set_channel(channel);
  wifi_promiscuous_enable(disable);
  /**
   * Set up promiscuous callback
   */
  wifi_set_promiscuous_rx_cb(promisc_cb);
  wifi_promiscuous_enable(enable);
}

void loop() {
  /**
   * choose specific channel ex: 7 to make it faster
   * if we only interested in our presence (not intruder)
   * Most popular channels for 2.4 GHz Wi-Fi are 1, 6, and 11,
   * because they don’t overlap with one another.
   */
  channel = 1;
  boolean sendMQTT = false;
  wifi_set_channel(channel);
  /**
   * Beacon is a packet broadcast sent by the router that synchronizes the wireless network.
   * A beacon is needed to receive information about the router, included but not limited to SSID
   * and other parameters. The beacon interval is simply the frequency of the beacon – how often 
   * the beacon is broadcast by the router. 
   * Most routers are automatically set to a default of 100 milliseconds.
   * Most routers allow the user to adjust the beacon interval within a range 
   * from 20ms to 1000ms (from the default 100ms).
   * There are a few guidelines that can be used to guide you to the proper
   * settings for your hardware.
   * This is why ESP8266 stays 200 ms in each channel to be sure not to meach any beacon message
   * Approximately it takes about 3 seconds to scan all channels
   * It can be reduced by uging more than one ESP8266, as it esp8266 is not a multichannel device
   * example: 
   * ESP1 (1 - 5)
   * ESP2 (6 - 10)
   * ESP3 (11 - 14)
   * If esp detect a new device, sends a message to MQTT
   */ 
  while (true) {
    nothing_new++;                          // functions.h Array is not finite, check bounds and adjust if required
    if (nothing_new > 200) {                // monitor channel for 200 ms
      nothing_new = 0;
      channel++;
      /**
       * Only scan channels 1 to 14
       * unless decided only to check specific channel (AP: Access Point)
       * ex: if (channel == 8) break;
       */ 
      if (channel == 15) break;             
      wifi_set_channel(channel);
    }
    /**
     * critical processing timeslice for NONOS SDK, No delay(0) yield()
     * RTOS vs NonOS:
     * The main goal of most RTOS-es - bring preemptive multitasking/multithreading
     * Multithreading allows us to write clean and linear code without trillions of async callbacks, and complex state graphs.
     * It is very helpful for c/c++ programs, because c/c++ have poor support of async programming. 
     * c++11 lambdas make c++ bit better for asyncs. But modern c++ have too complex syntax itself.
     * But preemptive multithreading is very dangerous, because we have remember about thread synchronization. 
     * There are many side effects: deadlocks, bottle necks, race conditions and etc. Also many 3-rd party libraries are not thread safe,
     * and calling them from different threads can crash your program in random places.
     * So preemptive multithreading is good choice only when you cleanly understand, how them working, and have strong experience with them.
     * cooperative multitasking is optimal choice. Have a look to coroutines libraries. They are also allows us to create linear code, 
     * but there no most side effects of preemptive multithreading, because we can control context switch.
     */
    delay(1);

    if (clients_known_count > clients_known_count_old) {
      clients_known_count_old = clients_known_count;
      sendMQTT = true;
    }
    if (aps_known_count > aps_known_count_old) {
      aps_known_count_old = aps_known_count;
      sendMQTT = true;
    }
    if (millis() - sendEntry > SENDTIME) {
      sendEntry = millis();
      sendMQTT = true;
    }
  }
  purgeDevice();
  if (sendMQTT) {
    showDevices();
    sendDevices();
  }
}


void connectToWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
}

void purgeDevice() {
  for (int u = 0; u < clients_known_count; u++) {
    if ((millis() - clients_known[u].lastDiscoveredTime) > PURGETIME) {
      Serial.print("purge Client" );
      Serial.println(u);
      for (int i = u; i < clients_known_count; i++) memcpy(&clients_known[i], &clients_known[i + 1], sizeof(clients_known[i]));
      clients_known_count--;
      break;
    }
  }
  for (int u = 0; u < aps_known_count; u++) {
    if ((millis() - aps_known[u].lastDiscoveredTime) > PURGETIME) {
      Serial.print("purge Bacon" );
      Serial.println(u);
      for (int i = u; i < aps_known_count; i++) memcpy(&aps_known[i], &aps_known[i + 1], sizeof(aps_known[i]));
      aps_known_count--;
      break;
    }
  }
}


void showDevices() {
  Serial.println("");
  Serial.println("");
  Serial.println("-------------------Device DB-------------------");
  Serial.printf("%4d Devices + Clients.\n", aps_known_count + clients_known_count); // show count
  
  /**
   * Beacon frame is one of the management frames in IEEE 802.11 based WLANs.
   * It contains all the information about the network. Beacon frames are transmitted periodically,
   * they serve to announce the presence of a wireless LAN and to synchronise the members of the service set.
   * -- show Beacons--
   */
  for (int u = 0; u < aps_known_count; u++) {
    Serial.printf( "%4d ",u); // Show beacon number
    Serial.print("B ");
    Serial.print(formatMac1(aps_known[u].bssid));
    Serial.print(" RSSI ");
    Serial.print(aps_known[u].rssi);
    Serial.print(" channel ");
    Serial.println(aps_known[u].channel);
  }

  // show Clients
  for (int u = 0; u < clients_known_count; u++) {
    Serial.printf("%4d ",u); // Show client number
    Serial.print("C ");
    Serial.print(formatMac1(clients_known[u].station));
    Serial.print(" RSSI ");
    Serial.print(clients_known[u].rssi);
    Serial.print(" channel ");
    Serial.println(clients_known[u].channel);
  }
}

void sendDevices() {
  String deviceMac;
  /**
   * Promiscuous mode:
   * https://en.wikipedia.org/wiki/Promiscuous_mode
   */ 
  wifi_promiscuous_enable(disable);
  connectToWiFi();
  client.setServer(MQTTServer, MQTTPort);
  while (!client.connected()) {
    Serial.println("Connecting to MQTT ...");

    if (client.connect("ESP32Client", "admin", "admin" )) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
    }
    yield();
  }

  // Purge json string
  jsonBuffer.clear();
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& mac = root.createNestedArray("MAC");
  // JsonArray& rssi = root.createNestedArray("RSSI");

  /**
   * Add bbeacons, only strongest signals
   */ 
  for (int u = 0; u < aps_known_count; u++) {
    deviceMac = formatMac1(aps_known[u].bssid);
    if (aps_known[u].rssi > MINRSSI) {
      mac.add(deviceMac);
      // rssi.add(aps_known[u].rssi);
    }
  }

  // Add Clients
  for (int u = 0; u < clients_known_count; u++) {
    deviceMac = formatMac1(clients_known[u].station);
    if (clients_known[u].rssi > MINRSSI) {
      mac.add(deviceMac);
      // rssi.add(clients_known[u].rssi);
    }
  }

  Serial.println();
  Serial.printf("number of devices: %02d\n", mac.size());
  root.prettyPrintTo(Serial);
  root.printTo(jsonString);

  if (client.publish("Sniffer", jsonString) == 1) Serial.println("Successfully published");
  else {
    Serial.println();
    Serial.println("Not published. Please add #define MQTT_MAX_PACKET_SIZE 2048 at the beginning of PubSubClient.h file");
    Serial.println();
  }
  client.loop();
  client.disconnect ();
  delay(100);
  wifi_promiscuous_enable(enable);
  sendEntry = millis();
}
