# Header-only: COLDX_HIDDEN. A game links coldx::protect to say it uses it.
add_library(coldx_protect INTERFACE)
add_library(coldx::protect ALIAS coldx_protect)
target_include_directories(coldx_protect INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_features(coldx_protect INTERFACE cxx_std_20)
