idf_component_register(INCLUDE_DIRS include)
target_compile_definitions(${COMPONENT_LIB} INTERFACE WOLFSSL_USER_SETTINGS)

# Make user_settings.h globally visible for wolfssl component
# CMAKE_C_FLAGS applies at the earliest stage of CMake configuration
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${COMPONENT_DIR}/include" CACHE STRING "" FORCE)
