#include "pixeltron.h"

#include <cJSON.h>
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/netdb.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include "ap.h"
#include "display.h"
#include "gfx.h"
#include "nvs_settings.h"
#include "pxt_brightness.h"
#include "pxt_codeword.h"
#include "pxt_gesture.h"
#include "pxt_proto.h"
#include "pxt_screens.h"
#include "version.h"
#include "wifi.h"

#define TAG "PXT"

static void device_mac(uint8_t mac[6]) { esp_read_mac(mac, ESP_MAC_WIFI_STA); }

// The server's id for this device: "pxt-" + code word, lower case, no dash.
static void device_id(char *out, size_t n) {
  uint8_t mac[6];
  device_mac(mac);
  char word[16];
  pxt_code_word(mac, word, sizeof(word));
  size_t j = 0;
  char compact[16];
  for (size_t i = 0; word[i] && j + 1 < sizeof(compact); i++) {
    char c = word[i];
    if (c == '-') continue;
    compact[j++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
  }
  compact[j] = '\0';
  snprintf(out, n, "pxt-%s", compact);
}

static void show_fb(const pxt_fb_t *fb) {
  display_draw(&fb->px[0][0][0], PXT_FB_W, PXT_FB_H, 3, 0, 1, 2);
  display_flip();
}

// ---------------------------------------------------------------- hotspot

void pxt_ap_config(wifi_config_t *ap) {
  uint8_t mac[6];
  device_mac(mac);
  char ssid[24], code[PXT_SETUP_CODE_LEN + 1];
  pxt_hotspot_ssid(mac, ssid, sizeof(ssid));
  pxt_setup_code(mac, code, sizeof(code));

  strlcpy((char *)ap->ap.ssid, ssid, sizeof(ap->ap.ssid));
  ap->ap.ssid_len = strlen(ssid);
  strlcpy((char *)ap->ap.password, code, sizeof(ap->ap.password));
  ap->ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap->ap.pmf_cfg.required = false;
  ESP_LOGI(TAG, "Setup hotspot %s", ssid);
}

bool pxt_is_set_up(void) {
  char key[MAX_API_KEY_LEN + 1] = {0};
  const char *url = nvs_get_image_url();
  return nvs_get_api_key(key, sizeof(key)) == ESP_OK && key[0] != '\0' &&
         url != NULL && url[0] != '\0';
}

void pxt_show_setup_screen(void) {
  static pxt_fb_t fb;
  uint8_t mac[6];
  device_mac(mac);
  char word[16], qr[64];
  pxt_code_word(mac, word, sizeof(word));
  pxt_wifi_qr(mac, qr, sizeof(qr));
  gfx_stop();
  if (pxt_screen_setup(&fb, qr, word)) {
    show_fb(&fb);
  } else {
    ESP_LOGE(TAG, "Setup QR didn't fit: %s", qr);
  }
}

// ---------------------------------------------------------- setup portal
//
// Stages and errors as in the app's SetupStatus (setup_portal.dart).

typedef enum {
  STAGE_IDLE,
  STAGE_JOINING,
  STAGE_ADDRESS,
  STAGE_INTERNET,
  STAGE_CLOUD,
  STAGE_OK,
} stage_t;

static const char *const kStageNames[] = {"idle",     "joining", "address",
                                          "internet", "cloud",   "ok"};

static struct {
  SemaphoreHandle_t lock;
  int attempt;
  stage_t stage;
  const char *error;  // "AUTH_FAIL", "NO_AP_FOUND", "NO_INTERNET", "CAPTIVE"
  char claim_code[16];
  char device_id[40];
  char ssid[33];
  char password[65];
  char server[160];
} s_portal;

static void portal_set(int attempt, stage_t stage, const char *error) {
  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  if (attempt == s_portal.attempt) {
    s_portal.stage = stage;
    s_portal.error = error;
  }
  xSemaphoreGive(s_portal.lock);
  ESP_LOGI(TAG, "Setup %s%s%s", kStageNames[stage], error ? " " : "",
           error ? error : "");
}

static bool portal_current(int attempt) {
  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  bool cur = attempt == s_portal.attempt;
  xSemaphoreGive(s_portal.lock);
  return cur;
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root) {
  char *body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t err = httpd_resp_sendstr(req, body ? body : "{}");
  free(body);
  return err;
}

static esp_err_t api_info(httpd_req_t *req) {
  uint8_t mac[6];
  device_mac(mac);
  char word[16];
  pxt_code_word(mac, word, sizeof(word));
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "product", "pixeltron");
  cJSON_AddStringToObject(root, "model", "pixeltron-64x32");
  cJSON_AddStringToObject(root, "code_word", word);
  cJSON_AddStringToObject(root, "firmware", FIRMWARE_VERSION);
  cJSON_AddBoolToObject(root, "claimed", pxt_is_set_up());
  return send_json(req, root);
}

