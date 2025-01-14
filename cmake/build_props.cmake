# ============================ Generic setup ==================================
# If platform is Emscripten
if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
    set(EMSCRIPTEN 1 BOOLEAN)
    unset(WIN32)
    unset(APPLE)
endif()

if(APPLE AND CMAKE_HOST_SYSTEM_VERSION VERSION_LESS 9)
    message("-- MacOS X 10.4 Tiger detected!")
    set(XTECH_MACOSX_TIGER TRUE)
endif()

# =============================== Policies ====================================

# Ninja requires custom command byproducts to be explicit.
if(POLICY CMP0058)
    cmake_policy(SET CMP0058 NEW)
endif()

# ExternalProject step targets fully adopt their steps.
if(POLICY CMP0114)
    cmake_policy(SET CMP0114 NEW)
endif()

# ========================= Macros and Functions ==============================

macro(xtech_add_warning_flag WARNINGFLAG WARNING_VAR)
    check_c_compiler_flag("${WARNINGFLAG}" HAVE_W_C_${WARNING_VAR})
    if(HAVE_W_C_${WARNING_VAR})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNINGFLAG}")
    endif()

    check_cxx_compiler_flag("${WARNINGFLAG}" HAVE_W_CXX_${WARNING_VAR})
    if(HAVE_W_CXX_${WARNING_VAR})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNINGFLAG}")
    endif()
endmacro()

macro(xtech_disable_warning_flag WARNINGFLAG WARNING_VAR)
    check_c_compiler_flag("-W${WARNINGFLAG}" HAVE_W_C_${WARNING_VAR})
    if(HAVE_W_C_${WARNING_VAR})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${WARNINGFLAG}")
    endif()

    check_cxx_compiler_flag("-W${WARNINGFLAG}" HAVE_W_CXX_${WARNING_VAR})
    if(HAVE_W_CXX_${WARNING_VAR})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-${WARNINGFLAG}")
    endif()
endmacro()

function(pge_cxx_standard STDVER)
    if(NOT WIN32)
        set(CMAKE_CXX_STANDARD ${STDVER} PARENT_SCOPE)
    elseif(MSVC AND CMAKE_VERSION VERSION_LESS "3.9.0" AND MSVC_VERSION GREATER_EQUAL "1900")
        include(CheckCXXCompilerFlag)
        CHECK_CXX_COMPILER_FLAG("/std:c++${STDVER}" _cpp_stdxx_flag_supported)
        if (_cpp_stdxx_flag_supported)
            add_compile_options("/std:c++${STDVER}")
        else()
            CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
            if (_cpp_latest_flag_supported)
                add_compile_options("/std:c++latest")
            endif()
        endif()
    else()
        set(CMAKE_CXX_STANDARD ${STDVER} PARENT_SCOPE)
    endif()
endfunction()


# ============================ Optimisations ==================================

