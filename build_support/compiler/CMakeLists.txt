cmake_minimum_required(VERSION 3.21)
project(AlteredBeastCodeGenerators LANGUAGES C)

set(genesis "${AB_INPUTS}/genesisrecomp")
set(core "${AB_INPUTS}/genesisrecomp/external/m68k-recomp-core")
set(overlays "${AB_INPUTS}/overlays")
set(frontend "${genesis}/recompiler/src")
include("${core}/cmake/M68kRecompCore.cmake")
m68k_recomp_core_sources(core_sources genesis)
m68k_recomp_core_include_dirs(core_includes genesis)

add_library(generator_support STATIC ${core_sources}
    "${frontend}/rom_parser.c" "${frontend}/game_config.c" "${frontend}/toml.c")
target_include_directories(generator_support PUBLIC "${frontend}" ${core_includes})
target_compile_features(generator_support PUBLIC c_std_11)

add_executable(generate_rom "${frontend}/main_step.c")
add_executable(generate_ram "${overlays}/frontend-ram/main_step.c")
add_executable(generate_wide "${overlays}/frontend-wide/main_step.c")
foreach(target generate_rom generate_ram generate_wide)
    target_link_libraries(${target} PRIVATE generator_support)
endforeach()
