add_executable(demo main.cpp sheet.h)
target_link_libraries(demo PRIVATE coldx::core)
coldx_game(demo)
coldx_set_warnings(demo)

# The sheet and sounds are made for coldx; the music is hello_audio's; the font ships
# with its license. content/content.coldx lists them, and coldxc packs them into
# content.rime beside demo.exe, which ContentManager mounts by itself.
coldx_add_content(demo PROJECT content/content.coldx)
