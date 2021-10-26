#ifndef _ESPAsyncHTTPUpdateServer_H_
#define _ESPAsyncHTTPUpdateServer_H_

#include <ESPAsyncWebServer.h>

class AsyncHTTPUpdateServer
{
private:
	bool _serial_output;
	AsyncWebServer *_server;
	String _username;
	String _password;
	bool _authenticated;
	String _updaterError;
	bool _shouldreboot;

protected:
	void _setUpdaterError();
	void _receiveData(uint8_t *data, size_t len);

public:
	AsyncHTTPUpdateServer(bool serial_debug = false);

	void setup(AsyncWebServer *server)
	{
		setup(server, emptyString, emptyString);
	}

	void setup(AsyncWebServer *server, const String &path)
	{
		setup(server, path, emptyString, emptyString);
	}

	void setup(AsyncWebServer *server, const String &username, const String &password)
	{
		setup(server, "/update", username, password);
	}

	void setup(AsyncWebServer *server, const String &path, const String &username, const String &password);

	void updateCredentials(const String &username, const String &password)
	{
		_username = username;
		_password = password;
	}

	void handleReboot();
};

#endif
