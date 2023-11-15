idf_component_register(SRCS "cJSON_Utils.c" "cJSON.c" "firebase.c"
                    INCLUDE_DIRS "include" "../../main"
                    PRIV_REQUIRES esp_http_client esp-tls driver environmental_sensor uv_sensor
                    EMBED_TXTFILES certificate.pem)