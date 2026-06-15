cmake_minimum_required(VERSION 3.16)
project(OpenGLProject)

set(CMAKE_CXX_STANDARD 17)

# ── Source files ──────────────────────────────────────────────
add_executable(OpenGLProject
    src/main.cpp
    src/glad.c
)

# ── Include paths ─────────────────────────────────────────────
target_include_directories(OpenGLProject PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# ── Libraries ─────────────────────────────────────────────────
target_link_directories(OpenGLProject PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)

if(WIN32)
    target_link_libraries(OpenGLProject PRIVATE
        glfw3
        opengl32
        gdi32
        user32
    )
elseif(APPLE)
    find_package(OpenGL REQUIRED)
    find_library(GLFW_LIB glfw PATHS /usr/local/lib /opt/homebrew/lib REQUIRED)
    target_link_libraries(OpenGLProject PRIVATE
        ${GLFW_LIB}
        OpenGL::GL
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
else()
    # Linux
    find_package(OpenGL REQUIRED)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        target_link_libraries(OpenGLProject PRIVATE glfw OpenGL::GL dl pthread)
    else()
        find_library(GLFW_LIB glfw3 glfw REQUIRED)
        target_link_libraries(OpenGLProject PRIVATE
            ${GLFW_LIB}
            OpenGL::GL
            dl pthread
        )
    endif()
endif()

# ── Compiler flags ────────────────────────────────────────────
if(MSVC)
    target_compile_options(OpenGLProject PRIVATE /W3 /O2)
else()
    target_compile_options(OpenGLProject PRIVATE
        -Wall -Wextra -O2
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:Debug>:-g>
    )
    # Statically embed MinGW runtime so the .exe has no libstdc++/libgcc/libwinpthread DLL deps
    if(WIN32)
        target_link_options(OpenGLProject PRIVATE
            -static-libgcc
            -static-libstdc++
            -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
        )
    endif()
endif()

# ── Windows: set console subsystem ────────────────────────────
if(WIN32)
    set_target_properties(OpenGLProject PROPERTIES
        WIN32_EXECUTABLE FALSE   # keep console window for debug output
    )
endif()