static const char *security_name(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:
    case WIFI_AUTH_OWE:
      return "open";
    case WIFI_AUTH_WPA2_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENT_192:
    case WIFI_AUTH_WPA_ENTERPRISE:
      return "enterprise";
    default:
      return "wpa";
  }
}

static esp_err_t api_scan(httpd_req_t *req) {
  wifi_scan_config_t cfg = {0};
  esp_err_t err = esp_wifi_scan_start(&cfg, true);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "503 Service Unavailable");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", "scan failed");
    return send_json(req, root);
  }
  uint16_t n = 24;
  wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
  if (recs == NULL) return httpd_resp_send_500(req);
  esp_wifi_scan_get_ap_records(&n, recs);

  cJSON *root = cJSON_CreateObject();
  cJSON *list = cJSON_AddArrayToObject(root, "networks");
  for (int i = 0; i < n; i++) {
    const char *ssid = (const char *)recs[i].ssid;
    if (ssid[0] == '\0') continue;  // hidden
    // One entry per name, the strongest.
    bool seen = false;
    for (int j = 0; j < n && !seen; j++) {
      if (j != i && strcmp(ssid, (const char *)recs[j].ssid) == 0 &&
          (recs[j].rssi > recs[i].rssi ||
           (recs[j].rssi == recs[i].rssi && j < i))) {
        seen = true;
      }
    }
    if (seen) continue;
    cJSON *net = cJSON_CreateObject();
    cJSON_AddStringToObject(net, "ssid", ssid);
    cJSON_AddNumberToObject(net, "rssi", recs[i].rssi);
    cJSON_AddStringToObject(net, "security", security_name(recs[i].authmode));
    cJSON_AddItemToArray(list, net);
  }
  free(recs);
  return send_json(req, root);
}

static esp_err_t api_status(httpd_req_t *req) {
  cJSON *root = cJSON_CreateObject();
  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  stage_t stage = s_portal.stage;
  const char *error = s_portal.error;
  char code[16], id[40];
  strlcpy(code, s_portal.claim_code, sizeof(code));
  strlcpy(id, s_portal.device_id, sizeof(id));
  xSemaphoreGive(s_portal.lock);

  cJSON_AddStringToObject(root, "stage", kStageNames[stage]);
  if (error) cJSON_AddStringToObject(root, "error", error);
  if (code[0]) cJSON_AddStringToObject(root, "claim_code", code);
  if (id[0]) cJSON_AddStringToObject(root, "device_id", id);
  return send_json(req, root);
}

static void restart_task(void *arg) {
  vTaskDelay(pdMS_TO_TICKS(800));  // let the response go out
  ESP_LOGI(TAG, "Setup done, restarting");
  esp_restart();
}

static esp_err_t api_done(httpd_req_t *req) {
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  xTaskCreate(restart_task, "pxt_restart", 2048, NULL, 5, NULL);
  return ESP_OK;
}

// --- the connect attempt

static const char *join_error(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "AUTH_FAIL";
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
      return "NO_AP_FOUND";
    default:
      return NULL;
  }
}

// host of "https://host:port/path" into out
static bool url_host(const char *url, char *out, size_t n) {
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  size_t i = 0;
  while (p[i] && p[i] != '/' && p[i] != ':' && i + 1 < n) {
    out[i] = p[i];
    i++;
  }
  out[i] = '\0';
  return i > 0;
}

typedef struct {
  int status;
  char *body;
  int len;
  bool html;
} http_result_t;

static esp_err_t post_json(const char *url, const char *json,
                           http_result_t *r) {
  memset(r, 0, sizeof(*r));
  esp_http_client_config_t cfg = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 15000,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .disable_auto_redirect = true,  // a redirect means a captive portal
  };
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (c == NULL) return ESP_FAIL;
  esp_http_client_set_header(c, "Content-Type", "application/json");
  esp_err_t err = esp_http_client_open(c, strlen(json));
  if (err == ESP_OK) {
    esp_http_client_write(c, json, strlen(json));
    esp_http_client_fetch_headers(c);
    r->status = esp_http_client_get_status_code(c);
    char *ctype = NULL;
    if (esp_http_client_get_header(c, "Content-Type", &ctype) == ESP_OK &&
        ctype && strstr(ctype, "text/html")) {
      r->html = true;
    }
    const int cap = 2048;
    r->body = calloc(1, cap + 1);
    if (r->body) {
      int got;
      while (r->len < cap && (got = esp_http_client_read(c, r->body + r->len,
                                                         cap - r->len)) > 0) {
        r->len += got;
      }
    }
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return err;
}

