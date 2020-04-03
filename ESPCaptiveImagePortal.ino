/*
ESPStringTemplate example

Copyright (c) 2020 Dale Giancono. All rights reserved..
This file is a sample application of the ESPStringTemplate class
that uses HTML string elements stored in flash.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ESP8266WiFi.h"
#include "ESPAsyncTCP.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "FS.h"
#include "ESPStringTemplate.h"

/* Store different HTML elements in flash. Descriptions of the various 
tokens are included above each element that has tokens.*/
static const char _PAGEHEADER[] PROGMEM = "<!DOCTYPE html><html><body>";
/* %IMAGEURI% equals the uri of the image to be served. */
/* %IMAGEALT% equals the alternate text of the image to be served. */
/* %WIDTH% equals the width of the image on the webpage. */
/* %HEIGHT% equals the height of the image on the webpage. */
static const char _EMBEDIMAGE[] PROGMEM = "<img src=\"%IMAGEURI%\" alt=\"%IMAGEALT%\" width=\"%WIDTH%\" height=\"%HEIGHT%\">";
static const char _PAGEFOOTER[] PROGMEM = "</body></html>";

/* Declare buffer that will be used to fill with the string template. Care \
must be taken to ensure the buffer is large enough.*/
static char pageBuffer[400];

/* Create DNS server instance to enable captive portal. */
DNSServer dnsServer;
/* Create webserver instance for serving the StringTemplate example. */
AsyncWebServer server(80);
/* Soft AP network parameters */
IPAddress apIP(172, 217, 28, 1);
IPAddress netMsk(255, 255, 255, 0);

Dir spiffsRootDirectory;

void setup() 
{
  /* Start the serial so we can debug with it*/
  Serial.begin(115200);
  
  SPIFFS.begin();
  
  /* Configure access point with static IP address */
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP("CORONAVIRUS CURE INFORMATION");
  /* Start DNS server for captive portal. Route all requests to the ESP IP address. */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);

  /* Open the SPIFFS root directory and serve each file with a uri of the filename */
  spiffsRootDirectory = SPIFFS.openDir("/");
  while(spiffsRootDirectory.next())
  {
    server.serveStatic(
      spiffsRootDirectory.fileName().c_str(), 
      SPIFFS, 
      spiffsRootDirectory.fileName().c_str()); 
  }

    /* Define the handler for when the server receives a GET request for the root uri. */
  server.onNotFound([](AsyncWebServerRequest *request)
  {
    /* This handler is supposed to get the next file in the SPIFFS root directory and
    server it as an embedded image on a HTML page. It does that by getting the filename
    from the next file in the rootDirectory Dir instance, and setting it as the IMAGEURI
    token in the _EMBEDIMAGE HTML element.*/ 
   
    /* Get next file in the root directory. */
    if(!spiffsRootDirectory.next())
    {
      spiffsRootDirectory.rewind();
      spiffsRootDirectory.next();
    }
    char filename[spiffsRootDirectory.fileName().length()];
    strcpy(filename, spiffsRootDirectory.fileName().c_str());
    
    ESPStringTemplate pageTemplate(pageBuffer, sizeof(pageBuffer));
    TokenStringPair pairArray[4];
    pageTemplate.add(FPSTR(_PAGEHEADER));
    pairArray[0] = TokenStringPair("%IMAGEURI%", filename);
    pairArray[1] = TokenStringPair("%IMAGEALT%", "GO HOME");
    pairArray[2] = TokenStringPair("%WIDTH%", "100%");
    pairArray[3] = TokenStringPair("%HEIGHT%", " ");
    pageTemplate.add(FPSTR(_EMBEDIMAGE), pairArray, 4);
    pageTemplate.add(FPSTR(_PAGEFOOTER));   

    const char* page =  pageTemplate.get();
    request->send(200, "text/html", page);
  });
  
  /* Begin the web server */
  server.begin();
}

void loop() 
{
  dnsServer.processNextRequest();
}
