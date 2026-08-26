set(SC4_RENDER_SERVICES_ROOT "${CMAKE_SOURCE_DIR}/vendor/sc4-render-services" CACHE PATH "Path to sc4-render-services")
set(SC4_RENDER_SERVICES_PUBLIC_DIR "${SC4_RENDER_SERVICES_ROOT}/src")
set(SC4_RENDER_SERVICES_IMGUI_DIR "${SC4_RENDER_SERVICES_ROOT}/vendor/d3d7imgui/ImGui")

if(NOT EXISTS "${SC4_RENDER_SERVICES_IMGUI_DIR}/imgui.h")
    message(
        FATAL_ERROR
        "Missing sc4-render-services ImGui sources. Initialize submodules with: "
        "'git submodule update --init --recursive'."
    )
endif()

file(GLOB SC4_RENDER_SERVICES_IMGUI_CORE_SOURCES CONFIGURE_DEPENDS
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/*.cpp"
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/*.h"
)

set(SC4_RENDER_SERVICES_IMGUI_BACKENDS
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/imgui_impl_win32.h"
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/imgui_impl_win32.cpp"
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/imgui_impl_dx7.h"
    "${SC4_RENDER_SERVICES_IMGUI_DIR}/imgui_impl_dx7.cpp"
)

add_library(sc4_render_services_imgui SHARED
    ${SC4_RENDER_SERVICES_IMGUI_CORE_SOURCES}
    ${SC4_RENDER_SERVICES_IMGUI_BACKENDS}
)

add_library(SC4RenderServices::imgui ALIAS sc4_render_services_imgui)

target_include_directories(sc4_render_services_imgui PUBLIC
    "${SC4_RENDER_SERVICES_IMGUI_DIR}"
)

target_compile_definitions(sc4_render_services_imgui PRIVATE
    IMGUI_DLL_EXPORT
)

if(WIN32)
    target_link_libraries(sc4_render_services_imgui PRIVATE
        user32
        gdi32
        ddraw
        dxguid
    )
endif()

set_target_properties(sc4_render_services_imgui PROPERTIES
    OUTPUT_NAME "imgui"
)
