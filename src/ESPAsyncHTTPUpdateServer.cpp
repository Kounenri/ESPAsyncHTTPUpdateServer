#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <flash_hal.h>
#include <FS.h>
#include <LittleFS.h>
#include "StreamString.h"
#include "ESPAsyncHTTPUpdateServer.h"

static const char successResponse[] PROGMEM =
	"<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

void AsyncHTTPUpdateServer::_setUpdaterError()
{
	if (_serial_output)
		Update.printError(Serial);
	StreamString str;
	Update.printError(str);
	_updaterError = str.c_str();
}

void AsyncHTTPUpdateServer::_receiveData(uint8_t *data, size_t len)
{
	if (len)
	{
		if (Update.write(data, len) != len)
		{
			_setUpdaterError();
		}
	}
}

AsyncHTTPUpdateServer::AsyncHTTPUpdateServer(bool serial_debug)
{
	_serial_output = serial_debug;
	_server = NULL;
	_username = emptyString;
	_password = emptyString;
	_authenticated = false;
}

void AsyncHTTPUpdateServer::setup(AsyncWebServer *server, const String &path, const String &username, const String &password)
{
	_server = server;
	_username = username;
	_password = password;

	// handler for the /update form page
	_server->on(
		path.c_str(), HTTP_GET, [&](AsyncWebServerRequest *request)
		{
			if (_username != emptyString && _password != emptyString && !request->authenticate(_username.c_str(), _password.c_str()))
				return request->requestAuthentication();
			request->send(LittleFS, F("/update.html"), String());
		});

	// handler for the /update form POST (once file upload finishes)
	_server->on(
		path.c_str(), HTTP_POST, [&](AsyncWebServerRequest *request)
		{
			if (!_authenticated)
				return request->requestAuthentication();
			if (Update.hasError())
			{
				request->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
			}
			else
			{
				request->client()->setNoDelay(true);
				request->send_P(200, PSTR("text/html"), successResponse);
				request->client()->stop();

				_shouldreboot = true;
			}
		},
		[&](AsyncWebServerRequest *request, const String &filename, const String &name, size_t index, uint8_t *data, size_t len, bool final)
		{
			// handler for the file upload, gets the sketch bytes, and writes
			// them through the Update object

			if (index == 0 && !final)
			{
				_updaterError.clear();
				if (_serial_output)
					Serial.setDebugOutput(true);

				_authenticated = (_username == emptyString || _password == emptyString || request->authenticate(_username.c_str(), _password.c_str()));
				if (!_authenticated)
				{
					if (_serial_output)
						Serial.printf("Unauthenticated Update\n");

					return;
				}

				WiFiUDP::stopAll();
				if (_serial_output)
					Serial.printf("Update: %s %s\n", filename.c_str(), name.c_str());

				Update.runAsync(true); // Important!!!

				if (name == "filesystem")
				{
					size_t fsSize = ((size_t)&_FS_end - (size_t)&_FS_start);
					close_all_fs();
					if (!Update.begin(fsSize, U_FS)) //start with max available size
					{
						_setUpdaterError();
					}
				}
				else
				{
					uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
					if (!Update.begin(maxSketchSpace, U_FLASH)) //start with max available size
					{
						_setUpdaterError();
					}
				}

				_receiveData(data, len); // need write data at first time
			}
			else if (_authenticated && index != 0 && !final && !_updaterError.length())
			{
				if (_serial_output)
					Serial.printf(".");

				_receiveData(data, len);
			}
			else if (_authenticated && final && !_updaterError.length())
			{
				_receiveData(data, len); // need write data on end

				if (!_updaterError.length())
				{
					if (Update.end(true)) //true to set the size to the current progress
					{
						if (_serial_output)
							Serial.printf("Update Success: %u\nRebooting...\n", index + len);
					}
					else
					{
						_setUpdaterError();
					}

					if (_serial_output)
						Serial.setDebugOutput(false);
				}
			}
			/*else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED)
			{
				Update.end();
				if (_serial_output)
					Serial.println("Update was aborted");
			}
			delay(0);*/
		});
}

void AsyncHTTPUpdateServer::handleReboot()
{
	if (_shouldreboot)
	{
		if (_serial_output)
			Serial.printf("HandleReboot rebooting...\n");

		_server->end();
		delay(100);
		ESP.restart();
	}
}
