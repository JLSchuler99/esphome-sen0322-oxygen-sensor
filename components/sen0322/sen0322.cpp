#include "sen0322.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sen0322 {

static const char *const TAG = "sen0322";

// Register commands according to datasheet
static const uint8_t SEN0322_COLLECT_PHASE = 0x01;
static const uint8_t SEN0322_JUDGE_PHASE = 0x02;
static const uint8_t SEN0322_OXYGEN_DATA = 0x03;

void SEN0322Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN0322...");
  
  // Initialize sensor
  if (!this->write_byte(SEN0322_COLLECT_PHASE, 0x00)) {
    ESP_LOGE(TAG, "Failed to initialize sensor");
    this->mark_failed();
    return;
  }
  
  esphome::delay(100);
  ESP_LOGCONFIG(TAG, "SEN0322 setup complete");
}

void SEN0322Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN0322 Oxygen Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Oxygen", this);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SEN0322 failed!");
  }
}

void SEN0322Sensor::update() {
  ESP_LOGV(TAG, "Updating SEN0322 sensor...");
  
  // Send command to read oxygen concentration
  if (!this->write_byte(SEN0322_OXYGEN_DATA, 0x00)) {
    ESP_LOGE(TAG, "Failed to send oxygen data command");
    this->status_set_warning();
    return;
  }
  
  // Wait for sensor to process
  esphome::delay(50);
  
  // Read 3 bytes of data
  uint8_t data[3];
  if (!this->read_bytes_raw(data, 3)) {
    ESP_LOGE(TAG, "Failed to read oxygen data");
    this->status_set_warning();
    return;
  }
  
  // Parse oxygen concentration with little-endian byte order
  // SEN0322 uses reversed byte order: data[1] = high byte, data[0] = low byte
  uint16_t raw_oxygen = (data[1] << 8) | data[0];
  float oxygen_concentration = raw_oxygen * 0.01f;
  
  // Validate reading (oxygen should be between 0-30% for safety margin)
  if (oxygen_concentration < 0.0f || oxygen_concentration > 30.0f) {
    ESP_LOGW(TAG, "Invalid oxygen reading: %.2f%% (raw: 0x%04X)", oxygen_concentration, raw_oxygen);
    this->status_set_warning();
    return;
  }
  
  ESP_LOGD(TAG, "Got oxygen concentration: %.2f%%", oxygen_concentration);
  this->publish_state(oxygen_concentration);
  this->status_clear_warning();
}

float SEN0322Sensor::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace sen0322
}  // namespace esphome
