// Per-device setup identifiers derived from the Wi-Fi MAC address.
//
//   code word     "FOX-42"     shown on the panel and in the app, so people
//                              pair the device in front of them
//   setup code    "4F2A9KQ7"   eight characters on the label and in the setup
//                              link; also the setup hotspot's WPA2 password
//                              (which needs >= 8)
//   hotspot SSID  "Pixeltron-FOX42"
//   Wi-Fi QR      "WIFI:T:WPA;S:Pixeltron-FOX42;P:4F2A9KQ7;;" on the panel;
//                 the app scans it, and a phone camera can join from it
//   setup URL     "HTTPS://TENTHOUSANDPIXELS.COM/S/4F2A9KQ7"   for links;
//                 upper case so a QR code can use alphanumeric mode
//                 (40 characters fits a version 2 code)
//
// pixeltron-server (internal/server/codeword.go) and the app's simulator
// (tools/mock_server/lib/device_sim.dart) derive the same values; keep the
// test vectors in host/test_pixeltron.c in step with theirs.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PXT_SETUP_URL_PREFIX "HTTPS://TENTHOUSANDPIXELS.COM/S/"
#define PXT_HOTSPOT_PREFIX "Pixeltron-"
#define PXT_SETUP_CODE_LEN 8
// Buffer size for pxt_setup_url(): prefix + 8-character code + NUL, rounded up.
#define PXT_SETUP_URL_MAX 48

// out needs at least 8 bytes ("BEAR-42" + NUL).
void pxt_code_word(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least PXT_SETUP_CODE_LEN + 1 bytes.
void pxt_setup_code(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least PXT_SETUP_URL_MAX bytes.
void pxt_setup_url(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least 17 bytes ("Pixeltron-BEAR42" + NUL).
void pxt_hotspot_ssid(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least 48 bytes.
void pxt_wifi_qr(const uint8_t mac[6], char* out, size_t out_len);

#ifdef __cplusplus
}
#endif
