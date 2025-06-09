cmake_minimum_required(VERSION 3.16)

set(srcs 
    "src/configDb.cpp"
)

# The values of REQUIRES and PRIV_REQUIRES should not depend on any configuration choices (CONFIG_xxx macros). This is because requirements are expanded before configuration is loaded. Other component variables (like include paths or source files) can depend on configuration choices.
set(reqs
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${reqs}
)

# set(CMAKE_CXX_STANDARD 17) # this one worked for me, but not on another machine.
# target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17") # this one sets the option for C files also, which raises a warning.
# idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17" APPEND) # this one sets the option project-wide, which is not intended.
# set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17) # this one seems to work
# target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17) # this one works too, and seems to be the most modern option, but didn't work on another machine.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
