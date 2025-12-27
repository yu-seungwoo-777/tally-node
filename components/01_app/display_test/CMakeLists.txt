# Display Test App Component

idf_component_register(
    SRCS "display_test_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES DisplayManager TxPage RxPage button_service t_log
)
