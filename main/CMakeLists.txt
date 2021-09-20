set(COMPONENT_SRCS "main.c" "ui.c" "wifi.c" "images/heart_icon.c" "images/report_icon.c" "images/logs_icon.c" "images/fun_icon.c" "images/qr_code.c")
set(COMPONENT_ADD_INCLUDEDIRS "." "./includes")
set(COMPONENT_REQUIRES
    "nvs_flash" 
    "esp-aws-iot" 
    "esp-cryptoauthlib" 
    "core2forAWS"
    "esp_http_client" 
    "json"
)

register_component()
target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
