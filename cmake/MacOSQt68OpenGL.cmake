# Qt 6.8.3's FindWrapOpenGL.cmake falls back to -framework AGL when AGL is
# absent from the macOS SDK. AGL is no longer available in macOS 26. Create
# Qt's wrapper target before find_package(Qt6) and keep the OpenGL dependency.
if(APPLE AND NOT TARGET WrapOpenGL::WrapOpenGL)
    find_package(OpenGL REQUIRED)
    if(NOT TARGET OpenGL::GL)
        message(FATAL_ERROR "macOS OpenGL framework is unavailable")
    endif()
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED GLOBAL)
    target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
endif()
