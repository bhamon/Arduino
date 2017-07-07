#include <ESP.h>
#include <Updater.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP8266HttpUpdateMulti.h>
#include <StreamString.h>

ESP8266HttpUpdateMulti::ESP8266HttpUpdateMulti()
: m_lastError(ERROR_NONE) {
}

ESP8266HttpUpdateMulti::~ESP8266HttpUpdateMulti() {
}

ESP8266HttpUpdateMulti::HttpUpdateResult ESP8266HttpUpdateMulti::update(const String& p_url, const String& p_firmwareVersion, const String& p_datasetVersion) {
	HTTPClient http;
    http.begin(p_url);

    return handleUpdate(http, p_firmwareVersion, p_datasetVersion);
}

ESP8266HttpUpdateMulti::HttpUpdateResult ESP8266HttpUpdateMulti::update(const String& p_host, uint16_t p_port, const String& p_path, const String& p_firmwareVersion, const String& p_datasetVersion) {
	HTTPClient http;
	http.begin(p_host, p_port, p_path);

	return handleUpdate(http, p_firmwareVersion, p_datasetVersion);
}

ESP8266HttpUpdateMulti::HttpUpdateResult ESP8266HttpUpdateMulti::update(const String& p_url, const String& p_firmwareVersion, const String& p_datasetVersion, const String& p_fingerprint) {
	HTTPClient http;
	http.begin(p_url, p_fingerprint);

	return handleUpdate(http, p_firmwareVersion, p_datasetVersion);
}

ESP8266HttpUpdateMulti::HttpUpdateResult ESP8266HttpUpdateMulti::update(const String& p_host, uint16_t p_port, const String& p_path, const String& p_firmwareVersion, const String& p_datasetVersion, const String& p_fingerprint) {
	HTTPClient http;
    http.begin(p_host, p_port, p_path, p_fingerprint);

    return handleUpdate(http, p_firmwareVersion, p_datasetVersion);
}

