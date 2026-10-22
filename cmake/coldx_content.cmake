# Packs a game's content into a .rime beside its executable, as part of its build:
#
#   coldx_add_content(my_game PROJECT content/content.coldx)
#
# PROJECT  the content project file; everything in its folder is watched for changes.
# OUTPUT   the package's file name, content.rime by default. ContentManager mounts
#          "<RootDirectory>.rime" by itself, and the root is "content" unless the game
#          changes it, so keep the two in step.
# TOOL     the tool target to run, coldxc by default; a game with its own processors
#          names its own tool (see coldx/content/build.h).
#
# The tool only rewrites the package when it changes, and keeps a cache of processed
# sources in the build folder.
function(coldx_add_content target)
    cmake_parse_arguments(ARG "" "PROJECT;OUTPUT;TOOL" "" ${ARGN})
    if(NOT ARG_PROJECT)
        message(FATAL_ERROR "coldx_add_content(${target}): PROJECT is required")
    endif()
    if(NOT ARG_OUTPUT)
        set(ARG_OUTPUT content.rime)
    endif()
    if(NOT ARG_TOOL)
        set(ARG_TOOL coldxc)
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
    add_custom_command(OUTPUT "${stamp}"
        COMMAND ${ARG_TOOL} --project "${project}" --output "${package}" --cache "${work}/cache"
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS ${ARG_TOOL} ${sources}
        COMMENT "Packing ${ARG_OUTPUT} for ${target}"
        VERBATIM)
    add_custom_target(${target}_content DEPENDS "${stamp}")
    add_dependencies(${target} ${target}_content)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${package}" "$<TARGET_FILE_DIR:${target}>/${ARG_OUTPUT}"
        VERBATIM)
endfunction()
