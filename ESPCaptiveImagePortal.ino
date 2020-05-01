/*
  ESPCaptiveImagePortal

  Copyright (c) 2020 Dale Giancono. All rights reserved..
  This file is a an application for a captive image portal with upload page.

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

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <ESPStringTemplate.h>
#include <ESPFlash.h>
#include <ESPFlashCounter.h>
#include <ESPFlashString.h>


#define DEFAULT_SSID                  "ESPCaptiveImagePortal"
#define HTTP_USER_AUTH                "supersecretadmin"
#define HTTP_PASS_AUTH                "yomamarama"
#define SSID_FILEPATH                 "/ssid.txt"
#define CONNECTION_COUNTER_FILEPATH   "/connectionCounter"

#define IMAGES_DIRECTORY              "/images"
#define WEB_PAGE_FILEPATH             "/webpage.html"

/* Store different HTML elements in flash. Descriptions of the various
  tokens are included above each element that has tokens.*/
static const char _PAGEHEADER[]         PROGMEM = "<!DOCTYPE html><html><head><style>body{background-color: black; color:white;}</style></head><body>";
static const char _TITLE[]              PROGMEM = "<hr><h1>%TITLE%</h1><hr>";
static const char _SUBTITLE[]           PROGMEM = "<hr><h2>%SUBTITLE%</h2><hr>";

/* %IMAGEURI% equals the uri of the image to be served. */
/* %WIDTH% equals the width of the image on the webpage. */
/* %HEIGHT% equals the height of the image on the webpage. */
static const char _EMBEDIMAGE[]         PROGMEM = "<img src=\"%IMAGEURI%\" alt=\"%IMAGEURI%\" width=\"100%\">";
static const char _DELETEIMAGE[]        PROGMEM = "<form action=\"/delete\"> <img src=\"%IMAGEURI%\" alt=\"%IMAGEURI%\" width=\"20%\"> <input type=\"submit\" value=\"delete\" name=\"%IMAGEURI%\"></form>";
static const char _ADDIMAGE[]           PROGMEM = "<form action=\"/supersecretpage\" method=\"post\" enctype=\"multipart/form-data\">Image to Upload:<input type=\"file\" value=\"uploadFile\" name=\"uploadFile\" accept=\"image/*\"><input type=\"submit\" value=\"Upload Image\" name=\"submit\"></form>";
static const char _SSIDEDIT[]           PROGMEM = "<form action=\"/ssidedit\">New SSID (You will be disconnected from WiFi): <input type=\"text\" value=\"\" name=\"SSID\"><input type=\"submit\" name=\"submit\"></form>";
static const char _FILEUPLOADMESSAGE[]  PROGMEM = "File upload complete. click <a href=\"/secretuploadpage\">here</a> to continue...";
static const char _BREAK[]              PROGMEM = "<br>";
static const char _PAGEFOOTER[]         PROGMEM = "</body></html>";

void handleCaptiveImagePortal(AsyncWebServerRequest *request);
void handleUploadPage(AsyncWebServerRequest *request);
void handleDelete(AsyncWebServerRequest *request);
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
void handleSsidEdit(AsyncWebServerRequest *request);

/* Create DNS server instance to enable captive portal. */
DNSServer dnsServer;
/* Create webserver instance for serving the StringTemplate example. */
AsyncWebServer server(80);
/*ESPFlash instance used for uploading files. */
ESPFlash<uint8_t> fileUploadFlash;
/*ESPFlash instance used for storing ssid. */
ESPFlashString ssid(SSID_FILEPATH, DEFAULT_SSID);
/*ESPFlash instance used for counting connections. */
ESPFlashCounter connectionCounter(CONNECTION_COUNTER_FILEPATH);
/*Buffer for webpage */
char buffer[3000];


/* Soft AP network parameters */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
Dir imagesDirectory;

void setup()
{
  SPIFFS.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  
  /* Open the SPIFFS root directory and serve each file with a uri of the filename */
  imagesDirectory = SPIFFS.openDir(IMAGES_DIRECTORY);
  while (imagesDirectory.next())
  {
    server.serveStatic(
      imagesDirectory.fileName().c_str(),
      SPIFFS,
      imagesDirectory.fileName().c_str(),
      "no-cache");
  }
  
  /* Configure access point with static IP address */
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid.get());

  /* Start DNS server for captive portal. Route all requests to the ESP IP address. */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);


  server.on("/supersecretpage", HTTP_GET, handleUploadPage);
  server.on("/supersecretpage", HTTP_POST, handleUploadPage, handleFileUpload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/ssidedit", HTTP_GET, handleSsidEdit);

  /* Define the handler for when the server receives a GET request for the root uri. */
  server.onNotFound(handleCaptiveImagePortal);

  /* Set the LED low (it is inverted) */
  digitalWrite(LED_BUILTIN, true);
  /* Begin the web server */
  server.begin();
}