static void connect_task(void *arg) {
  const int attempt = (int)(intptr_t)arg;
  char ssid[33], password[65], server[160];
  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  strlcpy(ssid, s_portal.ssid, sizeof(ssid));
  strlcpy(password, s_portal.password, sizeof(password));
  strlcpy(server, s_portal.server, sizeof(server));
  xSemaphoreGive(s_portal.lock);

  // 1. Join the home network.
  wifi_connect_to(ssid, password);
  int64_t deadline = esp_timer_get_time() + 25 * 1000000LL;
  while (!wifi_is_connected()) {
    if (!portal_current(attempt)) goto out;
    const char *err = join_error(wifi_last_disconnect_reason());
    if (err) {
      portal_set(attempt, STAGE_JOINING, err);
      goto out;
    }
    if (esp_timer_get_time() > deadline) {
      portal_set(attempt, STAGE_JOINING,
                 wifi_last_disconnect_reason() ? "AUTH_FAIL" : "NO_AP_FOUND");
      goto out;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  portal_set(attempt, STAGE_ADDRESS, NULL);

  // 2. Internet: can we resolve the server?
  portal_set(attempt, STAGE_INTERNET, NULL);
  char host[96];
  if (!url_host(server, host, sizeof(host))) {
    portal_set(attempt, STAGE_INTERNET, "NO_INTERNET");
    goto out;
  }
  bool resolved = false;
  for (int i = 0; i < 4 && !resolved; i++) {
    struct addrinfo hints = {.ai_family = AF_UNSPEC,
                             .ai_socktype = SOCK_STREAM};
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, NULL, &hints, &res) == 0 && res) {
      resolved = true;
      freeaddrinfo(res);
    } else {
      vTaskDelay(pdMS_TO_TICKS(1500));
    }
  }
  if (!resolved) {
    portal_set(attempt, STAGE_INTERNET, "NO_INTERNET");
    goto out;
  }

  // 3. Cloud: register for a key and a claim code.
  portal_set(attempt, STAGE_CLOUD, NULL);
  uint8_t mac[6];
  device_mac(mac);
  char mac_str[18], url[200], body[160];
  snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  size_t sl = strlen(server);
  while (sl > 0 && server[sl - 1] == '/') server[--sl] = '\0';
  snprintf(url, sizeof(url), "%s/app/v0/device/register", server);
  snprintf(body, sizeof(body),
           "{\"mac\":\"%s\",\"product\":\"pixeltron\",\"firmware\":\"%s\"}",
           mac_str, FIRMWARE_VERSION);

  http_result_t r = {0};
  for (int i = 0; i < 3; i++) {
    free(r.body);
    if (post_json(url, body, &r) == ESP_OK && r.status > 0 && r.status < 500)
      break;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  if (!portal_current(attempt)) goto out_free;

  if ((r.status >= 300 && r.status < 400) || (r.status == 200 && r.html)) {
    portal_set(attempt, STAGE_INTERNET, "CAPTIVE");
    goto out_free;
  }
  if (r.status == 409) {
    // Already claimed (reconnecting to a new network): keep the key we have.
    nvs_set_ssid(ssid);
    nvs_set_password(password);
    nvs_save_settings();
    char id[40];
    device_id(id, sizeof(id));
    xSemaphoreTake(s_portal.lock, portMAX_DELAY);
    if (attempt == s_portal.attempt) {
      s_portal.claim_code[0] = '\0';
      strlcpy(s_portal.device_id, id, sizeof(s_portal.device_id));
    }
    xSemaphoreGive(s_portal.lock);
    portal_set(attempt, STAGE_OK, NULL);
    // If the app never says /api/done, close the hotspot on our own.
    ap_start_shutdown_timer();
    goto out_free;
  }
  cJSON *json = (r.status == 200 && r.body) ? cJSON_Parse(r.body) : NULL;
  const cJSON *id = cJSON_GetObjectItem(json, "device_id");
  const cJSON *key = cJSON_GetObjectItem(json, "api_key");
  const cJSON *code = cJSON_GetObjectItem(json, "claim_code");
  const cJSON *ws = cJSON_GetObjectItem(json, "ws_url");
  if (!cJSON_IsString(id) || !cJSON_IsString(key) || !cJSON_IsString(code) ||
      !cJSON_IsString(ws)) {
    ESP_LOGE(TAG, "Register failed: HTTP %d", r.status);
    cJSON_Delete(json);
    portal_set(attempt, STAGE_CLOUD,
               json || r.status == 200 ? "CAPTIVE" : "NO_INTERNET");
    goto out_free;
  }

  // Keep the network and the server; the next boot connects for real.
  nvs_set_ssid(ssid);
  nvs_set_password(password);
  nvs_set_api_key(key->valuestring);
  nvs_set_image_url(ws->valuestring);
  nvs_save_settings();

  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  if (attempt == s_portal.attempt) {
    strlcpy(s_portal.claim_code, code->valuestring,
            sizeof(s_portal.claim_code));
    strlcpy(s_portal.device_id, id->valuestring, sizeof(s_portal.device_id));
  }
  xSemaphoreGive(s_portal.lock);
  cJSON_Delete(json);
  portal_set(attempt, STAGE_OK, NULL);
  // If the app never says /api/done, close the hotspot on our own.
  ap_start_shutdown_timer();

out_free:
  free(r.body);
out:
  vTaskDelete(NULL);
}

static esp_err_t api_connect(httpd_req_t *req) {
  char buf[512];
  int len = req->content_len;
  if (len <= 0 || len >= (int)sizeof(buf)) {
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
  }
  int got = 0;
  while (got < len) {
    int r = httpd_req_recv(req, buf + got, len - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (r <= 0)
      return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    got += r;
  }
  buf[got] = '\0';

  cJSON *json = cJSON_Parse(buf);
  const cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
  const cJSON *password = cJSON_GetObjectItem(json, "password");
  const cJSON *server = cJSON_GetObjectItem(json, "server_url");
  if (!cJSON_IsString(ssid) || ssid->valuestring[0] == '\0' ||
      strlen(ssid->valuestring) > 32 ||
      (cJSON_IsString(password) && strlen(password->valuestring) > 64)) {
    cJSON_Delete(json);
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
  }

  int attempt;
  xSemaphoreTake(s_portal.lock, portMAX_DELAY);
  attempt = ++s_portal.attempt;
  s_portal.stage = STAGE_JOINING;
  s_portal.error = NULL;
  s_portal.claim_code[0] = '\0';
  s_portal.device_id[0] = '\0';
  strlcpy(s_portal.ssid, ssid->valuestring, sizeof(s_portal.ssid));
  strlcpy(s_portal.password,
          cJSON_IsString(password) ? password->valuestring : "",
          sizeof(s_portal.password));
  strlcpy(s_portal.server,
          (cJSON_IsString(server) && server->valuestring[0])
              ? server->valuestring
              : CONFIG_PXT_SERVER_URL,
          sizeof(s_portal.server));
  xSemaphoreGive(s_portal.lock);
  cJSON_Delete(json);

  if (xTaskCreate(connect_task, "pxt_connect", 8192, (void *)(intptr_t)attempt,
                  5, NULL) != pdPASS) {
    return httpd_resp_send_500(req);
  }
  httpd_resp_set_status(req, "202 Accepted");
  return httpd_resp_send(req, NULL, 0);
}

void pxt_portal_register(httpd_handle_t server) {
  if (s_portal.lock == NULL) s_portal.lock = xSemaphoreCreateMutex();
  const httpd_uri_t uris[] = {
      {.uri = "/api/info", .method = HTTP_GET, .handler = api_info},
      {.uri = "/api/scan", .method = HTTP_GET, .handler = api_scan},
      {.uri = "/api/connect", .method = HTTP_POST, .handler = api_connect},
      {.uri = "/api/status", .method = HTTP_GET, .handler = api_status},
      {.uri = "/api/done", .method = HTTP_POST, .handler = api_done},
  };
  for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
    httpd_register_uri_handler(server, &uris[i]);
  }
  ESP_LOGI(TAG, "Setup portal API ready at http://" PXT_AP_IP_STR "/api");
}

// ------------------------------------------------------------- knob input

static esp_websocket_client_handle_t s_ws;
static pxt_brightness_t s_bright;
static SemaphoreHandle_t s_bright_lock;

static void ws_send(const char *msg, int len) {
  if (len <= 0 || s_ws == NULL || !esp_websocket_client_is_connected(s_ws))
    return;
  if (esp_websocket_client_send_text(s_ws, msg, len, pdMS_TO_TICKS(1000)) < 0) {
    ESP_LOGW(TAG, "Send failed: %s", msg);
  }
}

void pxt_brightness_from_server_level(int level) {
  if (s_bright_lock == NULL) return;
  xSemaphoreTake(s_bright_lock, portMAX_DELAY);
  pxt_brightness_from_server(&s_bright, level);
  xSemaphoreGive(s_bright_lock);
}

static pcnt_unit_handle_t encoder_start(void) {
#if CONFIG_PXT_ENCODER_A_PIN >= 0 && CONFIG_PXT_ENCODER_B_PIN >= 0
  pcnt_unit_config_t unit_cfg = {.high_limit = 10000, .low_limit = -10000};
  pcnt_unit_handle_t unit = NULL;
  if (pcnt_new_unit(&unit_cfg, &unit) != ESP_OK) return NULL;
  pcnt_glitch_filter_config_t filter = {.max_glitch_ns = 1000};
  pcnt_unit_set_glitch_filter(unit, &filter);

  pcnt_chan_config_t a_cfg = {.edge_gpio_num = CONFIG_PXT_ENCODER_A_PIN,
                              .level_gpio_num = CONFIG_PXT_ENCODER_B_PIN};
  pcnt_chan_config_t b_cfg = {.edge_gpio_num = CONFIG_PXT_ENCODER_B_PIN,
                              .level_gpio_num = CONFIG_PXT_ENCODER_A_PIN};
  pcnt_channel_handle_t a = NULL, b = NULL;
  pcnt_new_channel(unit, &a_cfg, &a);
  pcnt_new_channel(unit, &b_cfg, &b);
  // Quadrature decoding (4 counts per full cycle).
  pcnt_channel_set_edge_action(a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                               PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                               PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  gpio_set_pull_mode(CONFIG_PXT_ENCODER_A_PIN, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(CONFIG_PXT_ENCODER_B_PIN, GPIO_PULLUP_ONLY);
  pcnt_unit_enable(unit);
  pcnt_unit_clear_count(unit);
  pcnt_unit_start(unit);
  ESP_LOGI(TAG, "Knob encoder on GPIO %d/%d", CONFIG_PXT_ENCODER_A_PIN,
           CONFIG_PXT_ENCODER_B_PIN);
  return unit;
#else
  return NULL;
#endif
}

static void factory_reset(void) {
  static pxt_fb_t fb;
  ESP_LOGW(TAG, "Knob held 10 s: factory reset");
  gfx_stop();
  pxt_screen_resetting(&fb);
  show_fb(&fb);
  vTaskDelay(pdMS_TO_TICKS(1500));  // let the hold message go out
  nvs_flash_erase();
  esp_restart();
}

static void input_task(void *arg) {
  pxt_gesture_state_t gesture;
  pxt_proto_t proto;
  pxt_gesture_init(&gesture);
  pxt_proto_init(&proto);
  pcnt_unit_handle_t encoder = encoder_start();
  char msg[64];

  for (;;) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

#if CONFIG_BUTTON_PIN >= 0
    bool pressed = gpio_get_level(CONFIG_BUTTON_PIN) == 0;  // active low
    pxt_gesture_t g = pxt_gesture_update(&gesture, pressed, now);
    ws_send(msg, pxt_proto_gesture(&proto, g, msg, sizeof(msg)));
    ws_send(msg, pxt_proto_countdown(&proto,
                                     pxt_gesture_reset_countdown(&gesture, now),
                                     msg, sizeof(msg)));
    if (g != PXT_GESTURE_NONE) {
      ESP_LOGI(TAG, "Knob: %s", pxt_gesture_name(g));
    }
    if (g == PXT_GESTURE_RESET) factory_reset();
#endif

    if (encoder) {
      int count = 0;
      pcnt_unit_get_count(encoder, &count);
      int detents = count / CONFIG_PXT_ENCODER_COUNTS_PER_DETENT;
      if (detents != 0) {
        pcnt_unit_clear_count(encoder);
        xSemaphoreTake(s_bright_lock, portMAX_DELAY);
        int level = pxt_brightness_turn(&s_bright, detents, now);
        xSemaphoreGive(s_bright_lock);
        display_set_brightness((uint8_t)level);
      }
    }
    int report;
    xSemaphoreTake(s_bright_lock, portMAX_DELAY);
    bool due = pxt_brightness_take_report(&s_bright, now, &report);
    xSemaphoreGive(s_bright_lock);
    if (due) {
      nvs_set_brightness((uint8_t)report);
      ws_send(msg, pxt_proto_brightness(report, msg, sizeof(msg)));
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void pxt_input_start(esp_websocket_client_handle_t ws) {
  s_ws = ws;
  if (s_bright_lock == NULL) s_bright_lock = xSemaphoreCreateMutex();
  pxt_brightness_init(&s_bright, nvs_get_brightness());
  xTaskCreate(input_task, "pxt_input", 4096, NULL, 5, NULL);
}

// ------------------------------------------------------- updates & release

void pxt_report_ota(int percent, bool done, const char *error) {
  char msg[96];
  ws_send(msg, pxt_proto_ota(percent, done, error, msg, sizeof(msg)));
}

// The server refuses the websocket (404) both before the app has claimed the
// device and after its owner released it. Only the second means "forget the
// server": ask GET /app/v0/device/claim with our key, which answers
// {"claimed":false} while waiting and 401 once released.
typedef enum { LINK_UNKNOWN, LINK_WAITING, LINK_CLAIMED, LINK_RELEASED } link_t;

static link_t check_link(void) {
  const char *ws = nvs_get_image_url();
  char key[MAX_API_KEY_LEN + 1] = {0};
  if (ws == NULL || nvs_get_api_key(key, sizeof(key)) != ESP_OK || !key[0]) {
    return LINK_UNKNOWN;
  }
  // ws://host:port/<id>/ws -> http://host:port/app/v0/device/claim
  const char *scheme = strncmp(ws, "wss://", 6) == 0 ? "https://" : "http://";
  const char *host = strstr(ws, "://");
  if (host == NULL) return LINK_UNKNOWN;
  host += 3;
  const char *slash = strchr(host, '/');
  int host_len = slash ? (int)(slash - host) : (int)strlen(host);
  char url[200], auth[16 + MAX_API_KEY_LEN];
  snprintf(url, sizeof(url), "%s%.*s/app/v0/device/claim", scheme, host_len,
           host);
  snprintf(auth, sizeof(auth), "Bearer %s", key);

  esp_http_client_config_t cfg = {
      .url = url,
      .timeout_ms = 10000,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (c == NULL) return LINK_UNKNOWN;
  esp_http_client_set_header(c, "Authorization", auth);
  link_t result = LINK_UNKNOWN;
  if (esp_http_client_open(c, 0) == ESP_OK &&
      esp_http_client_fetch_headers(c) >= 0) {
    int status = esp_http_client_get_status_code(c);
    char body[128] = {0};
    esp_http_client_read(c, body, sizeof(body) - 1);
    if (status == 401) {
      result = LINK_RELEASED;
    } else if (status == 200) {
      result = strstr(body, "\"claimed\":true") ? LINK_CLAIMED : LINK_WAITING;
    }
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return result;
}

static volatile bool s_link_check_running = false;

static void link_check_task(void *arg) {
  link_t link = check_link();
  ESP_LOGI(TAG, "Link check: %s",
           link == LINK_RELEASED  ? "released"
           : link == LINK_WAITING ? "waiting for the app to claim"
           : link == LINK_CLAIMED ? "claimed"
                                  : "unknown");
  if (link == LINK_RELEASED) {
    // Released by its owner: show E4, forget the server, set up again.
    static pxt_fb_t fb;
    gfx_stop();
    pxt_screen_status(&fb, PXT_STATUS_NOT_LINKED);
    show_fb(&fb);
    nvs_set_api_key("");
    nvs_set_image_url(NULL);
    nvs_save_settings();
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
  }
  s_link_check_running = false;
  vTaskDelete(NULL);
}

void pxt_ws_handshake_status(int http_status) {
  static int refused = 0;
  if (http_status == 401 || http_status == 403 || http_status == 404) {
    refused++;
    ESP_LOGW(TAG, "Server refused the device (%d), %d in a row", http_status,
             refused);
  } else {
    refused = 0;
    return;
  }
  // Check after 3 refusals, then every 6th (about once a minute).
  if (refused < 3 || (refused - 3) % 6 != 0 || s_link_check_running) return;
  s_link_check_running = true;
  if (xTaskCreate(link_check_task, "pxt_link", 6144, NULL, 4, NULL) != pdPASS) {
    s_link_check_running = false;
  }
}
