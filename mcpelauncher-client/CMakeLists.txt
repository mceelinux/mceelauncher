cmake_minimum_required (VERSION 2.6)

project(mcpelauncher-client LANGUAGES CXX ASM)

find_package(CURL REQUIRED)

git_commit_hash(${CMAKE_CURRENT_SOURCE_DIR} CLIENT_GIT_COMMIT_HASH)
configure_file(src/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/build_info/build_info.h)

add_executable(mcpelauncher-client src/main.cpp src/main.h src/window_callbacks.cpp src/window_callbacks.h src/xbox_live_helper.cpp src/xbox_live_helper.h src/splitscreen_patch.cpp src/splitscreen_patch.h src/strafe_sprint_patch.cpp src/strafe_sprint_patch.h src/fake_swappygl.cpp src/fake_swappygl.h src/cll_upload_auth_step.cpp src/cll_upload_auth_step.h src/gl_core_patch.cpp src/gl_core_patch.h src/hbui_patch.cpp src/hbui_patch.h src/utf8_util.h src/shader_error_patch.cpp src/shader_error_patch.h src/jni/jni_descriptors.cpp src/jni/java_types.h src/jni/main_activity.cpp src/jni/main_activity.h src/jni/asset_manager.cpp src/jni/asset_manager.h src/jni/store.cpp src/jni/store.h src/jni/cert_manager.cpp src/jni/cert_manager.h src/jni/http_stub.cpp src/jni/http_stub.h src/jni/package_source.cpp src/jni/package_source.h src/jni/jni_support.h src/jni/jni_support.cpp src/jni/fmod.h src/jni/fmod.cpp src/fake_looper.cpp src/fake_looper.h src/fake_window.cpp src/fake_window.h src/fake_assetmanager.cpp src/fake_assetmanager.h src/fake_egl.cpp src/fake_egl.h src/fake_inputqueue.cpp src/fake_inputqueue.h src/symbols.cpp src/symbols.h src/text_input_handler.cpp src/text_input_handler.h src/jni/xbox_live.cpp src/jni/xbox_live.h src/core_patches.cpp src/core_patches.h  src/thread_mover.cpp src/thread_mover.h src/jni/lib_http_client.cpp src/jni/lib_http_client.h src/jni/lib_http_client_websocket.cpp src/jni/lib_http_client_websocket.h src/jni/accounts.cpp src/jni/accounts.h src/jni/arrays.cpp src/jni/arrays.h src/jni/jbase64.cpp src/jni/jbase64.h src/jni/locale.cpp src/jni/locale.h src/jni/securerandom.cpp src/jni/securerandom.h src/jni/signature.cpp src/jni/signature.h src/jni/uuid.cpp src/jni/uuid.h src/jni/webview.cpp src/jni/webview.h src/util.cpp src/util.h src/xal_webview_factory.cpp src/xal_webview_factory.h src/xal_webview.h src/settings.cpp src/settings.h )
target_link_libraries(mcpelauncher-client logger properties-parser mcpelauncher-core gamewindow filepicker msa-daemon-client daemon-server-utils cll-telemetry argparser baron android-support-headers libc-shim ${CURL_LIBRARIES})
target_include_directories(mcpelauncher-client PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/build_info/ ${CURL_INCLUDE_DIRS})

option(NO_OPENSSL "disable openssl code" OFF)
if (NO_OPENSSL)
    target_compile_definitions(mcpelauncher-client PRIVATE NO_OPENSSL)
else()
    find_package(OpenSSL REQUIRED)
    target_sources(mcpelauncher-client PRIVATE src/jni/shahasher.cpp src/jni/shahasher.h src/jni/ecdsa.cpp src/jni/ecdsa.h)
    target_link_libraries(mcpelauncher-client OpenSSL::Crypto)
endif()

if (IS_ARM_BUILD)
    if (IS_ARMHF_BUILD AND NOT IS_64BIT)
        target_sources(mcpelauncher-client PRIVATE src/armhf_support.cpp src/armhf_support.h)
        target_compile_definitions(mcpelauncher-client PRIVATE USE_ARMHF_SUPPORT)
    endif()
