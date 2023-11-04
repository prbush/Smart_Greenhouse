idf_component_register(SRCS "firebase.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_http_client esp-tls
                    EMBED_TXTFILES certificate.pem)