ESP8266HttpUpdateMulti::HttpUpdateResult ESP8266HttpUpdateMulti::handleUpdate(HTTPClient& p_http, const String& p_firmwareVersion, const String& p_datasetVersion) {
	p_http.useHTTP10(true);
    p_http.setTimeout(8000);
    p_http.setUserAgent("ESP8266-Http-Update-Multi");
    p_http.addHeader("X-Mac-Address", WiFi.macAddress());
    p_http.addHeader("X-Firmware-Version", p_firmwareVersion);
    p_http.addHeader("X-Dataset-Version", p_datasetVersion);
	p_http.addHeader("X-Chip-Size", String(ESP.getFlashChipRealSize()));
    p_http.addHeader("X-Free-Space", String(ESP.getFreeSketchSpace()));

	const char* headerKeys[] = { "X-Update-Type", "X-Md5" };
    size_t headerKeysSize = sizeof(headerKeys) / sizeof(char*);

    // track these headers
    p_http.collectHeaders(headerKeys, headerKeysSize);

    int statusCode = p_http.GET();
	DEBUG_HTTP_UPDATE("[httpUpdateMulti] HTTP statusCode: %d\n", statusCode);

	if(statusCode <= 0) {
        DEBUG_HTTP_UPDATE("[httpUpdateMulti] HTTP error: %s\n", p_http.errorToString(statusCode).c_str());
        p_http.end();

		m_lastError = ERROR_HTTP_REQUEST;
        return RESULT_FAILURE;
    }

	switch(statusCode) {
		case HTTP_CODE_OK:{
			int length = p_http.getSize();
			if(length <= 0) {
				DEBUG_HTTP_UPDATE("[httpUpdateMulti] Missing [Content-Length] header\n");
				p_http.end();

				m_lastError = ERROR_MISSING_HEADER_LENGTH;
				return RESULT_FAILURE;
			}

			String updateType = p_http.header("X-Update-Type");
			if(!updateType.length()) {
				DEBUG_HTTP_UPDATE("[httpUpdateMulti] Missing [X-Update-Type] header\n");
				p_http.end();

				m_lastError = ERROR_MISSING_HEADER_TYPE;
				return RESULT_FAILURE;
			}

			String md5 = p_http.header("X-Md5");
			if(!md5.length()) {
				DEBUG_HTTP_UPDATE("[httpUpdateMulti] Missing [X-Md5] header\n");
				p_http.end();

				m_lastError = ERROR_MISSING_HEADER_MD5;
				return RESULT_FAILURE;
			}

			DEBUG_HTTP_UPDATE("[httpUpdateMulti] Server headers:\n");
			DEBUG_HTTP_UPDATE("[httpUpdateMulti]  - Content-Length: %d\n", length);
			DEBUG_HTTP_UPDATE("[httpUpdateMulti]  - X-Update-Type: %s\n", updateType.c_str());
			DEBUG_HTTP_UPDATE("[httpUpdateMulti]  - X-Md5: %s\n", md5.c_str());

			bool isUpdateTypeFirmware = updateType.equals("firmware");
			bool isUpdateTypeDataset = updateType.equals("dataset");
			if(!isUpdateTypeFirmware && !isUpdateTypeDataset) {
				DEBUG_HTTP_UPDATE("[httpUpdateMulti] Invalid [X-Update-Type] header value\n");
				p_http.end();

				m_lastError = ERROR_INVALID_HEADER_TYPE;
				return RESULT_FAILURE;
			}

			WiFiClient* tcp = p_http.getStreamPtr();

			WiFiUDP::stopAll();
			WiFiClient::stopAllExcept(tcp);

			delay(100);

			if(isUpdateTypeFirmware) {
				uint8_t buffer[4];
				if(tcp->peekBytes(&buffer[0], 4) != 4) {
					DEBUG_HTTP_UPDATE("[httpUpdateMulti] Unable to peek magic bytes\n");
					p_http.end();

					m_lastError = ERROR_MAGIC_PEEK_FAILED;
					return RESULT_FAILURE;
				}

				if(buffer[0] != 0xE9) {
					DEBUG_HTTP_UPDATE("[httpUpdateMulti] Invalid magic bytes\n");
					p_http.end();

					m_lastError = ERROR_INVALID_MAGIC_BYTES;
					return RESULT_FAILURE;
				}

				uint32_t binFlashSize = ESP.magicFlashChipSize((buffer[3] & 0xf0) >> 4);
				if(binFlashSize > ESP.getFlashChipRealSize()) {
					DEBUG_HTTP_UPDATE("[httpUpdateMulti] Magic size mismatch, got %d\n", binFlashSize);
					p_http.end();

					m_lastError = ERROR_MAGIC_SIZE_MISMATCH;
					return RESULT_FAILURE;
				}
			}

			int command = isUpdateTypeFirmware ? U_FLASH : U_SPIFFS;
			if(!runUpdate(command, *tcp, length, md5)) {
				DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update failed\n");
				p_http.end();

				return RESULT_FAILURE;
			}

			DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update OK\n");
			p_http.end();

			if(isUpdateTypeFirmware) {
				return RESULT_SUCCESS_FIRMWARE;
			} else {
				return RESULT_SUCCESS_DATASET;
			}

		}
		case HTTP_CODE_PRECONDITION_FAILED:
			p_http.end();
			return RESULT_UP_TO_DATE;
		default:
			DEBUG_HTTP_UPDATE("[httpUpdateMulti] Invalid server status code (invalid parameters?)\n");
			p_http.end();

			m_lastError = ERROR_HTTP_STATUS_CODE;
			return RESULT_FAILURE;
	}
}

bool ESP8266HttpUpdateMulti::runUpdate(int p_command, Stream& p_in, uint32_t p_size, const String& p_md5) {
	StreamString error;

    if(!Update.begin(p_size, p_command)) {
        Update.printError(error);
        error.trim();

        DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update begin failed (%s)\n", error.c_str());

		m_lastError = ERROR_UPDATE_BEGIN;
        return false;
    }

    if(!Update.setMD5(p_md5.c_str())) {
		Update.printError(error);
        error.trim();

        DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update setMD5 failed (%s)\n", error.c_str());

		m_lastError = ERROR_UPDATE_SET_MD5;
        return false;
    }

    if(Update.writeStream(p_in) != p_size) {
		Update.printError(error);
        error.trim();

        DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update writeStream failed (%s)\n", error.c_str());

		m_lastError = ERROR_UPDATE_WRITE_STREAM;
        return false;
    }

    if(!Update.end()) {
		Update.printError(error);
        error.trim();

        DEBUG_HTTP_UPDATE("[httpUpdateMulti] Update end failed (%s)\n", error.c_str());

		m_lastError = ERROR_UPDATE_END;
        return false;
    }

    return true;
}