void loop()
{
  static int previousNumberOfStations = -1;
  
  dnsServer.processNextRequest();
  
  int numberOfStations = WiFi.softAPgetStationNum();
  if(numberOfStations != previousNumberOfStations)
  {
    if(numberOfStations < previousNumberOfStations)
    {
      digitalWrite(LED_BUILTIN, true);
    }
    else
    {
      digitalWrite(LED_BUILTIN, false);
      connectionCounter.increment();
    }

    previousNumberOfStations = numberOfStations;
  }

  
}

void handleCaptiveImagePortal(AsyncWebServerRequest *request)
{  
  String filename;
  ESPStringTemplate pageTemplate(buffer, sizeof(buffer));
  /* This handler is supposed to get the next file in the SPIFFS root directory and
    server it as an embedded image on a HTML page. It does that by getting the filename
    from the next file in the rootDirectory Dir instance, and setting it as the IMAGEURI
    token in the _EMBEDIMAGE HTML element.*/

  /* Get next file in the root directory. */
  if(!imagesDirectory.next())
  {
    imagesDirectory.rewind();
    imagesDirectory.next();
  }

  filename = imagesDirectory.fileName();  
  pageTemplate.add_P(_PAGEHEADER);
  if(filename != "")
  {
    pageTemplate.add_P(_EMBEDIMAGE, "%IMAGEURI%", filename.c_str());
  }
  else
  {
    pageTemplate.add_P(PSTR("<h1>NO IMAGES UPLOADED!!!</h1>"));
  }
  pageTemplate.add_P(_PAGEFOOTER);
  request->send(200, "text/html", buffer);
}

void handleUploadPage(AsyncWebServerRequest *request)
{
  if(!request->authenticate(HTTP_USER_AUTH, HTTP_PASS_AUTH))
  {
    return request->requestAuthentication();
  }
  String filename;
  ESPStringTemplate pageTemplate(buffer, sizeof(buffer));
  pageTemplate.add_P(_PAGEHEADER);
  pageTemplate.add_P(_TITLE, "%TITLE%", "Super Secret Page");
  pageTemplate.add_P(_SUBTITLE, "%SUBTITLE%", "Visit Count");
  pageTemplate.add(String(connectionCounter.get()).c_str());
  pageTemplate.add_P(_SUBTITLE, "%SUBTITLE%", "Delete Image");

  imagesDirectory.rewind();
  while (imagesDirectory.next())
  {
    pageTemplate.add_P(_DELETEIMAGE, "%IMAGEURI%", imagesDirectory.fileName().c_str());
  }
  pageTemplate.add_P(_SUBTITLE, "%SUBTITLE%", "Add Image");
  pageTemplate.add_P(_ADDIMAGE);
  pageTemplate.add_P(_SUBTITLE, "%SUBTITLE%", "Change SSID");
  pageTemplate.add_P(_SSIDEDIT);
  pageTemplate.add_P(_PAGEFOOTER);

  request->send(200, "text/html", buffer);
}

void handleDelete(AsyncWebServerRequest *request)
{
  if(!request->authenticate(HTTP_USER_AUTH, HTTP_PASS_AUTH))
  {
    return request->requestAuthentication();
  }
  for (int i = 0; i < request->params(); i++)
  {
    AsyncWebParameter* p = request->getParam(i);
    if((p->name().c_str() != ""))
    {
      SPIFFS.remove(p->name().c_str());
    }
  }
  request->redirect("/supersecretpage");
}

void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{

  if (!index)
  {
    if(!request->authenticate(HTTP_USER_AUTH, HTTP_PASS_AUTH))
    {
      return request->requestAuthentication();
    }
    String rootFileName;
    rootFileName = "/images/" + filename;
    fileUploadFlash.setFileName(rootFileName.c_str());
  }

  fileUploadFlash.appendElements(data, len);
  
  if (final)
  {
    server.serveStatic(
      fileUploadFlash.getFileName(),
      SPIFFS,
      fileUploadFlash.getFileName(),
      "no-cache");
  }
}

void handleSsidEdit(AsyncWebServerRequest *request)
{
  if(!request->authenticate(HTTP_USER_AUTH, HTTP_PASS_AUTH))
  {
    return request->requestAuthentication();
  }

  AsyncWebParameter* p = request->getParam("SSID");
  if(p->value() != "")
  {
    ssid.set(p->value());
    WiFi.softAPdisconnect(true);
    WiFi.softAP(p->value());  
  }
  else
  {
    request->redirect("/supersecretpage");
  }
}
