idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS 
        "."
        "../src"
        "../include"
    REQUIRES
        unity
        cmock
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
