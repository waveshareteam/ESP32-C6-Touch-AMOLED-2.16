idf_component_register(
    SRCS 
    "i2c_bsp.cpp" 
    PRIV_REQUIRES 
    driver
    pmicpower
    INCLUDE_DIRS 
    "./")