else()
    target_sources(mcpelauncher-client PRIVATE src/cpuid.cpp src/cpuid.h src/xbox_shutdown_patch.cpp src/xbox_shutdown_patch.h src/texel_aa_patch.cpp src/texel_aa_patch.h)
endif()

if(TARGET SDL3::SDL3)
    message(VERBOSE "USING SDL3")
    target_link_libraries(mcpelauncher-client SDL3::SDL3)
    if (USE_SDL3_AUDIO)
        message(VERBOSE "USING SDL3AUDIO")
        target_sources(mcpelauncher-client PRIVATE src/jni/sdl3audio.cpp src/jni/sdl3audio.h src/fake_audio.cpp src/fake_audio.h)
        target_compile_definitions(mcpelauncher-client PRIVATE HAVE_SDL3AUDIO)
    endif()
endif()

if (NOT USE_SDL3_AUDIO)
    find_package(PulseAudio)
    message(STATUS "USING EXPERIMENTAL PULSEAUDIO BACKEND")
    if (PULSEAUDIO_FOUND AND PULSEAUDIOSIMPLE_FOUND)
        target_sources(mcpelauncher-client PRIVATE src/jni/pulseaudio.cpp src/jni/pulseaudio.h)
        target_link_libraries(mcpelauncher-client ${PULSEAUDIO_LIBRARIES} ${PULSEAUDIOSIMPLE_LIBRARIES})
        target_compile_definitions(mcpelauncher-client PRIVATE HAVE_PULSEAUDIO)
    endif()
endif()

option(XAL_WEBVIEW_USE_CLI "use cli for login to xboxlive" OFF)
if (XAL_WEBVIEW_USE_CLI)
    target_sources(mcpelauncher-client PRIVATE src/xal_webview_cli.cpp src/xal_webview_cli.h)
    target_compile_definitions(mcpelauncher-client PRIVATE XAL_WEBVIEW_USE_CLI)
endif()
option(XAL_WEBVIEW_USE_QT "use a qtwebengine webview for login to xboxlive" ON)
if (XAL_WEBVIEW_USE_QT)
    target_sources(mcpelauncher-client PRIVATE src/xal_webview_qt.cpp src/xal_webview_qt.h)
    target_compile_definitions(mcpelauncher-client PRIVATE XAL_WEBVIEW_USE_QT)
endif()
option(XAL_WEBVIEW_QT_PATH "Additional paths in which the mcpelauncher-webview should be looked in. Relative to the application directory." )
if (XAL_WEBVIEW_QT_PATH)
    target_compile_definitions(mcpelauncher-client PRIVATE -DMCPELAUNCHER_WEBVIEW_PATH="${XAL_WEBVIEW_QT_PATH}")
endif()
option(ENABLE_ERROR_WINDOW "Use an errorwindow to host certain errormessages" TRUE)
if (ENABLE_ERROR_WINDOW)
    target_link_libraries(mcpelauncher-client mcpelauncher-errorwindow)
    target_compile_definitions(mcpelauncher-client PRIVATE MCPELAUNCHER_ENABLE_ERROR_WINDOW)
endif()

if(USE_SNMALLOC)
    target_link_libraries(mcpelauncher-client PRIVATE snmalloc)
    target_sources(mcpelauncher-client PRIVATE ${snmalloc_ext_SOURCE_DIR}/src/snmalloc/override/new.cc)
endif()
option(USE_IMGUI "use imgui for an overlay ui" ON)
if(USE_IMGUI AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../imgui" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui.cpp")
    target_sources(mcpelauncher-client PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui_demo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui_tables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/backends/imgui_impl_opengl3.cpp
        src/glad.c
        src/imgui_ui.cpp
        src/imgui_ui.h
    )

    target_include_directories(mcpelauncher-client BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/../imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(mcpelauncher-client PRIVATE
        IMGUI_IMPL_OPENGL_ES3
        USE_IMGUI
    )
endif()

install(TARGETS mcpelauncher-client RUNTIME COMPONENT mcpelauncher-client DESTINATION bin)
include(CPackSettings.cmake)
