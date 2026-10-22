# Release builds that give away less about how they were made, and are harder to match
# function by function. MSVC only for now; elsewhere the function does nothing.
#
#   cmake -DCOLDX_HARDEN=ON ...      coldx's own libraries and samples
#   coldx_harden(my_game)            a game's executable
#
# In the Release configuration:
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
# A game built this way needs coldx built with COLDX_HARDEN=ON too: a library built with
# RTTI carries its class names into the game.
option(COLDX_HARDEN "Release builds of coldx and its samples without RTTI and build-machine details" OFF)

function(coldx_harden target)
    if(NOT MSVC)
        return()
    endif()
    set(release "$<CONFIG:Release>")
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
