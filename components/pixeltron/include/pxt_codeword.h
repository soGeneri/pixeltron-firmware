// Per-device setup identifiers derived from the Wi-Fi MAC address.
//
//   code word   "FOX-42"   shown on the panel, in the BLE name and in the app,
//                          so people pair the device in front of them
//   setup code  "4F2A9K"   six characters in the setup link
//   setup URL   "HTTPS://PXT.IO/S/4F2A9K"   upper case so the QR code can
//                          use alphanumeric mode and stay at version 1
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PXT_SETUP_URL_PREFIX "HTTPS://PXT.IO/S/"

// out needs at least 8 bytes ("BEAR-42" + NUL).
void pxt_code_word(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least 7 bytes.
void pxt_setup_code(const uint8_t mac[6], char* out, size_t out_len);
// out needs at least 24 bytes.
void pxt_setup_url(const uint8_t mac[6], char* out, size_t out_len);

#ifdef __cplusplus
}
#endif
