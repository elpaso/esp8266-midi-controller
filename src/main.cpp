/*
 * MIDI Pedal ESP8266

 * This program turns the ESP-8266 into a MIDI controller with 6 buttons
 * that can send MIDI commands to a MIDI device.
 * The MIDI commands can be configured via a web interface.
 * The configuration is stored in the SPIFFS file system.
 * The web interface is served by a web server running on the ESP-8266.
 * The web server is started only if the ESP-8266 is connected to a Wi-Fi network.
 * The Wi-Fi network is configured via the AP_1, PWD_1, AP_2, PWD_2 constants.
 * The ESP-8266 tries to connect to the first network, if it fails it tries to connect to the second one.
 * If it fails to connect to both networks, the web server is not started.
 *
 * Copyright 2024 Alessandro Pasotti
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#define AP_1 "SSID_1"      // The SSID (name) of the Wi-Fi network you want to connect to
#define PWD_1 "PASSWORD_1" // The password of the Wi-Fi network
#define AP_2 "SSID_2"      // The SSID (name) of the Wi-Fi network you want to connect to
#define PWD_2 "PASSWORD_2" // The password of the Wi-Fi network

//#define DEBUG

#define LONG_PRESS_INTERVAL_MS 300

#include <OneButton.h>
#include "midi_controller.h"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h> // Include the WebServer library
#include <FS.h>               // Include the SPIFFS library

ESP8266WiFiMulti wifiMulti; // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP request on port 80

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)

// MIDI Buttons configuration

OneButton btn1 = OneButton(
    BUTTON_PIN1, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

OneButton btn2 = OneButton(
    BUTTON_PIN2, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

OneButton btn3 = OneButton(
    BUTTON_PIN3, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

OneButton btn4 = OneButton(
    BUTTON_PIN4, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

OneButton btn5 = OneButton(
    BUTTON_PIN5, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

OneButton btn6 = OneButton(
    BUTTON_PIN6, // Input pin for the button
    true,        // Button is active LOW
    true         // Enable internal pull-up resistor
);

// Array of 6 midi buttons
MIDIButtonCommands midiButtons[6];
MIDICommandFlags midiCommandFlags[6];

// Button

void initMIDIButtons()
{
    initMIDIButton(midiButtons[0], "/button1");
    initMIDIButton(midiButtons[1], "/button2");
    initMIDIButton(midiButtons[2], "/button3");
    initMIDIButton(midiButtons[3], "/button4");
    initMIDIButton(midiButtons[4], "/button5");
    initMIDIButton(midiButtons[5], "/button6");

    btn1.setIdleMs(midiButtons[0].doublePush.count == 0 ? 60 : 1000);
    btn2.setIdleMs(midiButtons[1].doublePush.count == 0 ? 60 : 1000);
    btn3.setIdleMs(midiButtons[2].doublePush.count == 0 ? 60 : 1000);
    btn4.setIdleMs(midiButtons[3].doublePush.count == 0 ? 60 : 1000);
    btn5.setIdleMs(midiButtons[4].doublePush.count == 0 ? 60 : 1000);
    btn6.setIdleMs(midiButtons[5].doublePush.count == 0 ? 60 : 1000);

    btn1.setClickMs(midiButtons[0].doublePush.count == 0 ? 60 : 400);
    btn2.setClickMs(midiButtons[1].doublePush.count == 0 ? 60 : 400);
    btn3.setClickMs(midiButtons[2].doublePush.count == 0 ? 60 : 400);
    btn4.setClickMs(midiButtons[3].doublePush.count == 0 ? 60 : 400);
    btn5.setClickMs(midiButtons[4].doublePush.count == 0 ? 60 : 400);
    btn6.setClickMs(midiButtons[5].doublePush.count == 0 ? 60 : 400);

#ifdef DEBUG

    // Print doublepush count for all buttons
    Serial.println("Doublepush count");
    Serial.println("Button 1: " + String(midiButtons[0].doublePush.count));
    Serial.println("Button 2: " + String(midiButtons[1].doublePush.count));
    Serial.println("Button 3: " + String(midiButtons[2].doublePush.count));
    Serial.println("Button 4: " + String(midiButtons[3].doublePush.count));
    Serial.println("Button 5: " + String(midiButtons[4].doublePush.count));
    Serial.println("Button 6: " + String(midiButtons[5].doublePush.count));

#endif

    btn1.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
    btn2.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
    btn3.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
    btn4.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
    btn5.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
    btn6.setLongPressIntervalMs(LONG_PRESS_INTERVAL_MS);
}

bool serverStart()
{
    // add Wi-Fi networks you want to connect to
    wifiMulti.addAP(AP_1, PWD_1);
    wifiMulti.addAP(AP_2, PWD_1);

#ifdef DEBUG
    Serial.println("Connecting ...");
#endif

    int i = 0;

    // Wait for connection
    while (wifiMulti.run() != WL_CONNECTED && i++ < 5)
    { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
        delay(250);
#ifdef DEBUG
        Serial.print('.');
#endif
        ++i;
    }

    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED)
    {
#ifdef DEBUG
        Serial.println('\n');
        Serial.print("Connected to ");
        Serial.println(WiFi.SSID()); // Tell us what network we're connected to
        Serial.print("IP address:\t");
        Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
#endif
        if (MDNS.begin("esp8266"))
        { // Start the mDNS responder for esp8266.local
#ifdef DEBUG
            Serial.println("mDNS responder started");
        }
        else
        {
            Serial.println("Error setting up MDNS responder!");
#endif
        }

        // Redirect / to index.html
        server.on("/", HTTP_GET, []() {
            server.sendHeader("Location", "/index.html", true); // redirect to our html web page
            server.send(303, "text/plain", "See Other");        // return a 302 redirect (browser will immediately ask for the page at the new location)
        });

        server.on("/set", HTTP_POST, []() { // If the client requests the root path, send them to the control page
                                            // Parse command

#ifdef DEBUG
            Serial.println("POST /set");
#endif
            for (int i = 0; i < 6; i++)
            {
#ifdef DEBUG
                Serial.println("Button " + String(i + 1));
                Serial.println("PUSH " + server.arg("BUTTON_" + String(i + 1) + "_PUSH"));
                Serial.println("HOLD " + server.arg("BUTTON_" + String(i + 1) + "_HOLD"));
                Serial.println("DOUBLE_PUSH" + server.arg("BUTTON_" + String(i + 1) + "_DOUBLE_PUSH"));
#endif
                midiButtons[i].push = parseMIDICommands(server.arg("BUTTON_" + String(i + 1) + "_PUSH"));
                midiButtons[i].hold = parseMIDICommands(server.arg("BUTTON_" + String(i + 1) + "_HOLD"));
                midiButtons[i].doublePush = parseMIDICommands(server.arg("BUTTON_" + String(i + 1) + "_DOUBLE_PUSH"));
                midiButtons[i].flags.repeatOnHold = server.arg("BUTTON_" + String(i + 1) + "_REPEAT_FLAG") == "1";
                //midiButtons[i].flags.disableDoublePush = server.arg("BUTTON_" + String(i + 1) + "_DISABLE_DOUBLE_FLAG") == "1";
                midiButtons[i].var.min = server.arg("BUTTON_" + String(i + 1) + "_VAR_MIN").toInt();
                midiButtons[i].var.max = server.arg("BUTTON_" + String(i + 1) + "_VAR_MAX").toInt();
                midiButtons[i].var.value = server.arg("BUTTON_" + String(i + 1) + "_VAR_VALUE").toInt();

                // Write values to SPIFFS
                saveMIDIButton(midiButtons[i], "/button" + String(i + 1));
            }

            server.sendHeader("Location", "/index.html", true); // redirect to our html web page
            server.send(303, "text/plain", "See Other");        // return a 302 redirect (browser will immediately ask for the page at the new location)
        });

        server.on("/index.html", HTTP_GET, []() {
            File file = SPIFFS.open("/index.html", "r");

            // read file content into String
            String form = file.readString();
            file.close();

            // Replace all 6 button form placeholder with actual values
            form.replace("{{BUTTON_1_PUSH}}", midiButtons[0].push.toString());
            form.replace("{{BUTTON_1_HOLD}}", midiButtons[0].hold.toString());
            form.replace("{{BUTTON_1_DOUBLE_PUSH}}", midiButtons[0].doublePush.toString());
            form.replace("{{BUTTON_1_REPEAT_FLAG}}", midiButtons[0].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_1_DISABLE_DOUBLE_FLAG}}", midiButtons[0].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_1_VAR_MIN}}", String(midiButtons[0].var.min));
            form.replace("{{BUTTON_1_VAR_MAX}}", String(midiButtons[0].var.max));
            form.replace("{{BUTTON_1_VAR_VALUE}}", String(midiButtons[0].var.value));

            form.replace("{{BUTTON_2_PUSH}}", midiButtons[1].push.toString());
            form.replace("{{BUTTON_2_HOLD}}", midiButtons[1].hold.toString());
            form.replace("{{BUTTON_2_DOUBLE_PUSH}}", midiButtons[1].doublePush.toString());
            form.replace("{{BUTTON_2_REPEAT_FLAG}}", midiButtons[1].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_2_DISABLE_DOUBLE_FLAG}}", midiButtons[1].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_2_VAR_MIN}}", String(midiButtons[1].var.min));
            form.replace("{{BUTTON_2_VAR_MAX}}", String(midiButtons[1].var.max));
            form.replace("{{BUTTON_2_VAR_VALUE}}", String(midiButtons[1].var.value));

            form.replace("{{BUTTON_3_PUSH}}", midiButtons[2].push.toString());
            form.replace("{{BUTTON_3_HOLD}}", midiButtons[2].hold.toString());
            form.replace("{{BUTTON_3_DOUBLE_PUSH}}", midiButtons[2].doublePush.toString());
            form.replace("{{BUTTON_3_REPEAT_FLAG}}", midiButtons[2].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_3_DISABLE_DOUBLE_FLAG}}", midiButtons[2].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_3_VAR_MIN}}", String(midiButtons[2].var.min));
            form.replace("{{BUTTON_3_VAR_MAX}}", String(midiButtons[2].var.max));
            form.replace("{{BUTTON_3_VAR_VALUE}}", String(midiButtons[2].var.value));

            form.replace("{{BUTTON_4_PUSH}}", midiButtons[3].push.toString());
            form.replace("{{BUTTON_4_HOLD}}", midiButtons[3].hold.toString());
            form.replace("{{BUTTON_4_DOUBLE_PUSH}}", midiButtons[3].doublePush.toString());
            form.replace("{{BUTTON_4_REPEAT_FLAG}}", midiButtons[3].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_4_DISABLE_DOUBLE_FLAG}}", midiButtons[3].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_4_VAR_MIN}}", String(midiButtons[3].var.min));
            form.replace("{{BUTTON_4_VAR_MAX}}", String(midiButtons[3].var.max));
            form.replace("{{BUTTON_4_VAR_VALUE}}", String(midiButtons[3].var.value));

            form.replace("{{BUTTON_5_PUSH}}", midiButtons[4].push.toString());
            form.replace("{{BUTTON_5_HOLD}}", midiButtons[4].hold.toString());
            form.replace("{{BUTTON_5_DOUBLE_PUSH}}", midiButtons[4].doublePush.toString());
            form.replace("{{BUTTON_5_REPEAT_FLAG}}", midiButtons[4].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_5_DISABLE_DOUBLE_FLAG}}", midiButtons[4].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_5_VAR_MIN}}", String(midiButtons[4].var.min));
            form.replace("{{BUTTON_5_VAR_MAX}}", String(midiButtons[4].var.max));
            form.replace("{{BUTTON_5_VAR_VALUE}}", String(midiButtons[4].var.value));

            form.replace("{{BUTTON_6_PUSH}}", midiButtons[5].push.toString());
            form.replace("{{BUTTON_6_HOLD}}", midiButtons[5].hold.toString());
            form.replace("{{BUTTON_6_DOUBLE_PUSH}}", midiButtons[5].doublePush.toString());
            form.replace("{{BUTTON_6_REPEAT_FLAG}}", midiButtons[5].flags.repeatOnHold ? "checked" : "");
            //form.replace("{{BUTTON_6_DISABLE_DOUBLE_FLAG}}", midiButtons[5].flags.disableDoublePush ? "checked" : "");
            form.replace("{{BUTTON_6_VAR_MIN}}", String(midiButtons[5].var.min));
            form.replace("{{BUTTON_6_VAR_MAX}}", String(midiButtons[5].var.max));
            form.replace("{{BUTTON_6_VAR_VALUE}}", String(midiButtons[5].var.value));

#ifdef DEBUG
            Serial.println("Sending form");
#endif
            server.send(200, "text/html", form); //Send web page
        });

        server.onNotFound([]() {                                  // If the client requests any URI
            if (!handleFileRead(server.uri()))                    // send it if it exists
                server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
        });

        server.begin(); // Actually start the server
#ifdef DEBUG
        Serial.println("HTTP server started");
#endif
        SPIFFS.begin(); // Start the SPI Flash Files System

        return true;
    }
    else
    {

#ifdef DEBUG
        Serial.println('\n');
        Serial.println("Connection failed.");
#endif

        return false;
    }
}

bool isLedOn = false;
int clickNumber = 0;

void push(int btn)
{
#ifdef DEBUG
    Serial.println("Button " + String(btn) + " push");
#endif
    // btn is 1-based, so we need to subtract 1 to get the correct CC number
    sendMIDICommandList(midiButtons[btn - 1].push, midiButtons[btn - 1]);
}

void hold(int btn)
{
    if (midiButtons[btn - 1].flags.repeatOnHold)
    {
#ifdef DEBUG
        Serial.println("Button " + String(btn) + " hold");
#endif
        // btn is 1-based, so we need to subtract 1 to get the correct CC number
        sendMIDICommandList(midiButtons[btn - 1].hold, midiButtons[btn - 1]);
    }
}

void longPressStart(int btn)
{
    if (!midiButtons[btn - 1].flags.repeatOnHold)
    {
#ifdef DEBUG
        Serial.println("Button " + String(btn) + " long press start");
#endif
        // btn is 1-based, so we need to subtract 1 to get the correct CC number
        sendMIDICommandList(midiButtons[btn - 1].hold, midiButtons[btn - 1]);
    }
}

void doublepush(int btn)
{
#ifdef DEBUG
    Serial.println("Button " + String(btn) + " double push");
#endif
    // btn is 1-based, so we need to subtract 1 to get the correct CC number
    sendMIDICommandList(midiButtons[btn - 1].doublePush, midiButtons[btn - 1]);
}

bool serverStarted = false;

void setup()
{
    pinMode(LED_BUILTIN_AUX, OUTPUT);
    digitalWrite(LED_BUILTIN_AUX, LOW);

    // Set serial data rate for on MIDI OUT port
    MIDI_OUT_Serial.begin(31250);

#ifdef DEBUG
    Serial.begin(9600);
    Serial.println("MIDI Pedal ESP8266");
#endif

    serverStarted = serverStart();

    if (!serverStarted)
    {
        // switch led off
        digitalWrite(LED_BUILTIN_AUX, HIGH);
    }

    btn1.attachClick([]() {
        push(1);
    });

    btn1.attachDoubleClick([]() {
        doublepush(1);
    });

    btn1.attachDuringLongPress([]() {
        hold(1);
    });

    btn1.attachLongPressStart([]() {
        longPressStart(1);
    });

    btn2.attachClick([]() {
        push(2);
    });

    btn2.attachDoubleClick([]() {
        doublepush(2);
    });

    btn2.attachDuringLongPress([]() {
        hold(2);
    });

    btn2.attachLongPressStart([]() {
        longPressStart(2);
    });

    btn3.attachClick([]() {
        push(3);
    });

    btn3.attachDoubleClick([]() {
        doublepush(3);
    });

    btn3.attachDuringLongPress([]() {
        hold(3);
    });

    btn3.attachLongPressStart([]() {
        longPressStart(3);
    });

    btn4.attachClick([]() {
        push(4);
    });

    btn4.attachDoubleClick([]() {
        doublepush(4);
    });

    btn4.attachDuringLongPress([]() {
        hold(4);
    });

    btn4.attachLongPressStart([]() {
        longPressStart(4);
    });

    btn5.attachClick([]() {
        push(5);
    });

    btn5.attachDoubleClick([]() {
        doublepush(5);
    });

    btn5.attachDuringLongPress([]() {
        hold(5);
    });

    btn5.attachLongPressStart([]() {
        longPressStart(5);
    });

    btn6.attachClick([]() {
        push(6);
    });

    btn6.attachDoubleClick([]() {
        doublepush(6);
    });

    btn6.attachDuringLongPress([]() {
        hold(6);
    });

    initMIDIButtons();

    // Some default MIDI commands
    if (midiButtons[0].push.count == 0)
    {
        midiButtons[0].push = parseMIDICommands("CC 1 80 127, CC 1 80 0"); // TRK P/S
    }

    if (midiButtons[0].hold.count == 0)
    {
        midiButtons[0].hold = parseMIDICommands("CC 1 81 127, CC 1 81 0"); // TRK CLR
    }

    if (midiButtons[1].push.count == 0)
    {
        midiButtons[1].push = parseMIDICommands("CC 1 82 127, CC 1 82 0"); // UNDO/REDO
    }

    if (midiButtons[1].hold.count == 0)
    {
        midiButtons[1].hold = parseMIDICommands("CC 1 82 127, CC 1 82 0"); // UNDO/REDO
    }

    if (midiButtons[2].push.count == 0)
    {
        midiButtons[2].push = parseMIDICommands("CC 1 83 127, CC 1 83 0"); // RYTHM P/S
    }

    if (midiButtons[3].push.count == 0)
    {
        midiButtons[3].push = parseMIDICommands("CC 1 84 127, CC 1 84 0"); // TAP TEMPO
    }

    if (midiButtons[4].push.count == 0)
    {
        midiButtons[4].push = parseMIDICommands("VAR_INC 1 1, CC 1 85 VAR, CC 1 85 127"); // PATTERN
        midiButtons[4].hold = parseMIDICommands("VAR_INC 1 1, CC 1 85 VAR, CC 1 85 127"); // PATTERN
        midiButtons[4].flags.repeatOnHold = true;
        midiButtons[4].doublePush = parseMIDICommands("VAR_DEC 1 1, CC 1 85 VAR, CC 1 85 127"); // PATTERN
        midiButtons[4].var.min = 0;
        midiButtons[4].var.max = 56;
        midiButtons[4].var.value = 23;
    }

    if (midiButtons[5].push.count == 0)
    {
        midiButtons[5].push = parseMIDICommands("CC 1 86 127, CC 1 86 0"); // MEM INC
    }

    if (midiButtons[5].hold.count == 0)
    {
        midiButtons[5].hold = parseMIDICommands("CC 1 87 127,CC 1 87 0"); // MEM DEC
        midiButtons[5].flags.repeatOnHold = true;
    }
}

void loop()
{

    if (serverStarted)
    {
        MDNS.update();
        server.handleClient();
    }

    btn1.tick();
    btn2.tick();
    btn3.tick();
    btn4.tick();
    btn5.tick();
    btn6.tick();
}

String getContentType(String filename)
{ // convert the file extension to the MIME type
    if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    return "text/plain";
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
#ifdef DEBUG
    Serial.println("handleFileRead: " + path);
#endif
    if (path.endsWith("/"))
        path += "index.html";                        // If a folder is requested, send the index file
    const String contentType = getContentType(path); // Get the MIME type
    if (SPIFFS.exists(path))
    {                                                       // If the file exists
        File file = SPIFFS.open(path, "r");                 // Open it
        size_t sent = server.streamFile(file, contentType); // And send it to the client
        file.close();                                       // Then close the file again
        return true;
    }
#ifdef DEBUG
    Serial.println("\tFile Not Found");
#endif
    return false; // If the file doesn't exist, return false
}