cmake_minimum_required(VERSION 3.16)
project(derclou C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Remove stale linker paths that point to Homebrew-only locations that may
# not exist on the current machine.
foreach(flag_var IN ITEMS
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS)
    if(${flag_var} MATCHES "/usr/local/opt/openssl/lib")
        string(REPLACE "-L/usr/local/opt/openssl/lib" "" cleaned_flag "${${flag_var}}")
        string(REGEX REPLACE "  +" " " cleaned_flag "${cleaned_flag}")
        set(${flag_var} "${cleaned_flag}" CACHE STRING "" FORCE)
    endif()
endforeach()

find_package(SDL3 REQUIRED CONFIG)

set(SRC_FILES
    src/anim/sysanim.c
    src/base/base.c
    src/cdrom/cdrom.c
    src/data/dataappl.c
    src/data/database.c
    src/data/datacalc.c
    src/data/relation.c
    src/dialog/dialog.c
    src/dialog/talkappl.c
    src/disk/disk.c
    src/error/error.c
    src/gameplay/gp_app.c
    src/gameplay/gp.c
    src/gameplay/loadsave.c
    src/gameplay/tcreques.c
    src/gfx/gfx.c
    src/gfx/gfxnch4.c
    src/inphdl/inphdl.c
    src/landscap/access.c
    src/landscap/landscap.c
    src/landscap/hardware.c
    src/landscap/init.c
    src/landscap/raster.c
    src/landscap/scroll.c
    src/landscap/spot.c
    src/list/list.c
    src/living/bob.c
    src/living/living.c
    src/memory/memory.c
    src/organisa/organisa.c
    src/planing/graphics.c
    src/planing/guards.c
    src/planing/io.c
    src/planing/main.c
    src/planing/planer.c
    src/planing/player.c
    src/planing/prepare.c
    src/planing/support.c
    src/planing/system.c
    src/planing/sync.c
    src/present/present.c
    src/present/interac.c
    src/random/random.c
    src/scenes/cars.c
    src/scenes/dealer.c
    src/scenes/done.c
    src/scenes/evidence.c
    src/scenes/inside.c
    src/scenes/invest.c
    src/scenes/scenes.c
    src/scenes/tools.c
    src/sound/buffer.c
    src/sound/fmopl.c
    src/sound/fx.c
    src/sound/newsound.c
    src/sound/hsc.c
    src/story/story.c
    src/text/text.c
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Use the user-facing name "clou" (no "der") for the produced binary/app.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "clou")

# Match the Makefile's compile flags.
target_compile_options(${PROJECT_NAME} PRIVATE
    -O2
    -funroll-loops
    -pedantic
    -Wall
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wsign-compare
)

# Keep include search identical to the Makefile.
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL3 just like the original build.
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
)

# ---------------------------
# Cross-platform app icon
# ---------------------------

# Windows: embed icon via resource file (.rc references clou.ico)
if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/assets/icons/windows/clou.rc
    )
endif()

# macOS: make an .app bundle and include clou.icns
if(APPLE AND WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "clou.icns"
    )

    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/assets/icons/macos/clou.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )

    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/assets/icons/macos/clou.icns
    )
endif()

# Linux / FreeDesktop: install icon + desktop entry (icon is not embedded into ELF)
if(UNIX AND NOT APPLE)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

    install(FILES ${CMAKE_SOURCE_DIR}/assets/linux/clou.desktop
            DESTINATION share/applications)

    install(FILES ${CMAKE_SOURCE_DIR}/assets/icons/linux/hicolor/256x256/apps/clou.png
            DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES ${CMAKE_SOURCE_DIR}/assets/icons/linux/hicolor/512x512/apps/clou.png
            DESTINATION share/icons/hicolor/512x512/apps)
endif()
