if(NOT WIN32)
    message(STATUS "SC4AdaptiveLotsDll is only built on Win32. Skipping the dll target.")
    return()
endif()

set(GZCOM_DIR "${CMAKE_SOURCE_DIR}/vendor/gzcom-dll/gzcom-dll")
set(SC4_DLL_UTILITIES_DIR "${CMAKE_SOURCE_DIR}/vendor/sc4-dll-utilities/sc4-dll-utilities")

file(GLOB_RECURSE GZCOM_HEADERS CONFIGURE_DEPENDS
    "${GZCOM_DIR}/include/*.h"
    "${GZCOM_DIR}/include/*.hpp"
)
file(GLOB_RECURSE GZCOM_SOURCES CONFIGURE_DEPENDS
    "${GZCOM_DIR}/src/*.cpp"
)
list(REMOVE_ITEM GZCOM_SOURCES
    "${GZCOM_DIR}/src/main.cpp"
    "${GZCOM_DIR}/src/cGZExampleDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZExtraCheatsDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZExtraExtraCheatsDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZCityHallUpgradeDllDirector.cpp"
)

add_library(gzcom2 STATIC ${GZCOM_SOURCES} ${GZCOM_HEADERS})
target_include_directories(gzcom2 PUBLIC "${GZCOM_DIR}/include")

file(GLOB_RECURSE SC4_DLL_UTILITIES_HEADERS CONFIGURE_DEPENDS
    "${SC4_DLL_UTILITIES_DIR}/include/*.h"
    "${SC4_DLL_UTILITIES_DIR}/include/*.hpp"
)
file(GLOB_RECURSE SC4_DLL_UTILITIES_SOURCES CONFIGURE_DEPENDS
    "${SC4_DLL_UTILITIES_DIR}/src/*.cpp"
)
list(REMOVE_ITEM SC4_DLL_UTILITIES_SOURCES
    "${SC4_DLL_UTILITIES_DIR}/src/Logger.cpp"
)

find_path(MINI_INCLUDE_DIR NAMES mini/ini.h)
if(NOT MINI_INCLUDE_DIR)
    message(FATAL_ERROR "mini/ini.h was not found. Ensure the pulzed-mini vcpkg dependency is installed.")
endif()

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

string(REPLACE "." "," SC4_RESOURCE_FILE_VERSION "${PROJECT_VERSION}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
    @ONLY
)

add_library(${PROJECT_NAME} SHARED
    ${PROJECT_SOURCES}
    ${SC4_DLL_UTILITIES_HEADERS}
    ${SC4_DLL_UTILITIES_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/SC4AdaptiveLotsDll.def"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MINI_INCLUDE_DIR}"
    "${SC4_DLL_UTILITIES_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    gzcom2
    spdlog::spdlog
    WIL::WIL
    kernel32
    user32
    Version
    ddraw
    dxguid
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SC4AdaptiveLotsDll"
    SUFFIX ".dll"
    PREFIX ""
)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

if(BUILD_TESTING)
    add_executable(SC4AdaptiveLotsMappingTests
        "${CMAKE_SOURCE_DIR}/tests/LotMappingTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/LotMapping.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/RingMetadata.cpp"
    )
    target_include_directories(SC4AdaptiveLotsMappingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME SC4AdaptiveLotsMappingTests COMMAND SC4AdaptiveLotsMappingTests)

    add_executable(SC4AdaptiveLotsTooltipTests
        "${CMAKE_SOURCE_DIR}/tests/TooltipTextTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/RingMetadata.cpp"
    )
    target_include_directories(SC4AdaptiveLotsTooltipTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME SC4AdaptiveLotsTooltipTests COMMAND SC4AdaptiveLotsTooltipTests)
endif()

if(WIN32 AND SC4_ENABLE_PLUGIN_DEPLOYMENT)
    if(SC4_PLUGINS_DIR)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SC4_PLUGINS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${PROJECT_NAME}>
                "${SC4_PLUGINS_DIR}/"
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${CMAKE_SOURCE_DIR}/dist/SC4AdaptiveLotsDll.ini
                -DDST=${SC4_PLUGINS_DIR}/SC4AdaptiveLotsDll.ini
                -P ${CMAKE_SOURCE_DIR}/cmake/CopyIfNotExists.cmake
            COMMENT "Deploying SC4AdaptiveLotsDll.dll to the SimCity 4 Plugins folder"
        )
        message(STATUS "Plugin deployment enabled: ${SC4_PLUGINS_DIR}")
    else()
        message(WARNING "SC4_PLUGINS_DIR is empty. Automatic Plugins deployment is disabled.")
    endif()
endif()
