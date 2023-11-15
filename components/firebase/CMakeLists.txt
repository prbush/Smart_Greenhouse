idf_component_register(SRCS "cJSON_Utils.c" "cJSON.c" "firebase.c"
                    INCLUDE_DIRS "include"
                    REQUIRES environmental_control
                    PRIV_REQUIRES esp_http_client esp-tls driver
                    EMBED_TXTFILES certificate.pem)