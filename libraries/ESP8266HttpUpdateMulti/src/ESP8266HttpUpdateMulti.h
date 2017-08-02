#ifndef ESP8266_HTTP_UPDATE_MULTI_H
#define ESP8266_HTTP_UPDATE_MULTI_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>

#ifdef DEBUG_ESP_HTTP_UPDATE
#ifdef DEBUG_ESP_PORT
#define DEBUG_HTTP_UPDATE(...) DEBUG_ESP_PORT.printf( __VA_ARGS__ )
#endif
#endif

#ifndef DEBUG_HTTP_UPDATE
#define DEBUG_HTTP_UPDATE(...)
#endif

class ESP8266HttpUpdateMulti {
  public:
    enum HttpUpdateResult {
      RESULT_FAILURE,
      RESULT_SUCCESS_FIRMWARE,
      RESULT_SUCCESS_DATASET,
      RESULT_UP_TO_DATE
    };

    enum HttpUpdateError {
      ERROR_NONE,
      ERROR_HTTP_REQUEST,
      ERROR_HTTP_STATUS_CODE,
      ERROR_MISSING_HEADER_LENGTH,
      ERROR_MISSING_HEADER_TYPE,
      ERROR_MISSING_HEADER_MD5,
      ERROR_INVALID_HEADER_TYPE,
      ERROR_MAGIC_PEEK_FAILED,
      ERROR_INVALID_MAGIC_BYTES,
      ERROR_MAGIC_SIZE_MISMATCH,
      ERROR_UPDATE_BEGIN,
      ERROR_UPDATE_SET_MD5,
      ERROR_UPDATE_WRITE_STREAM,
      ERROR_UPDATE_END
    };

  private:
    HttpUpdateError m_lastError;

  public:
    ESP8266HttpUpdateMulti();
    ~ESP8266HttpUpdateMulti();

    inline HttpUpdateError getLastError() { return m_lastError; }
    inline const char* getLastErrorString() {
      switch(m_lastError) {
        case ERROR_NONE:
          return "None";
        case ERROR_HTTP_REQUEST:
          return "HTTP request failed";
        case ERROR_HTTP_STATUS_CODE:
          return "Invalid server HTTP status code";
        case ERROR_MISSING_HEADER_LENGTH:
          return "Missing [Content-Length] header";
        case ERROR_MISSING_HEADER_TYPE:
          return "Missing [X-Update-Type] header";
        case ERROR_MISSING_HEADER_MD5:
          return "Missing [X-Md5] header";
        case ERROR_INVALID_HEADER_TYPE:
          return "Invalid [X-Update-Type] header value";
        case ERROR_MAGIC_PEEK_FAILED:
          return "Magic bytes peek failed";
        case ERROR_INVALID_MAGIC_BYTES:
          return "Invalid magic bytes";
        case ERROR_MAGIC_SIZE_MISMATCH:
          return "Magic size mismatch";
        case ERROR_UPDATE_BEGIN:
          return "Error while executing Update.begin()";
        case ERROR_UPDATE_SET_MD5:
          return "Error while executing Update.setMD5()";
        case ERROR_UPDATE_WRITE_STREAM:
          return "Error while executing Update.writeStream()";
        case ERROR_UPDATE_END:
          return "Error while executing Update.end()";
        default:
          return "Unknown";
      }
    }

    HttpUpdateResult update(
      const String& p_url,
      const String& p_uuid,
      const String& p_firmwareVersion,
      const String& p_datasetVersion
    );

    HttpUpdateResult update(
      const String& p_host,
      uint16_t p_port,
      const String& p_path,
      const String& p_uuid,
      const String& p_firmwareVersion,
      const String& p_datasetVersion
    );

    HttpUpdateResult update(
      const String& p_url,
      const String& p_uuid,
      const String& p_firmwareVersion,
      const String& p_datasetVersion,
      const String& p_fingerprint
    );

    HttpUpdateResult update(
      const String& p_host,
      uint16_t p_port,
      const String& p_path,
      const String& p_uuid,
      const String& p_firmwareVersion,
      const String& p_datasetVersion,
      const String& p_fingerprint
    );

  private:
    HttpUpdateResult handleUpdate(
      HTTPClient& p_http,
      const String& p_uuid,
      const String& p_firmwareVersion,
      const String& p_datasetVersion
    );

    bool runUpdate(int p_command, Stream& p_in, uint32_t p_size, const String& p_md5);
};

#endif
