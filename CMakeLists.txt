cmake_minimum_required(VERSION 3.20)
project(AlteredBeastWidescreen LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This preview requires 64-bit Windows and MSVC.")
endif()

get_filename_component(default_sourceport "${CMAKE_CURRENT_SOURCE_DIR}/../AlteredBeast-SourcePort" ABSOLUTE)
set(AB_SOURCEPORT_ROOT "${default_sourceport}" CACHE PATH "Local qualified AlteredBeast-SourcePort workspace")
set(runtime_run "${AB_SOURCEPORT_ROOT}/build/runtime-runs/e21a6f4dad5d4689aadba631bd098267")
set(preview_run "${AB_SOURCEPORT_ROOT}/build/task23/run-debcd3f38f0a42e89b93d79d2da3683f")
set(generated_dir "${preview_run}/generated")
set(runtime_lib_dir "${runtime_run}/runtime-tests/Release")

function(require_sha256 path expected)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Missing qualified local build input: ${path}. See README.md.")
    endif()
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Qualified input has changed: ${path}")
    endif()
endfunction()

require_sha256("${preview_run}/manifest.json" 3e2d7af4a23bc87dc8809636ecce8b100270ec31c6d1ab84e9bae19873f50635)
file(READ "${preview_run}/manifest.json" preview_manifest)
string(JSON generated_count LENGTH "${preview_manifest}" generated_sha256)
math(EXPR generated_last "${generated_count} - 1")
set(variant_sources)
foreach(index RANGE ${generated_last})
    string(JSON name MEMBER "${preview_manifest}" generated_sha256 ${index})
    string(JSON digest GET "${preview_manifest}" generated_sha256 "${name}")
    string(TOLOWER "${digest}" digest)
    require_sha256("${generated_dir}/${name}" "${digest}")
    if(name MATCHES "^ab_right_.*[.]c$")
        list(APPEND variant_sources "${generated_dir}/${name}")
    endif()
endforeach()

set(runtime_include_dirs
    "${AB_SOURCEPORT_ROOT}/native/runtime"
    "${AB_SOURCEPORT_ROOT}/build/task03e/source/runner/video"
    "${AB_SOURCEPORT_ROOT}/build/task03/toolchain/source/external/m68k-recomp-core/runtime"
    "${AB_SOURCEPORT_ROOT}/build/task03g/source/runner/external/superzazu"
    "${AB_SOURCEPORT_ROOT}/build/task03b/runs/be46926310704c52bef54220a5108376/generated-rom"
    "${AB_SOURCEPORT_ROOT}/build/task03c/runs/a564ea05c6834d4391ec0cba0ed855ac/generated-ram"
    "${runtime_run}/analysis"
    "${generated_dir}"
)
foreach(dir IN LISTS runtime_include_dirs)
    if(NOT IS_DIRECTORY "${dir}")
        message(FATAL_ERROR "Missing qualified local include directory: ${dir}")
    endif()
endforeach()

set(hash_ab_game ce829283565ee4eb03bbbbad6daa267725e0daac213b18bd96f885affee5adf1)
set(hash_ab_audio d09ecc76cdf015b461424d0ed2d5ddd10aca916d2a6abefa1c1e63afffdceeeb)
set(hash_ab_fm c1c8ffbdfc09f40a7b7ac19b9fc64d683ff4414f1f9fe72ef7e9a8584152bb46)
set(hash_ab_psg 31ede53c47ceb38c3e0dd3bf9d51f5f36ed1393a1e36b8a3d6c40872532151ee)
set(hash_ab_z80 c0929a74fa8b64bacedd51a4dd9921fd0f9c4b69e0922baf971867b242ba45f4)
set(hash_ab_vdp_bus de9c795d0c78aee07aca883ae8a450a2db89f5dc2a0efe37a7a5f34389fef845)
set(hash_ab_sdl_controller 8f3874a29db69ede536feb8f745c5b1fc4417230a184d4ddcfcc115efdf700a0)
set(hash_ab_controller 7b45794bfb877564efc7f7265816bd276a4e9c8e77bf1115f9f8169be143ecc6)
set(core_libraries ab_game ab_audio ab_fm ab_psg ab_z80 ab_vdp_bus)
foreach(lib IN LISTS core_libraries ITEMS ab_sdl_controller ab_controller)
    set(lib_path "${runtime_lib_dir}/${lib}.lib")
    require_sha256("${lib_path}" "${hash_${lib}}")
    add_library(${lib} STATIC IMPORTED)
    set_target_properties(${lib} PROPERTIES IMPORTED_LOCATION "${lib_path}")
endforeach()

find_package(SDL3 3.4.20 EXACT CONFIG REQUIRED
    PATHS "${AB_SOURCEPORT_ROOT}/build/dependencies/SDL3/build" NO_DEFAULT_PATH)

add_library(widescreen_core STATIC src/game.c src/preview-raster.c src/beast-action.c ${variant_sources})
target_compile_features(widescreen_core PUBLIC c_std_11)
target_include_directories(widescreen_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" ${runtime_include_dirs})
target_link_libraries(widescreen_core PUBLIC ${core_libraries})

add_executable(widescreen_preview src/preview-host.c src/beast-menu.c src/ab_debug_report.c)
set_target_properties(widescreen_preview PROPERTIES LINKER_LANGUAGE CXX)
target_compile_options(widescreen_preview PRIVATE /utf-8)
target_compile_definitions(widescreen_preview PRIVATE AB_DEBUG_REPORTS=1 _CRT_SECURE_NO_WARNINGS)
target_link_libraries(widescreen_preview PRIVATE widescreen_core ab_sdl_controller ab_controller SDL3::SDL3 winmm bcrypt)
add_custom_command(TARGET widescreen_preview POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:widescreen_preview>"
    VERBATIM)

include(CTest)
if(BUILD_TESTING)
    add_executable(plane_edge_test tests/plane_edge_policy.c)
    target_compile_features(plane_edge_test PRIVATE c_std_11)
    target_include_directories(plane_edge_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" ${runtime_include_dirs})
    add_test(NAME plane_edge_policy COMMAND plane_edge_test)

    set(AB_TEST_ROM "" CACHE FILEPATH "Optional local UE REV02 ROM for gameplay tests")
    if(AB_TEST_ROM)
        add_executable(beast_test tests/beast-tests.c)
        set_target_properties(beast_test PROPERTIES LINKER_LANGUAGE CXX)
        target_link_libraries(beast_test PRIVATE widescreen_core)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/chooser-capture")
        add_test(NAME beast_chooser COMMAND beast_test "${AB_TEST_ROM}" "${CMAKE_CURRENT_BINARY_DIR}/chooser-capture")
        add_test(NAME controller_menu COMMAND widescreen_preview "${AB_TEST_ROM}" --menu-smoke)
        set_tests_properties(beast_chooser controller_menu PROPERTIES TIMEOUT 90)
    endif()
endif()
