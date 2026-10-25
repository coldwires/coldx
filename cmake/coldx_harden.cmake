# Release builds that give away less about how they were made, and are harder to match
# function by function. MSVC, and GCC or Clang.
#
#   cmake -DCOLDX_HARDEN=ON ...      coldx's own libraries and samples
#   coldx_harden(my_game)            a game's executable
#
# In the Release configuration, with MSVC:
#   /GR-                      no RTTI: class names are not stored. dynamic_cast and typeid
#                             stop working on the game's own types.
#   /d1trimfile               source paths compiled in (__FILE__) lose the source folder
#   /GL, /LTCG                whole-program optimization: code from many files mixes
#   /OPT:REF /OPT:ICF         unused code removed, identical functions folded
#   /EMITTOOLVERSIONINFO:NO   no record of the tools that built it
#   /Brepro                   a hash in place of the build time
#   /DEBUG /PDBALTPATH:%_PDB% symbols in a .pdb kept beside the build (archive it, do not
#                             ship it); the executable names it without a path
#
# With GCC or Clang:
#   -fno-rtti                 the same as /GR-
#   -fvisibility=hidden       no exported symbol tables for the code's own functions
#   -ffile-prefix-map         source paths compiled in lose the source folder
#   -ffunction-sections, -fdata-sections, --gc-sections   unused code removed
#   link-time optimization    CMake's INTERPROCEDURAL_OPTIMIZATION, so static libraries
#                             are archived with the compiler's own ar
#   symbols split off         an executable's symbols go to <name>.debug beside it (archive
#                             it, do not ship it), and the executable is stripped
#
# A game built this way needs coldx built with COLDX_HARDEN=ON too: a library built with
# RTTI carries its class names into the game. With GCC or Clang the reverse holds as well:
# everything that derives from a coldx class must be built without RTTI, so COLDX_HARDEN
# turns it off for all C++ in the build, and a game hardening itself does the same.
option(COLDX_HARDEN "Release builds of coldx and its samples without RTTI and build-machine details" OFF)

function(coldx_harden target)
    # Once a target: a second pass would strip a stripped file over its own symbols.
    get_target_property(done ${target} COLDX_HARDENED)
    if(done)
        return()
    endif()
    set_property(TARGET ${target} PROPERTY COLDX_HARDENED TRUE)
    set(release "$<CONFIG:Release>")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            "$<${release}:-fno-rtti;-fvisibility=hidden;-fvisibility-inlines-hidden;-ffunction-sections;-fdata-sections;-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=>")
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY")
            target_link_options(${target} PRIVATE "$<${release}:LINKER:--gc-sections>")
            if(CMAKE_OBJCOPY)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND "$<${release}:${CMAKE_OBJCOPY};--only-keep-debug;$<TARGET_FILE:${target}>;$<TARGET_FILE:${target}>.debug>"
                    COMMAND "$<${release}:${CMAKE_OBJCOPY};--strip-all;--add-gnu-debuglink=$<TARGET_FILE:${target}>.debug;$<TARGET_FILE:${target}>>"
                    COMMAND_EXPAND_LISTS
                    VERBATIM)
            endif()
        endif()
        return()
    endif()
    if(NOT MSVC)
        return()
    endif()
    # The game's own folders out of any __FILE__ it compiles in.
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/" source_prefix)
    target_compile_options(${target} PRIVATE "$<${release}:/GR-;/GL;/Zi;/d1trimfile:${source_prefix}>")
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "STATIC_LIBRARY")
        set_property(TARGET ${target} APPEND PROPERTY STATIC_LIBRARY_OPTIONS "$<${release}:/LTCG>")
    elseif(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY")
        target_link_options(${target} PRIVATE
            "$<${release}:/LTCG;/OPT:REF;/OPT:ICF;/EMITTOOLVERSIONINFO:NO;/Brepro;/DEBUG;/PDBALTPATH:%_PDB%>")
    endif()
endfunction()
