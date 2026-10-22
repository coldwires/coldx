# Packs a game's content into a .rime beside its executable, as part of its build:
#
#   coldx_add_content(my_game PROJECT content/content.coldx)
#
# PROJECT   the content project file; everything in its folder is watched for changes.
# OUTPUT    the package's file name, content.rime by default. ContentManager mounts
#           "<RootDirectory>.rime" by itself, and the root is "content" unless the game
#           changes it, so keep the two in step.
# TOOL      the tool target to run, coldxc by default; a game with its own processors
#           names its own tool (see coldx/content/build.h).
# KEY_FILE  encrypt the package with this key (make one with coldxc --new-key FILE, and
#           keep it out of public repositories). The game gets the key through a source
#           file generated into its build; nothing to call.
# BUILD_ID  mark the package and the game with a fingerprint of this id; coldxc --identify
#           reads it back. Defaults to the COLDX_BUILD_ID cache variable; empty for none.
#
# The tool only rewrites the package when it changes, and keeps a cache of processed
# sources in the build folder.
set(COLDX_BUILD_ID "" CACHE STRING "Fingerprint id for packages and games built with coldx_add_content")

function(coldx_add_content target)
    cmake_parse_arguments(ARG "" "PROJECT;OUTPUT;TOOL;KEY_FILE;BUILD_ID" "" ${ARGN})
    if(NOT ARG_PROJECT)
        message(FATAL_ERROR "coldx_add_content(${target}): PROJECT is required")
    endif()
    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT content.rime)
    endif()
    if(NOT ARG_TOOL)
        set(ARG_TOOL coldxc)
    endif()
    if(NOT DEFINED ARG_BUILD_ID)
        set(ARG_BUILD_ID "${COLDX_BUILD_ID}")
    endif()

    get_filename_component(project "${ARG_PROJECT}" ABSOLUTE)
    get_filename_component(project_dir "${project}" DIRECTORY)
    # New files in the content folder, and in any folder a section takes files "from",
    # rerun CMake, so they are packed too.
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${project_dir}/*")
    file(STRINGS "${project}" from_lines REGEX "^[ \t]*from[ \t]*=")
    foreach(line IN LISTS from_lines)
        string(REGEX REPLACE "^[ \t]*from[ \t]*=[ \t]*([^#]*[^ \t#])?.*$" "\\1" from_dir "${line}")
        file(GLOB_RECURSE from_sources CONFIGURE_DEPENDS "${project_dir}/${from_dir}/*")
        list(APPEND sources ${from_sources})
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${project}")

    set(work "${CMAKE_CURRENT_BINARY_DIR}/${target}_content")
    set(package "${work}/${ARG_OUTPUT}")
    # A stamp, not the package, is the command's output: the tool leaves an unchanged
    # package untouched, and the stamp keeps the command from running every build.
    set(stamp "${work}/${ARG_OUTPUT}.stamp")
    set(outputs "${stamp}")
    set(extra)
    if(ARG_KEY_FILE)
        get_filename_component(key_file "${ARG_KEY_FILE}" ABSOLUTE)
        list(APPEND extra --key-file "${key_file}")
        list(APPEND sources "${key_file}")
    endif()
    if(NOT ARG_BUILD_ID STREQUAL "")
        list(APPEND extra --build-id "${ARG_BUILD_ID}")
    endif()
    if(extra)
        # The key and fingerprint reach the game through a source file compiled into it.
        set(game_source "${work}/coldx_content_${target}.cpp")
        list(APPEND extra --game-source "${game_source}")
        list(APPEND outputs "${game_source}")
        target_sources(${target} PRIVATE "${game_source}")
        set_source_files_properties("${game_source}" PROPERTIES GENERATED TRUE)
    endif()
    add_custom_command(OUTPUT ${outputs}
        COMMAND ${ARG_TOOL} --project "${project}" --output "${package}" --cache "${work}/cache" ${extra}
        COMMAND ${CMAKE_COMMAND} -E touch ${outputs}
        DEPENDS ${ARG_TOOL} ${sources}
        COMMENT "Packing ${ARG_OUTPUT} for ${target}"
        VERBATIM)
    add_custom_target(${target}_content DEPENDS ${outputs})
    add_dependencies(${target} ${target}_content)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${package}" "$<TARGET_FILE_DIR:${target}>/${ARG_OUTPUT}"
        VERBATIM)
endfunction()
