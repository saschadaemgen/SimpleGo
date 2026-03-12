idf_component_register(
    SRCS "zstd.c"
    INCLUDE_DIRS "include"
)

# Suppress warnings in third-party code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-implicit-fallthrough
    -Wno-maybe-uninitialized
    -Wno-format
    -O2
)
