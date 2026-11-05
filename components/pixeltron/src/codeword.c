#include <stdio.h>
#include <string.h>

#include "pxt_codeword.h"

// Short animal names that read cleanly in the 5x7 font (4 letters max, so
// they fit beside the QR code) and are hard to mishear.
static const char* const kWords[32] = {
    "FOX",  "OWL",  "CAT",  "BEE",  "ELK",  "YAK",  "EMU",  "APE",
    "BAT",  "COD",  "DOG",  "EEL",  "GNU",  "HEN",  "JAY",  "KOI",
    "RAM",  "RAY",  "BEAR", "CRAB", "CROW", "DEER", "DOVE", "DUCK",
    "FROG", "GOAT", "HARE", "LION", "LYNX", "MOLE", "NEWT", "PUMA",
};

// No 0/O or 1/I, so the code can be read aloud or typed from the label.
static const char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

// FNV-1a, salted so the code word and setup code don't share bits.
static uint32_t mac_hash(const uint8_t mac[6], uint32_t salt) {
  uint32_t h = 2166136261u ^ salt;
  for (int i = 0; i < 6; i++) {
    h ^= mac[i];
    h *= 16777619u;
  }
  return h;
}

void pxt_code_word(const uint8_t mac[6], char* out, size_t out_len) {
  uint32_t h = mac_hash(mac, 0x77u);
  snprintf(out, out_len, "%s-%02u", kWords[h & 31],
           (unsigned)(10 + (h >> 5) % 90));
}

void pxt_setup_code(const uint8_t mac[6], char* out, size_t out_len) {
  // 40 bits: six characters from one hash (30 bits), two from another.
  uint32_t h = mac_hash(mac, 0x5eu);
  uint32_t h2 = mac_hash(mac, 0x5fu);
  char code[PXT_SETUP_CODE_LEN + 1];
  for (int i = 0; i < 6; i++) code[i] = kAlphabet[(h >> (i * 5)) & 31];
  for (int i = 0; i < 2; i++) code[6 + i] = kAlphabet[(h2 >> (i * 5)) & 31];
  code[PXT_SETUP_CODE_LEN] = '\0';
  snprintf(out, out_len, "%s", code);
}

void pxt_setup_url(const uint8_t mac[6], char* out, size_t out_len) {
  char code[PXT_SETUP_CODE_LEN + 1];
  pxt_setup_code(mac, code, sizeof(code));
  snprintf(out, out_len, PXT_SETUP_URL_PREFIX "%s", code);
}

void pxt_hotspot_ssid(const uint8_t mac[6], char* out, size_t out_len) {
  char word[16];
  pxt_code_word(mac, word, sizeof(word));
  char* dash = strchr(word, '-');
  if (dash) memmove(dash, dash + 1, strlen(dash));
  snprintf(out, out_len, PXT_HOTSPOT_PREFIX "%s", word);
}

void pxt_wifi_qr(const uint8_t mac[6], char* out, size_t out_len) {
  // SSID and code use letters and digits only, so nothing needs escaping.
  char ssid[24], code[PXT_SETUP_CODE_LEN + 1];
  pxt_hotspot_ssid(mac, ssid, sizeof(ssid));
  pxt_setup_code(mac, code, sizeof(code));
  snprintf(out, out_len, "WIFI:T:WPA;S:%s;P:%s;;", ssid, code);
}
