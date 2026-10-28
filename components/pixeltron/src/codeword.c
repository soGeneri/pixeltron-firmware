#include <stdio.h>

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
  uint32_t h = mac_hash(mac, 0x5eu);
  // 6 x 5 bits = 30 bits of the hash.
  char code[7];
  for (int i = 0; i < 6; i++) code[i] = kAlphabet[(h >> (i * 5)) & 31];
  code[6] = '\0';
  snprintf(out, out_len, "%s", code);
}

void pxt_setup_url(const uint8_t mac[6], char* out, size_t out_len) {
  char code[7];
  pxt_setup_code(mac, code, sizeof(code));
  snprintf(out, out_len, PXT_SETUP_URL_PREFIX "%s", code);
}
