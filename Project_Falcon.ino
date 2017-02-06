/*
  Project Falcon

  What's this?

  A System Notifier for SMS. Falcon receives text from anyone
  and alerts everyone on the house by creating a buzz/noise.
  Everytime a person is detected by Falcon (Using Motion Sensor),
  it checks if the message received is still unread and buzz
  again and again until they read the message.

  Special Thanks:
  Ardin Hajihil (A very nice guy :D)
  Easyelectronyx - Thanks to their products to make this project

  Author: Exequiel Egbert V. Ponce / Tenten Ponce
*/

#include "WiFiEsp.h" //For wifi
#include "SIM900.h" // For sms
#include "sms.h" // For sms
#include "call.h" // For Calls

/*For wifi configurations*/
IPAddress ip(192, 168, 1, 100); //static ip, so there will be no problem
char ssid[] = "PLDTHOMEDSL_Hygienic"; //Wifi name to connect on
char pass[] = "adrian123"; //Wifi password
int status = WL_IDLE_STATUS;
int ledStatus = LOW;
WiFiEspServer server(80); //server port, default lol
// use a ring buffer to increase speed and reduce memory allocation
RingBuffer buf(8); //dont know what's this

/*For SMS configurations*/
SMSGSM sms;
boolean started = false; // set to true if GSM communication is okay
byte Position; //address for sim card messages
char number[20]; //who texted
char smsbuffer[160]; // storage for SMS message

/*For Call Configurations*/
CallGSM call;
byte stat = 0; //handles the status of call

/*Project Falcon Configurations*/
boolean hasSms = false; //default no sms

/*Buzzer Configuration*/
const int buzzer = 9; //buzzer to arduino pin 9
boolean isSound = false; //one time buzz

/*PIR Sensor Configuration*/
boolean hasRead = true; //if sms is not read on the wifi, default no message
int motionPin = 22; //input pin for PIR Motion Sensor

void setup() {
  Serial.begin(115200);   // initialize serial for debugging
  Serial2.begin(9600);    // initialize serial for ESP module
  Serial1.begin(9600);   //initialize serial for GPS Module

  /*For Wifi Configurations*/
  WiFi.init(&Serial2);    // initialize ESP module

  pinMode(LED_BUILTIN, OUTPUT);  //Sets digital pin 13 as output pin

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }

  WiFi.config(ip); //set the static IP

  // attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }

  Serial.println("You're connected to the network");
  printWifiStatus();

  // start the web server on port 80
  server.begin();

  /*For SMS Configurations*/
  digitalWrite(LED_BUILTIN, HIGH); // turn on LED before GPS and GSM initialization
  GSMsetup(); // initialize GSM and connect to network
  digitalWrite(LED_BUILTIN, LOW); // turn off LED after initialization

  /*Buzzer Configuration*/
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 9 as an output

  /*Motion Configuration*/
  pinMode(motionPin, INPUT);
}


void loop() {
  if (isIncomingCall()) {
    buzz(10);
  } else {
    noTone(buzzer); //make sure that there's no tone
  }

  if (!isSound) {
    isSound = true; //1 time buzz only
    tone(buzzer, 1000); // Send 1KHz sound signal...
    delay(1000);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
  }

  receiveMessage(); // check for incoming SMS commands
  checkMotion();

  WiFiEspClient client = server.available();  // listen for incoming clients
  if (client) {                               // if you get a client,
    Serial.println("New client");             // print a message out the serial port
    buf.init();                               // initialize the circular buffer
    while (client.connected()) {              // loop while the client's connected
      if (client.available()) {               // if there's bytes to read from the client,
        char c = client.read();               // read a byte, then
        buf.push(c);                          // push it to the ring buffer

        // you got two newline characters in a row
        // that's the end of the HTTP request, so send a response
        if (buf.endsWith("\r\n\r\n")) {
          sendHttpResponse(client);
          break;
        }
      }
    }

    // close the connection
    client.stop();
    Serial.println("Client disconnected");
  }
}

boolean isIncomingCall() {
  stat = call.CallStatusWithAuth(number, 1, 3); //get call status

  if (stat == 4) { //4 means there's someone calling
    return true;
  } else {
    return false;
  }
}

void checkMotion() {
  int motionStatus = digitalRead(motionPin);  // read input value
  if (motionStatus == HIGH && !hasRead) { //check if there's presence AND if there's new message
    buzz(2);
  }
}

void GSMsetup() {
  Serial.println(F("Initilizing GSM"));
  if (gsm.begin(2400)) { // communicate with GSM module at 2400
    Serial.println(F("GSM is ready"));
    started = true; // established communication with GSM module
  }
  else Serial.println(F("GSM is not ready"));
}


void receiveMessage() {
  if (started) { // check if we can communicate with the GSM module
    Position = sms.IsSMSPresent(SMS_UNREAD); //check location of unread sms

    if (Position != 0) { // check if there is an unread SMS message
      sms.GetSMS(Position, number, smsbuffer, 160); // get number and SMS message
      Serial.print(F("Received SMS from "));
      Serial.println(number); // send number
      Serial.println(smsbuffer); // sender message
      hasSms = true;
      hasRead = false; //reset because there's new message
      buzz(3); //3 buzzes
      sms.DeleteSMS(Position); // delete read SMS message
    }
  }
}

void buzz(int buzzCount) {
  for (int i = 0; i < buzzCount; i++) {
    tone(buzzer, 1000); // Send 1KHz sound signal...
    delay(200);        // ...for 1 sec
    noTone(buzzer);     // Stop sound...
    delay(200);
  }
}

void sendHttpResponse(WiFiEspClient client) {
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();

  if (hasSms) { //print only if there's a message
    client.println("{\"code\":");
    client.println("\""); //enclose the number with ""
    client.println(number);
    client.println("\""); //closing tag of ""
    client.println(",\"msg\":");
    client.println("\""); //enclose the msg with ""
    client.println(smsbuffer);
    client.println("\""); //closing tag of ""
    client.println("}");
    hasRead = true;
  }

  // The HTTP response ends with another blank line:
  client.println();
}

void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print where to go in the browser
  Serial.println();
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
  Serial.println();
}

