idf_component_register(SRCS "environmental_control.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fan lights pdlc environmental_sensor uv_sensor)
