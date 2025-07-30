#include "sen0322.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sen0322 {

static const char *const TAG = "sen0322";

// Official register addresses
static const uint8_t REG_OXYGEN_DATA = 0x03;
static const uint8_t REG_KEY = 0x05;

void SEN0322Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN0322...");
  // No need to send collect/judge phase based on DFRobot reference
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

  // Step 1: Read calibration key from flash (register 0x05)
  if (!this->write_byte(REG_KEY, 0x00)) {
    ESP_LOGE(TAG, "Failed to request calibration key");
    this->status_set_warning();
    return;
  }

  esphome::delay(50);

  uint8_t key_byte;
  if (!this->read_byte(&key_byte)) {
    ESP_LOGE(TAG, "Failed to read calibration key");
    this->status_set_warning();
    return;
  }

  float key = (key_byte == 0) ? (20.9f / 120.0f) : (key_byte / 1000.0f);
  ESP_LOGD(TAG, "Calibration key: 0x%02X → %.5f", key_byte, key);

  // Step 2: Read and process multiple oxygen samples
  const int samples = 3;
  float total_oxygen = 0.0f;
  int valid_samples = 0;

  for (int i = 0; i < samples; i++) {
    // Request data
    if (!this->write_byte(REG_OXYGEN_DATA, 0x00)) {
      ESP_LOGE(TAG, "Failed to send oxygen data request");
      continue;
    }

    esphome::delay(100);  // Wait for sensor to prepare data

    uint8_t data[3];
    if (!this->read_bytes_raw(data, 3)) {
      ESP_LOGE(TAG, "Failed to read oxygen data");
      continue;
    }

    float oxygen = key * (data[0] + data[1] / 10.0f + data[2] / 100.0f);
    ESP_LOGV(TAG, "Raw bytes: [%d, %d, %d] → %.2f%%", data[0], data[1], data[2], oxygen);

    if (oxygen >= 0.0f && oxygen <= 30.0f) {
      total_oxygen += oxygen;
      valid_samples++;
    } else {
      ESP_LOGW(TAG, "Discarded invalid reading: %.2f%%", oxygen);
    }

    esphome::delay(100);  // Optional short delay between samples
  }

  if (valid_samples > 0) {
    float avg_oxygen = total_oxygen / valid_samples;
    ESP_LOGD(TAG, "Avg oxygen concentration: %.2f%%", avg_oxygen);
    this->publish_state(avg_oxygen);
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, "No valid samples collected");
    this->status_set_warning();
  }
}

float SEN0322Sensor::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace sen0322
}  // namespace esphome
