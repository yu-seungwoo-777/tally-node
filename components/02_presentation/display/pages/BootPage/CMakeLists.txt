# BootPage Component

idf_component_register(
    SRCS "BootPage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES DisplayManager t_log app_types
)
