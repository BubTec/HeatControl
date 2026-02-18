#include "https_redirect.h"

#include <cstring>

#include <esp_http_server.h>
#include <esp_https_server.h>

namespace HeatControl {
namespace {

httpd_handle_t httpsRedirectServer = nullptr;

constexpr const char kHttpsRedirectCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDDTCCAfWgAwIBAgIUUf57wBm0fTE1L1BfecpBnH6fBh8wDQYJKoZIhvcNAQEL\n"
    "BQAwFjEUMBIGA1UEAwwLSGVhdENvbnRyb2wwHhcNMjYwMjE4MjA0NjQ0WhcNMzYw\n"
    "MjE2MjA0NjQ0WjAWMRQwEgYDVQQDDAtIZWF0Q29udHJvbDCCASIwDQYJKoZIhvcN\n"
    "AQEBBQADggEPADCCAQoCggEBANBmxLcciM73z2rYCWtndo3At8jYOGB6lCZZMbgu\n"
    "h4YQ6R9d4AJZWYwEoXI8nIIwXGaJlsqmEVaxQl17YaTtJSknJIhxVakOpyvZdu0B\n"
    "bZNARDTisua99VnZVTVnmCMI2ZYuSKVOgtfxmu56V1SJKoAz1G2ut2dIR07V+03x\n"
    "TQkFcnO7dD06fHQI+huLt84LekNQCqv9pSSNPVLNyP2LxsXkllhdFageeFNLZLC0\n"
    "oQCBbczIaQY71Wg7CXJYhpEwVtm3hSzPaAGx+T5imIP7vvvQzfh8mVop/zgJ3AiW\n"
    "fPg0tWbgeNA1udn5D8DvRCjEe0Y2Zd3vCSryajUF7pdbBxUCAwEAAaNTMFEwHQYD\n"
    "VR0OBBYEFHSwf+DaWnuFMjy4rWVo6l8IQFJGMB8GA1UdIwQYMBaAFHSwf+DaWnuF\n"
    "Mjy4rWVo6l8IQFJGMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEB\n"
    "AHLFlSXZoOWdmaXgdw1lG7vdqrUt81s2jmkfxrzO81e8ah78jqRw7KYrq6xSX9IJ\n"
    "Fy+28/LA0Dw/oUQFw101kRmL7LvKyu124NixlR3oob77JAiKNw0nPIeA2/dFfq9U\n"
    "h/Q9TgzIGhW/g1ZDpYGQvYoLPXe5MXXfPyPhPwO/WhuYlYeYcVDK+jnRzeHqvBSk\n"
    "0pivrgUJVVqjGvs7QuM62Vdmnu6AhzOjbs8soJn4QdUV2gIYM65u7kc6QrEpHFWI\n"
    "G+LSM6ZBI0wqJE55m2sU7rYNbPxV7xbA67E0IuxgzpvlqjcNFdSb0GyRl0wQNaxM\n"
    "xYxWbG1K+s/Ex1ZazBb65wc=\n"
    "-----END CERTIFICATE-----\n";

constexpr const char kHttpsRedirectKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDQZsS3HIjO989q\n"
    "2AlrZ3aNwLfI2DhgepQmWTG4LoeGEOkfXeACWVmMBKFyPJyCMFxmiZbKphFWsUJd\n"
    "e2Gk7SUpJySIcVWpDqcr2XbtAW2TQEQ04rLmvfVZ2VU1Z5gjCNmWLkilToLX8Zru\n"
    "eldUiSqAM9RtrrdnSEdO1ftN8U0JBXJzu3Q9Onx0CPobi7fOC3pDUAqr/aUkjT1S\n"
    "zcj9i8bF5JZYXRWoHnhTS2SwtKEAgW3MyGkGO9VoOwlyWIaRMFbZt4Usz2gBsfk+\n"
    "YpiD+7770M34fJlaKf84CdwIlnz4NLVm4HjQNbnZ+Q/A70QoxHtGNmXd7wkq8mo1\n"
    "Be6XWwcVAgMBAAECggEAAtQaQGf9PV3krU0QV6pYVBU1CV9R6JFRWHHmeFr9UOcC\n"
    "2v+hrjZ7PijD7jJ/Z07zuhqM9uQpIRWX0n/9s8jaM8RJvSZYpMpDn5c8g3v8+j4p\n"
    "jtQy3Gngnh8Shn6I9JTGq0CmkMPPh9HKKgl0DPwtnP7XF8QCzj6PZTUHS6gGRSSM\n"
    "/UgwSJmg3BkyaKalUuApzVviq3GQkkjfhBtklhlRcjOSDc8yzRag+rinn17wT83j\n"
    "IuVfO2MGHzM2T1XdC17ONXsA0l7CGZn3v75xzsYrUS1Mw6AC7tbw4djFU8TlIeAn\n"
    "ND4JO916xghwaMMTSK8bqDinvlRUH9dKkCyfhIxLsQKBgQD8Pa21UbbMGxsSKlAt\n"
    "S1yDCvZflnbnu+WaWIRtyon5FViTonn8Ldfj4IidZpfvBzAzNm4wLgWpD+D6EkBP\n"
    "LwfcRnuWQwnfRq4/LGSNpxnE0e1dKzsEqjYxoGkrKVjy5sYrU4ZKLJ78wovww3wJ\n"
    "gFS5AbA5qNMLDoq3DaxT9RuUuQKBgQDTgdac4FDCaeSxvELjJbojiQZXYgOkRPu7\n"
    "clOEgE81lKAVTuzaXc2u+B5YMMSTQMdjhIGdNol9TAYpshF80gMvhTb0IUte5U1N\n"
    "pt/O7+Jh9egP+lyeFXBbijNrgA7MwqLApOk6CR8d7u3JBfHZTmK1p0tTyN//yfla\n"
    "88eiET/PPQKBgQCzHfFeUnpmEdF1Ysqwf1VgUMaHNkeVYx42LilL9YlocToHDFdn\n"
    "Nf6aePVKIHI/cHFPzJUObX/jf70YlyFHmXQRfZOBLnWyMXTGs3VsCX8I/rF5eZtd\n"
    "QqldMDW+Sow5YJrUZWl0/p6fil7pR0erT4/aGFLVfwbuURM7zd/TdrhxMQKBgQCp\n"
    "6RwnWmyGQdtejQaOw7gM4/8cI7kZNfGkW+uL/ieju4n+lDDiG4kojlqSLls8kEWd\n"
    "RM17Jly9M12gEVTLGUtF2ZaT+Es8KKk5QF17OGp8l7edXlsZA9AHObalHXLGO3XT\n"
    "nKdf4AQHX/HWE9h94eKaW5K/9Bc+vVp8Hmq5X3ILYQKBgCRrXzZlV8wrASpowbKW\n"
    "mG4k3Kr/cQwcjU+Q7R1ec2+ptVn2pPTwmNKO2XHQoHIds0yuql3Fd1szk3UmEoTs\n"
    "aD6lXm/q7fRcCXhWq82R5mfttRpUOKGKJiGlY7N2MiNAPwxxei37G4v1oTOMe5li\n"
    "N4eYpTFCuyJoY1RBLQXMy8kW\n"
    "-----END PRIVATE KEY-----\n";

esp_err_t httpsRedirectHandler(httpd_req_t *request) {
  httpd_resp_set_status(request, "301 Moved Permanently");
  httpd_resp_set_hdr(request, "Location", "http://4.3.2.1/");
  httpd_resp_set_hdr(request, "Cache-Control", "no-cache, no-store, must-revalidate");
  static constexpr char kBody[] = "Redirecting to http://4.3.2.1/ ...";
  httpd_resp_send(request, kBody, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

}  // namespace

void startHttpsRedirectServer() {
  if (httpsRedirectServer != nullptr) {
    return;
  }

  httpd_ssl_config_t httpsConfig = HTTPD_SSL_CONFIG_DEFAULT();
  httpsConfig.cacert_pem = reinterpret_cast<const uint8_t *>(kHttpsRedirectCertPem);
  httpsConfig.cacert_len = std::strlen(kHttpsRedirectCertPem);
  httpsConfig.prvtkey_pem = reinterpret_cast<const uint8_t *>(kHttpsRedirectKeyPem);
  httpsConfig.prvtkey_len = std::strlen(kHttpsRedirectKeyPem);
  httpsConfig.httpd.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_ssl_start(&httpsRedirectServer, &httpsConfig) != ESP_OK) {
    httpsRedirectServer = nullptr;
    return;
  }

  static httpd_uri_t redirectGet = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = httpsRedirectHandler,
      .user_ctx = nullptr,
#ifdef CONFIG_HTTPD_WS_SUPPORT
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr,
#endif
  };

  static httpd_uri_t redirectHead = {
      .uri = "/*",
      .method = HTTP_HEAD,
      .handler = httpsRedirectHandler,
      .user_ctx = nullptr,
#ifdef CONFIG_HTTPD_WS_SUPPORT
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr,
#endif
  };

  httpd_register_uri_handler(httpsRedirectServer, &redirectGet);
  httpd_register_uri_handler(httpsRedirectServer, &redirectHead);
}

}  // namespace HeatControl
