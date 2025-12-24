# TallyPage Component

idf_component_register(
    SRCS "TallyPage.cpp"
    INCLUDE_DIRS "include"
    REQUIRES DisplayManager tally_types t_log
)
