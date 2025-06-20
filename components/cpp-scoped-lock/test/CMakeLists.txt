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

# if IDF version 4.x, use C++17 standard
if(${IDF_VERSION_MAJOR} EQUAL 4)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
endif()

