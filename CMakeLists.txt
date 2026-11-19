cmake_minimum_required(VERSION 3.21)
project(AlteredBeastWidescreen LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This preview requires 64-bit Windows and MSVC.")
endif()

set(AB_INPUTS "" CACHE PATH "Inputs prepared by scripts/build_preview.py")
if(NOT AB_INPUTS OR NOT EXISTS "${AB_INPUTS}/generated/rom/ab_rom.h"
   OR NOT EXISTS "${AB_INPUTS}/generated/ram/ab_ram.h"
   OR NOT EXISTS "${AB_INPUTS}/generated/wide/ab_right.h")
    message(FATAL_ERROR "Run scripts/build_preview.py with your UE REV02 ROM first. See README.md.")
endif()

set(genesis "${AB_INPUTS}/genesisrecomp")
set(cpu "${AB_INPUTS}/genesisrecomp/external/m68k-recomp-core")
set(video "${genesis}/runner/video")
set(audio "${genesis}/runner/audio")
set(z80 "${genesis}/runner/external/superzazu")
set(ymfm "${genesis}/runner/external/ymfm/src")
set(generated "${AB_INPUTS}/generated")
set(overlays "${AB_INPUTS}/overlays")

# SDL handles controller input. The Windows host handles window drawing and audio.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
foreach(option SDL_STATIC SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL SDL_UNINSTALL
               SDL_AUDIO SDL_VIDEO SDL_GPU SDL_RENDER SDL_CAMERA SDL_HAPTIC SDL_POWER
               SDL_SENSOR SDL_DIALOG SDL_TRAY)
    set(${option} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${AB_INPUTS}/SDL3" sdl EXCLUDE_FROM_ALL)

add_library(ab_runtime STATIC
    runtime/ab_machine_bus.c runtime/ab_startup_io.c
    runtime/ab_z80_runner.c runtime/ab_z80_irq_core.c
    runtime/ab_psg.c runtime/ab_fm.cpp runtime/ab_audio.c
    "${audio}/sn76489.c"
    "${ymfm}/ymfm_opn.cpp" "${ymfm}/ymfm_adpcm.cpp" "${ymfm}/ymfm_ssg.cpp"
    "${overlays}/interrupt/m68k_step.c" "${overlays}/vdp/genesis_vdp.c"
)
target_compile_features(ab_runtime PUBLIC c_std_11 PRIVATE cxx_std_17)
target_include_directories(ab_runtime PUBLIC runtime "${cpu}/runtime" "${video}" "${z80}"
    PRIVATE "${audio}" "${ymfm}" "${genesis}/runner/include" "${genesis}/runner")
target_compile_definitions(ab_runtime PRIVATE GENESIS_HD_JOBS=0)

file(GLOB rom_code CONFIGURE_DEPENDS "${generated}/rom/ab_rom_*.c")
file(GLOB ram_code CONFIGURE_DEPENDS "${generated}/ram/ab_ram_*.c")
file(GLOB wide_code CONFIGURE_DEPENDS "${generated}/wide/ab_right_*.c")
add_library(widescreen_core STATIC src/game.c src/preview-raster.c src/beast-action.c
    ${rom_code} ${ram_code} ${wide_code})
target_include_directories(widescreen_core PUBLIC src
    PRIVATE "${generated}/rom" "${generated}/ram" "${generated}/wide")
target_link_libraries(widescreen_core PUBLIC ab_runtime)

add_executable(widescreen_preview src/preview-host.c src/beast-menu.c src/ab_debug_report.c
    runtime/ab_controller.c runtime/ab_sdl_controller.c runtime/ab_sdl_virtual.c)
target_compile_options(widescreen_preview PRIVATE /utf-8)
target_compile_definitions(widescreen_preview PRIVATE AB_DEBUG_REPORTS=1 _CRT_SECURE_NO_WARNINGS)
target_link_libraries(widescreen_preview PRIVATE widescreen_core SDL3::SDL3 winmm bcrypt)
add_custom_command(TARGET widescreen_preview POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:widescreen_preview>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${AB_INPUTS}/SDL3/LICENSE.txt"
        "$<TARGET_FILE_DIR:widescreen_preview>/SDL3-LICENSE.txt"
    VERBATIM)

include(CTest)
if(BUILD_TESTING)
    add_executable(plane_edge_test tests/plane_edge_policy.c)
    target_link_libraries(plane_edge_test PRIVATE ab_runtime)
    target_include_directories(plane_edge_test PRIVATE src)
    add_test(NAME plane_edge_policy COMMAND plane_edge_test)

    set(AB_TEST_ROM "" CACHE FILEPATH "Local UE REV02 ROM for gameplay tests")
    if(AB_TEST_ROM)
        add_executable(beast_test tests/beast-tests.c)
        target_link_libraries(beast_test PRIVATE widescreen_core)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/chooser-capture")
        add_test(NAME beast_chooser COMMAND beast_test "${AB_TEST_ROM}" "${CMAKE_CURRENT_BINARY_DIR}/chooser-capture")
        add_test(NAME controller_menu COMMAND widescreen_preview "${AB_TEST_ROM}" --menu-smoke)
        set_tests_properties(beast_chooser controller_menu PROPERTIES TIMEOUT 90)
    endif()
endif()
