#include "svc_mdns.h"
#include "esp_mac.h"
#include "esp_system.h"
#include <esp_log.h>
#include <mdns.h>
#include <string.h>
#include "sdkconfig.h"

static const char *TAG = "svc_mdns";

esp_err_t svc_mdns_init(void) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  char hostname[32];
  char instance_name[64];

  // Create unique names based on MAC (e.g., prefix-7f9c)
  // snprintf(hostname, sizeof(hostname), "%s-%02x%02x", CONFIG_APP_MDNS_HOSTNAME, mac[4], mac[5]);
  // snprintf(instance_name, sizeof(instance_name), "%s-%02x%02x Web Config",
  //          CONFIG_APP_MDNS_HOSTNAME, mac[4], mac[5]);

  snprintf(hostname, sizeof(hostname), "%s", CONFIG_APP_MDNS_HOSTNAME);
  snprintf(instance_name, sizeof(instance_name), "%s Web Config",
           CONFIG_APP_MDNS_HOSTNAME);

  // Initialize mDNS
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    if (err == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "mDNS already initialized, updating configuration");
    } else {
      ESP_LOGE(TAG, "MDNS Init failed: %d", err);
      return err;
    }
  }

  // Set unique hostname
  if ((err = mdns_hostname_set(hostname)) != ESP_OK) {
    ESP_LOGE(TAG, "mDNS set hostname failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "mDNS unique hostname set to: [%s.local]", hostname);

  // Set unique instance name
  if ((err = mdns_instance_name_set(instance_name)) != ESP_OK) {
    ESP_LOGE(TAG, "mDNS set instance failed: %s", esp_err_to_name(err));
    return err;
  }

  // Structure with TXT records
  mdns_txt_item_t serviceTxtData[2] = {{"board", "esp32"}, {"path", "/"}};

  // Remove existing service if causing conflict (Refresh)
  mdns_service_remove("_http", "_tcp");

  // Initialize service with UNIQUE name
  // Port 8080 for HTTP (Web Config)
  ESP_ERROR_CHECK(mdns_service_add(instance_name, "_http", "_tcp", 8080,
                                   serviceTxtData, 2));

  return ESP_OK;
}
