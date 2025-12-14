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

find_package(SDL2 REQUIRED)

set(SRC_FILES
    anim/sysanim.c
    base/base.c
    cdrom/cdrom.c
    data/dataappl.c
    data/database.c
    data/datacalc.c
    data/relation.c
    dialog/dialog.c
    dialog/talkappl.c
    disk/disk.c
    error/error.c
    gameplay/gp_app.c
    gameplay/gp.c
    gameplay/loadsave.c
    gameplay/tcreques.c
    gfx/gfx.c
    gfx/gfxnch4.c
    inphdl/inphdl.c
    landscap/access.c
    landscap/landscap.c
    landscap/hardware.c
    landscap/init.c
    landscap/raster.c
    landscap/scroll.c
    landscap/spot.c
    list/list.c
    living/bob.c
    living/living.c
    memory/memory.c
    organisa/organisa.c
    planing/graphics.c
    planing/guards.c
    planing/io.c
    planing/main.c
    planing/planer.c
    planing/player.c
    planing/prepare.c
    planing/support.c
    planing/system.c
    planing/sync.c
    present/present.c
    present/interac.c
    random/random.c
    scenes/cars.c
    scenes/dealer.c
    scenes/done.c
    scenes/evidence.c
    scenes/inside.c
    scenes/invest.c
    scenes/scenes.c
    scenes/tools.c
    sound/buffer.c
    sound/fmopl.c
    sound/fx.c
    sound/newsound.c
    sound/hsc.c
    story/story.c
    text/text.c
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

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

# Link SDL2 and libm just like the original build.
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
)
