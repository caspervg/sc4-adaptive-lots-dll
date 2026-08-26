if(NOT WIN32)
    message(STATUS "SC4TemplateDll is only built on Win32. Skipping the dll target.")
    return()
endif()

set(SC4_RENDER_SERVICES_ROOT "${CMAKE_SOURCE_DIR}/vendor/sc4-render-services" CACHE PATH "Path to sc4-render-services")
set(SC4_RENDER_SERVICES_PUBLIC_DIR "${SC4_RENDER_SERVICES_ROOT}/src")

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
    "${CMAKE_CURRENT_SOURCE_DIR}/SC4TemplateDll.def"
    "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    SC4_TEMPLATE_DLL_NAME="${PROJECT_NAME}"
    SC4_TEMPLATE_DLL_VERSION="${PROJECT_VERSION}"
    SC4_TEMPLATE_DLL_VERSION_LABEL="${SC4_TEMPLATE_APP_VERSION}"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MINI_INCLUDE_DIR}"
    "${SC4_DLL_UTILITIES_DIR}/include"
)

if(SC4_ENABLE_IMGUI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DLL_IMPORT)
    target_include_directories(${PROJECT_NAME} PRIVATE "${SC4_RENDER_SERVICES_PUBLIC_DIR}")
endif()

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

if(SC4_ENABLE_IMGUI)
    target_link_libraries(${PROJECT_NAME} PRIVATE SC4RenderServices::imgui)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SC4TemplateDll"
    SUFFIX ".dll"
    PREFIX ""
)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

if(WIN32 AND SC4_ENABLE_PLUGIN_DEPLOYMENT)
    if(SC4_PLUGINS_DIR)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SC4_PLUGINS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${PROJECT_NAME}>
                "${SC4_PLUGINS_DIR}/"
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${CMAKE_SOURCE_DIR}/dist/SC4TemplateDll.ini
                -DDST=${SC4_PLUGINS_DIR}/SC4TemplateDll.ini
                -P ${CMAKE_SOURCE_DIR}/cmake/CopyIfNotExists.cmake
            COMMENT "Deploying SC4TemplateDll.dll to the SimCity 4 Plugins folder"
        )
        message(STATUS "Plugin deployment enabled: ${SC4_PLUGINS_DIR}")
    else()
        message(WARNING "SC4_PLUGINS_DIR is empty. Automatic Plugins deployment is disabled.")
    endif()
endif()
