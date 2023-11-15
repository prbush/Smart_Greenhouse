idf_component_register(SRCS "environmental_control.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_adc fan lights pdlc environmental_sensor uv_sensor firebase)