# Strip garbage
if(APPLE)
    string(REGEX REPLACE "-O3" ""
        CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "-O3" ""
        CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")

    # Unify visibility to meet llvm's default.
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fvisibility-inlines-hidden" SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG)
    if(SUPPORTS_FVISIBILITY_INLINES_HIDDEN_FLAG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
    endif()
elseif(NOT MSVC)
    if(EMSCRIPTEN)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Os -fdata-sections -ffunction-sections")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Os -fdata-sections -ffunction-sections")
        set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -O3 -Os -fdata-sections -ffunction-sections")
        set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -O3 -Os -fdata-sections -ffunction-sections")
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
            set(LINK_FLAGS_MINSIZEREL  "${LINK_FLAGS_MINSIZEREL} -dead_strip")
        else()
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
            set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -Wl,--gc-sections -Wl,-s")
            set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -Wl,--gc-sections -Wl,-s")
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
            set(LINK_FLAGS_MINSIZEREL  "${LINK_FLAGS_MINSIZEREL} -Wl,--gc-sections -Wl,-s")
        endif()
    else()
        string(REGEX REPLACE "-O3" ""
            CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
        string(REGEX REPLACE "-O3" ""
            CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fdata-sections -ffunction-sections")
        if(ANDROID)
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -funwind-tables")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -funwind-tables")
        elseif(NINTENDO_3DS OR NINTENDO_WII)
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -Wl,--gc-sections")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -Wl,--gc-sections")
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -g -Wl,--gc-sections")
        elseif(VITA)
            # VitaSDK specifies -O2 for release configurations. PS Vita Support - Axiom 2022
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g -I../src -Wl,--gc-sections -DVITA=1 -fcompare-debug-second")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -I../src -Wl,--gc-sections -DVITA=1 -fpermissive -fcompare-debug-second -fno-optimize-sibling-calls -Wno-class-conversion")
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections")
        elseif(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Wl,--gc-sections -Wl,-s")
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
        else()
            set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
        endif()
    endif()
endif()

# Global optimization flags
if(NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
endif()

if(ANDROID)
    set(ANDROID_PLATFORM "android-16")
    set(ANDROID_CMAKE_FLAGS
        "-DANDROID_ABI=${ANDROID_ABI}"
        "-DANDROID_NDK=${ANDROID_NDK}"
        "-DANDROID_STL=c++_static"
        "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
        "-DANDROID_PLATFORM=${ANDROID_PLATFORM}"
        "-DANDROID_TOOLCHAIN=${ANDROID_TOOLCHAIN}"
        "-DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}"
        "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        "-DANDROID_ARM_NEON=${ANDROID_ARM_NEON}"
    )
endif()

if(VITA)
    include(cmake/vita_buildprops.cmake)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    add_definitions(-DNDEBUG)
endif()

if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug" AND NOT ANDROID)
    set(PGE_LIBS_DEBUG_SUFFIX "d")
else()
    set(PGE_LIBS_DEBUG_SUFFIX "")
endif()


# ============================= Warnings ======================================

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
    if(CMAKE_C_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    endif()

    # Remove "/showIncludes" flag
    if(CMAKE_CXX_FLAGS MATCHES "/showIncludes")
        string(REGEX REPLACE "/showIncludes" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()
    if(CMAKE_C_FLAGS MATCHES "/showIncludes")
        string(REGEX REPLACE "/showIncludes" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    endif()

    # Disable bogus MSVC warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4244 /wd4551 /wd4276 /wd6388")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244 /wd4551 /wd4276 /wd6388")

else()
    xtech_add_warning_flag("-Wall" ALL)
    xtech_add_warning_flag("-Wextra" EXTRA)
    if(NOT HAVE_W_EXTRA)
        xtech_add_warning_flag("-W" W)
    endif()
    xtech_add_warning_flag("-Wpedantic" PEDANTIC_WARNING)
    xtech_disable_warning_flag("variadic-macros" NO_VARIADIC_MACROS_WARNING)
    xtech_disable_warning_flag("psabi" NO_PSABI_WARNING)
endif()


# ================================ Tweaks ====================================

# -fPIC thing
if(LIBRARY_PROJECT AND NOT WIN32 AND NOT VITA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

if(UNIX) # When include/library/binary directory name is not usual in a system, make symbolic links for them
    if(NOT "${CMAKE_INSTALL_LIBDIR}" STREQUAL "lib")
        message("${CMAKE_INSTALL_LIBDIR} IS NOT STREQUAL lib")
        file(MAKE_DIRECTORY "${DEPENDENCIES_INSTALL_DIR}")
        execute_process(COMMAND ln -s "lib" "${DEPENDENCIES_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(NOT "${CMAKE_INSTALL_BINDIR}" STREQUAL "bin")
        message("${CMAKE_INSTALL_BINDIR} IS NOT STREQUAL bin")
        file(MAKE_DIRECTORY "${DEPENDENCIES_INSTALL_DIR}")
        execute_process(COMMAND ln -s "bin" "${DEPENDENCIES_INSTALL_DIR}/${CMAKE_INSTALL_BINDIR}")
    endif()
    if(NOT "${CMAKE_INSTALL_INCLUDEDIR}" STREQUAL "include")
        message("${CMAKE_INSTALL_INCLUDEDIR} IS NOT STREQUAL lib")
        file(MAKE_DIRECTORY "${DEPENDENCIES_INSTALL_DIR}")
        execute_process(COMMAND ln -s "include" "${DEPENDENCIES_INSTALL_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()
endif()

if(APPLE)
    # Prevent "directory not exists" warnings when building XCode as project
    file(MAKE_DIRECTORY ${DEPENDENCIES_INSTALL_DIR}/lib/Debug)
    file(MAKE_DIRECTORY ${DEPENDENCIES_INSTALL_DIR}/lib/Release)
    # Don't store built executables into "Debug" and "Release" folders
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_BUNDLE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_BUNDLE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
endif()

if(CMAKE_STATIC_LIBRARY_PREFIX STREQUAL "" AND CMAKE_STATIC_LIBRARY_SUFFIX STREQUAL ".lib")
    set(LIBRARY_STATIC_NAME_SUFFIX "-static")
else()
    set(LIBRARY_STATIC_NAME_SUFFIX "")
endif()

# Library path helpers
macro(set_static_lib OUTPUT_VAR LIBDIR LIBNAME)
    set(${OUTPUT_VAR} "${LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${LIBNAME}${PGE_LIBS_DEBUG_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}")
endmacro()

macro(set_shared_lib OUTPUT_VAR LIBDIR LIBNAME)
    if(WIN32)
        set(${OUTPUT_VAR} "${LIBDIR}/${CMAKE_IMPORT_LIBRARY_PREFIX}${LIBNAME}${PGE_LIBS_DEBUG_SUFFIX}${CMAKE_IMPORT_LIBRARY_SUFFIX}")
    else()
        set(${OUTPUT_VAR} "${LIBDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${LIBNAME}${PGE_LIBS_DEBUG_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
endmacro()
