#include "ds18b20_temperature_sensor.h"

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
// DS18B20 commands
constexpr uint8_t kCmdSkipRom = 0xCC;
constexpr uint8_t kCmdConvertT = 0x44;
constexpr uint8_t kCmdReadScratchpad = 0xBE;

portMUX_TYPE s_onewire_mux = portMUX_INITIALIZER_UNLOCKED;

// CRC8 Maxim/Dallas (polynome x^8+x^5+x^4+1, 0x8C sous forme reflechie) -
// utilise pour valider le scratchpad du DS18B20 : le bus 1-Wire bit-bange
// n'a pas de garantie de timing aussi stricte qu'un vrai maitre 1-Wire
// materiel, un octet corrompu par du bruit doit etre detecte plutot que
// silencieusement republie comme une "bonne" mesure.
uint8_t crc8_maxim(const uint8_t* data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (int b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return crc;
}
}  // namespace

Ds18b20TemperatureSensor::Ds18b20TemperatureSensor(gpio_num_t pin) : pin_(pin) {}

esp_err_t Ds18b20TemperatureSensor::init()
{
    gpio_reset_pin(pin_);
    gpio_set_direction(pin_, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(pin_, 1);
    // Pull-up interne en secours : une resistance 4.7k externe entre le
    // signal et 3.3V reste recommandee pour un bus 1-Wire fiable.
    gpio_set_pull_mode(pin_, GPIO_PULLUP_ONLY);

    return onewire_reset() ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t Ds18b20TemperatureSensor::read(float* out_value)
{
    if (!onewire_reset()) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(kCmdSkipRom);
    onewire_write_byte(kCmdConvertT);

    // Temps de conversion max en 12 bits (resolution par defaut du DS18B20)
    vTaskDelay(pdMS_TO_TICKS(750));

    if (!onewire_reset()) {
        return ESP_ERR_NOT_FOUND;
    }
    onewire_write_byte(kCmdSkipRom);
    onewire_write_byte(kCmdReadScratchpad);

    // Scratchpad complet (9 octets, le dernier est un CRC8 des 8 premiers) -
    // lire uniquement les 2 premiers octets ne permet aucune verification
    // d'integrite face au bruit sur un bus 1-Wire bit-bange.
    uint8_t scratchpad[9];
    for (uint8_t& byte : scratchpad) {
        byte = onewire_read_byte();
    }

    if (crc8_maxim(scratchpad, 8) != scratchpad[8]) {
        return ESP_ERR_INVALID_CRC;
    }

    int16_t raw = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);
    *out_value = raw / 16.0f;
    return ESP_OK;
}

bool Ds18b20TemperatureSensor::onewire_reset()
{
    gpio_set_level(pin_, 0);
    esp_rom_delay_us(480);
    gpio_set_level(pin_, 1);
    esp_rom_delay_us(70);
    int presence = gpio_get_level(pin_);  // 0 = capteur present (presence pulse)
    esp_rom_delay_us(410);
    return presence == 0;
}

void Ds18b20TemperatureSensor::onewire_write_bit(int bit)
{
    portENTER_CRITICAL(&s_onewire_mux);
    gpio_set_level(pin_, 0);
    esp_rom_delay_us(bit ? 6 : 60);
    gpio_set_level(pin_, 1);
    portEXIT_CRITICAL(&s_onewire_mux);
    esp_rom_delay_us(bit ? 64 : 10);
}

int Ds18b20TemperatureSensor::onewire_read_bit()
{
    int bit;
    portENTER_CRITICAL(&s_onewire_mux);
    gpio_set_level(pin_, 0);
    esp_rom_delay_us(2);
    gpio_set_level(pin_, 1);
    esp_rom_delay_us(8);
    bit = gpio_get_level(pin_);
    portEXIT_CRITICAL(&s_onewire_mux);
    esp_rom_delay_us(50);
    return bit;
}

void Ds18b20TemperatureSensor::onewire_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        onewire_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

uint8_t Ds18b20TemperatureSensor::onewire_read_byte()
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte >>= 1;
        if (onewire_read_bit()) {
            byte |= 0x80;
        }
    }
    return byte;
}
