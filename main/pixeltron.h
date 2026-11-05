// Pixeltron: setup with the Pixels app, knob input and update reports.
//
// Built only with CONFIG_PIXELTRON. The contract with the app is
// pixels/packages/tenk_core/lib/src/setup_portal.dart; with the server,
// pixeltron-server API.md "Pixels App API" (device bootstrap) and the
// websocket messages in components/pixeltron/include/pxt_proto.h.
#pragma once

#include <esp_err.h>
#include <esp_http_server.h>
#include <esp_websocket_client.h>
#include <esp_wifi_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The setup hotspot's address; the app expects it here.
#define PXT_AP_IP_A 192
#define PXT_AP_IP_B 168
#define PXT_AP_IP_C 4
#define PXT_AP_IP_D 1
#define PXT_AP_IP_STR "192.168.4.1"

// Hotspot name "Pixeltron-FOX42", WPA2 with the setup code as password.
void pxt_ap_config(wifi_config_t *ap);

// Registers /api/info, /api/scan, /api/connect, /api/status and /api/done.
// Call before any wildcard handler is registered.
void pxt_portal_register(httpd_handle_t server);

// True once the device has a server key and websocket URL (it has been
// through setup); false means it should show the setup screen.
bool pxt_is_set_up(void);

// Draws the QR setup screen (stops the animation player first).
void pxt_show_setup_screen(void);

// Starts the knob task; messages go out on ws once it is connected.
void pxt_input_start(esp_websocket_client_handle_t ws);

// Brightness the server set, so the knob turns from there.
void pxt_brightness_from_server_level(int level);

// Firmware update progress for the app (percent < 0: unknown).
void pxt_report_ota(int percent, bool done, const char *error);

// The server refused the websocket handshake with this HTTP status. After a
// few 401/403/404s in a row the device was released: forget the server and
// go back to setup (shows E4 first).
void pxt_ws_handshake_status(int http_status);

#ifdef __cplusplus
}
#endif
