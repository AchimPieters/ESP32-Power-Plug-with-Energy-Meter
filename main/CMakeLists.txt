idf_component_register(
    SRCS "main.c" "esp32-lcm.c" "bl0937.c" "custom_characteristics.c"
    # Managed components are named namespace__component when installed via the IDF Component Manager.
    REQUIRES freertos esp_wifi esp_event esp_netif nvs_flash driver esp_timer app_update spi_flash esp_system mdns
             achimpieters__esp32-homekit achimpieters__esp32-button
)
