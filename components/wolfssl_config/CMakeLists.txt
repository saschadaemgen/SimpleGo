idf_component_register(INCLUDE_DIRS include)
target_compile_definitions(${COMPONENT_LIB} INTERFACE WOLFSSL_USER_SETTINGS)

# Make user_settings.h globally visible so wolfssl component finds it
# Required for both local and managed_components builds
idf_build_set_property(C_COMPILE_OPTIONS "-I${COMPONENT_DIR}/include" APPEND)
