#############################################################################
# Name:        StageFiles.cmake
# Purpose:     Stages an application's resources and binary for packaging
# Author:      Blake Madden
# Created:     2025-10-26
# Copyright:   (c) 2025 Blake Madden
# License:     Eclipse Public License 2.0
#############################################################################

# ==========================================================================================
#  Overview
#  ------------------------------------------------------------------------------------------
#  Post-build helpers that copy an application's resources and binary into its build folder
#  and into these packaging staging areas:
#    • Windows installer folder:  <WINDOWS_DIR>/resources and <WINDOWS_DIR>/release
#    • macOS app bundle:          Contents/Resources (and the whole bundle into <MACOS_DIR>)
#    • AppImage AppDir:           <APPIMAGE_DIR>/usr/bin and <APPIMAGE_DIR>/usr/share/locale
#    • DEB and RPM roots:         <DIR>/usr/bin, <DIR>/usr/share/<APP_DATA_NAME>,
#                                 and <DIR>/usr/share/locale
#
#  Resources are staged into every configured area on every build. Areas that are not
#  passed to stage_setup() are skipped. The binary is only staged for the host platform,
#  and only in Release builds.
#
#  Settings are stored as properties on the target, so several targets in one project
#  can each be staged differently.
#
#  Usage:
#      include(StageFiles.cmake)
#
#      stage_setup(myapp
#          APP_DATA_NAME      "MyApp"
#          TRANSLATION_DOMAIN myapp
#          WINDOWS_DIR        installers/windows
#          MACOS_DIR          installers/macos/release
#          APPIMAGE_DIR       installers/unix/AppDir
#          DEB_DIR            installers/unix/deb-stage
#          RPM_DIR            installers/unix/rpm-stage)
#
#      stage_directory(myapp "resources/themes" "themes")
#      stage_file(myapp "LICENSE")
#      stage_file(myapp "$<TARGET_FILE_DIR:myapp>/res.wad")
#      stage_translations(myapp es)
#      stage_binary(myapp)
#
#  Notes:
#    • All functions must be called after the target is defined.
#    • stage_setup() must be called before any other stage_*() function.
#    • Call stage_binary() after all resource staging, so that the copied macOS bundle
#      includes all resources.
# ==========================================================================================

# Requires: CMake's FindGettext module (built-in)
# Will set GETTEXT_MSGFMT_EXECUTABLE if found.
find_package(Gettext QUIET)

if(NOT GETTEXT_MSGFMT_EXECUTABLE AND WIN32)
    set(_STAGE_POEDIT_MSGFMT "$ENV{ProgramFiles}/Poedit/GettextTools/bin/msgfmt.exe")
    if(EXISTS "${_STAGE_POEDIT_MSGFMT}")
        set(GETTEXT_MSGFMT_EXECUTABLE "${_STAGE_POEDIT_MSGFMT}"
            CACHE FILEPATH "Path to msgfmt from Poedit" FORCE)
        set(GETTEXT_FOUND TRUE CACHE BOOL "Gettext found through Poedit" FORCE)
        message(STATUS "Found msgfmt.exe via Poedit: ${_STAGE_POEDIT_MSGFMT}")
    endif()
endif()

# ==========================================================================================
#  Function: stage_setup
#  ------------------------------------------------------------------------------------------
#  Configures where a target's files are staged.
#
#  Usage:
#      stage_setup(<TARGET>
#          [APP_DATA_NAME <name>]
#          [TRANSLATION_DOMAIN <domain>]
#          [LOCALE_DIR <dir>]
#          [WX_LOCALE_DIR <dir>]
#          [WINDOWS_DIR <dir>]
#          [MACOS_DIR <dir>]
#          [APPIMAGE_DIR <dir>]
#          [DEB_DIR <dir>]
#          [RPM_DIR <dir>])
#
#  Arguments:
#      APP_DATA_NAME
#          Folder name under usr/share for DEB and RPM resources. This should match the
#          application's name (without spaces), which is where it looks for its resources.
#          Defaults to the target name.
#      TRANSLATION_DOMAIN
#          Base name of the application's .mo file. Defaults to the target name.
#      LOCALE_DIR
#          Folder holding <LANG>.po or <LANG>.mo files, and optionally <LANG>/wxstd.mo.
#          Defaults to ${CMAKE_CURRENT_SOURCE_DIR}/locale.
#      WX_LOCALE_DIR
#          Folder holding the wxWidgets <LANG>.po files.
#          Defaults to ${wxWidgets_BASEDIR}/locale (if wxWidgets_BASEDIR is defined).
#      WINDOWS_DIR
#          Windows installer root. Resources go into resources/ and the binary into release/.
#      MACOS_DIR
#          Folder that the finished .app bundle is copied into.
#      APPIMAGE_DIR
#          AppImage AppDir root.
#      DEB_DIR, RPM_DIR
#          DEB and RPM package roots.
#
#  Relative paths are resolved against ${CMAKE_CURRENT_SOURCE_DIR}.
# ==========================================================================================
function(stage_setup TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "stage_setup(): '${TARGET}' is not a target.")
    endif()

    set(_DIR_KEYS LOCALE_DIR WX_LOCALE_DIR WINDOWS_DIR MACOS_DIR APPIMAGE_DIR DEB_DIR RPM_DIR)
    set(_ALL_KEYS APP_DATA_NAME TRANSLATION_DOMAIN ${_DIR_KEYS})

    cmake_parse_arguments(PARSE_ARGV 1 _ARG "" "${_ALL_KEYS}" "")
    if(_ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "stage_setup(): unknown arguments '${_ARG_UNPARSED_ARGUMENTS}'.")
    endif()

    if(NOT _ARG_APP_DATA_NAME)
        set(_ARG_APP_DATA_NAME "${TARGET}")
    endif()
    if(NOT _ARG_TRANSLATION_DOMAIN)
        set(_ARG_TRANSLATION_DOMAIN "${TARGET}")
    endif()
    if(NOT _ARG_LOCALE_DIR)
        set(_ARG_LOCALE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/locale")
    endif()
    if(NOT _ARG_WX_LOCALE_DIR AND DEFINED wxWidgets_BASEDIR)
        set(_ARG_WX_LOCALE_DIR "${wxWidgets_BASEDIR}/locale")
    endif()

    foreach(_KEY IN LISTS _DIR_KEYS)
        if(_ARG_${_KEY})
            cmake_path(ABSOLUTE_PATH _ARG_${_KEY}
                BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE)
        endif()
    endforeach()

    foreach(_KEY IN LISTS _ALL_KEYS)
        set_property(TARGET ${TARGET} PROPERTY STAGE_${_KEY} "${_ARG_${_KEY}}")
    endforeach()
    set_property(TARGET ${TARGET} PROPERTY STAGE_CONFIGURED TRUE)

    message(STATUS "Staging configured for '${TARGET}':")
    foreach(_KEY WINDOWS_DIR MACOS_DIR APPIMAGE_DIR DEB_DIR RPM_DIR)
        if(_ARG_${_KEY})
            message(STATUS "  ${_KEY}: ${_ARG_${_KEY}}")
        endif()
    endforeach()
endfunction()

# ==========================================================================================
#  Function: stage_directory
#  ------------------------------------------------------------------------------------------
#  Recursively copies a folder into the build folder and every resource staging area.
#
#  Usage:
#      stage_directory(<TARGET> <SRC> <DEST_REL> [CLEAN])
#
#  Example:
#      stage_directory(myapp "resources/report-themes" "report-themes" CLEAN)
#      stage_directory(myapp "resources/scripting" "")
#
#  Arguments:
#      SRC
#          Source folder. Relative paths are resolved against ${CMAKE_CURRENT_SOURCE_DIR}.
#      DEST_REL
#          Subfolder inside each resource root. Use "" to copy into the resource root.
#      CLEAN
#          Deletes each destination folder before copying, so that files removed from SRC
#          are also removed from the staging areas. Requires a non-empty DEST_REL.
#
#  A missing source folder is a configure-time error, unless SRC uses a generator expression.
# ==========================================================================================
function(stage_directory TARGET SRC DEST_REL)
    _stage_check_setup(${TARGET})

    cmake_parse_arguments(PARSE_ARGV 3 _ARG "CLEAN" "" "")
    if(_ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "stage_directory(): unknown arguments '${_ARG_UNPARSED_ARGUMENTS}'.")
    endif()
    if(_ARG_CLEAN AND DEST_REL STREQUAL "")
        message(FATAL_ERROR
            "stage_directory(): CLEAN requires a DEST_REL, or it would delete the resource roots.")
    endif()

    _stage_resolve_source("${SRC}" _SRC _HAS_GENEX)

    if(NOT _HAS_GENEX AND NOT IS_DIRECTORY "${_SRC}")
        message(FATAL_ERROR
            "Source folder '${_SRC}' not found and cannot be copied to staging areas.")
    endif()

    _stage_resource_dirs(${TARGET} "${DEST_REL}" _DEST_DIRS)

    set(_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E echo "Staging '${SRC}' into build and installer folders.")
    foreach(_DIR IN LISTS _DEST_DIRS)
        if(_ARG_CLEAN)
            list(APPEND _COMMANDS COMMAND ${CMAKE_COMMAND} -E rm -rf "${_DIR}")
        endif()
        list(APPEND _COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_SRC}" "${_DIR}")
    endforeach()

    add_custom_command(TARGET ${TARGET} POST_BUILD ${_COMMANDS} VERBATIM)
endfunction()

# ==========================================================================================
#  Function: stage_file
#  ------------------------------------------------------------------------------------------
#  Copies a single file into the build folder and every resource staging area.
#
#  Usage:
#      stage_file(<TARGET> <SRC> [<DEST_REL>])
#
#  Example:
#      stage_file(myapp "LICENSE")
#      stage_file(myapp "${CMAKE_BINARY_DIR}/citations.bib")
#      stage_file(myapp "src/import/glyphlist.txt" "pdf-tables")
#      stage_file(myapp "$<TARGET_FILE_DIR:myapp>/res.wad")
#
#  Arguments:
#      SRC
#          Source file. Relative paths are resolved against ${CMAKE_CURRENT_SOURCE_DIR}.
#          Generator expressions are allowed (e.g., for files built after the target),
#          but the file name itself must be literal.
#      DEST_REL
#          Optional subfolder inside each resource root.
#
#  A missing source file is a configure-time error, unless SRC uses a generator expression.
# ==========================================================================================
function(stage_file TARGET SRC)
    _stage_check_setup(${TARGET})

    set(_DEST_REL "")
    if(ARGC GREATER 2)
        set(_DEST_REL "${ARGV2}")
    endif()

    _stage_resolve_source("${SRC}" _SRC _HAS_GENEX)

    if(NOT _HAS_GENEX AND NOT EXISTS "${_SRC}")
        message(FATAL_ERROR
            "Source file '${_SRC}' not found and cannot be copied to staging areas.")
    endif()

    get_filename_component(_NAME "${SRC}" NAME)
    _stage_resource_dirs(${TARGET} "${_DEST_REL}" _DEST_DIRS)
    _stage_copy_file(${TARGET} "${_SRC}" "${_NAME}" "${_DEST_DIRS}" "${_NAME}")
endfunction()

# ==========================================================================================
#  Function: stage_translations
#  ------------------------------------------------------------------------------------------
#  Copies a language's compiled translations into the build folder and every staging area.
#  This includes the application's catalog (<TRANSLATION_DOMAIN>.mo) and the wxWidgets
#  stock catalog (wxstd.mo).
#
#  A prebuilt .mo file is used if present. Otherwise, the .po file is compiled with msgfmt
#  (if available). A missing catalog is reported but not an error.
#
#  Usage:
#      stage_translations(<TARGET> <LANG>)
#
#  Example:
#      stage_translations(myapp es)
#
#  Sources:
#      <LOCALE_DIR>/<LANG>.mo or <LOCALE_DIR>/<LANG>.po
#      <LOCALE_DIR>/<LANG>/wxstd.mo or <WX_LOCALE_DIR>/<LANG>.po
#
#  Destinations:
#      Build folder and Windows:  <root>/<LANG>/<file>.mo
#      AppImage:                  <root>/usr/share/locale/<LANG>/LC_MESSAGES/<file>.mo
#      DEB and RPM:               <root>/usr/share/locale/<LANG>/LC_MESSAGES/<DOMAIN>.mo
#                                 <root>/usr/share/<APP_DATA_NAME>/locale/<LANG>/LC_MESSAGES/wxstd.mo
#
#  wxstd.mo goes into the app's data folder for DEB and RPM, because several packages
#  installing it into /usr/share/locale would conflict. The application must add
#  <data dir>/locale as a catalog lookup prefix before loading its locale.
# ==========================================================================================
function(stage_translations TARGET LANG)
    _stage_check_setup(${TARGET})

    _stage_get(${TARGET} TRANSLATION_DOMAIN _DOMAIN)
    _stage_get(${TARGET} LOCALE_DIR _LOCALE_DIR)
    _stage_get(${TARGET} WX_LOCALE_DIR _WX_LOCALE_DIR)

    set(_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/locale/${LANG}")

    # Application catalog
    _stage_catalog(${TARGET} ${LANG}
        "${_LOCALE_DIR}/${LANG}.mo"
        "${_LOCALE_DIR}/${LANG}.po"
        "${_BIN_DIR}/${_DOMAIN}.mo"
        "${_DOMAIN}.mo"
        "${_DOMAIN}"
        FALSE)

    # wxWidgets stock catalog
    set(_WX_SRC_PO "")
    if(_WX_LOCALE_DIR)
        set(_WX_SRC_PO "${_WX_LOCALE_DIR}/${LANG}.po")
    endif()
    _stage_catalog(${TARGET} ${LANG}
        "${_LOCALE_DIR}/${LANG}/wxstd.mo"
        "${_WX_SRC_PO}"
        "${_BIN_DIR}/wxstd.mo"
        "wxstd.mo"
        "wxWidgets"
        TRUE)
endfunction()

# ==========================================================================================
#  Function: stage_binary
#  ------------------------------------------------------------------------------------------
#  Copies the built binary into the host platform's staging areas (Release config only):
#    • Windows:  <WINDOWS_DIR>/release/
#    • macOS:    the whole .app bundle into <MACOS_DIR>/
#    • Linux:    <APPIMAGE_DIR>/usr/bin/, <DEB_DIR>/usr/bin/, and <RPM_DIR>/usr/bin/
#
#  The Release check happens at build time, so this also works with multi-config
#  generators (Visual Studio, Xcode). Other configs skip these copies.
#
#  Usage:
#      stage_binary(<TARGET>)
#
#  Call this after all other stage_*() functions, so that the copied macOS bundle
#  includes the staged resources.
# ==========================================================================================
function(stage_binary TARGET)
    _stage_check_setup(${TARGET})

    set(_BINARY "$<TARGET_FILE:${TARGET}>")
    set(_BINARY_NAME "$<TARGET_FILE_NAME:${TARGET}>")

    # Each operation runs only for the Release config ("cmake -E true" does nothing)
    set(_ECHO "$<IF:$<CONFIG:Release>,echo,true>")
    set(_MKDIR "$<IF:$<CONFIG:Release>,make_directory,true>")
    set(_COPY "$<IF:$<CONFIG:Release>,copy_if_different,true>")
    set(_COPY_DIR "$<IF:$<CONFIG:Release>,copy_directory,true>")
    set(_REMOVE "$<IF:$<CONFIG:Release>,rm,true>")

    set(_COMMANDS "")

    if(WIN32)
        _stage_get(${TARGET} WINDOWS_DIR _WINDOWS_DIR)
        if(_WINDOWS_DIR)
            list(APPEND _COMMANDS
                COMMAND ${CMAKE_COMMAND} -E ${_MKDIR} "${_WINDOWS_DIR}/release"
                COMMAND ${CMAKE_COMMAND} -E ${_COPY}
                    "${_BINARY}" "${_WINDOWS_DIR}/release/${_BINARY_NAME}")
        endif()
    elseif(APPLE)
        _stage_get(${TARGET} MACOS_DIR _MACOS_DIR)
        get_target_property(_IS_BUNDLE ${TARGET} MACOSX_BUNDLE)
        if(_MACOS_DIR AND _IS_BUNDLE)
            # Replace the old bundle, so that removed resources don't linger in it
            list(APPEND _COMMANDS
                COMMAND ${CMAKE_COMMAND} -E ${_REMOVE} -rf
                    "${_MACOS_DIR}/$<TARGET_BUNDLE_DIR_NAME:${TARGET}>"
                COMMAND ${CMAKE_COMMAND} -E ${_MKDIR} "${_MACOS_DIR}"
                COMMAND ${CMAKE_COMMAND} -E ${_COPY_DIR} "$<TARGET_BUNDLE_DIR:${TARGET}>"
                    "${_MACOS_DIR}/$<TARGET_BUNDLE_DIR_NAME:${TARGET}>")
        elseif(_MACOS_DIR)
            list(APPEND _COMMANDS
                COMMAND ${CMAKE_COMMAND} -E ${_MKDIR} "${_MACOS_DIR}"
                COMMAND ${CMAKE_COMMAND} -E ${_COPY}
                    "${_BINARY}" "${_MACOS_DIR}/${_BINARY_NAME}")
        endif()
    elseif(UNIX)
        foreach(_KEY APPIMAGE_DIR DEB_DIR RPM_DIR)
            _stage_get(${TARGET} ${_KEY} _ROOT)
            if(_ROOT)
                list(APPEND _COMMANDS
                    COMMAND ${CMAKE_COMMAND} -E ${_MKDIR} "${_ROOT}/usr/bin"
                    COMMAND ${CMAKE_COMMAND} -E ${_COPY}
                        "${_BINARY}" "${_ROOT}/usr/bin/${_BINARY_NAME}")
            endif()
        endforeach()
    endif()

    if(_COMMANDS)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E ${_ECHO} "Staging '${TARGET}' binary into installer folders."
            ${_COMMANDS}
            VERBATIM)
    endif()
endfunction()

# ==========================================================================================
#  Internal helpers
# ==========================================================================================

# Reads a staging setting from a target, returning an empty string if unset
function(_stage_get TARGET KEY OUT_VAR)
    get_target_property(_VALUE ${TARGET} STAGE_${KEY})
    if(NOT _VALUE)
        set(_VALUE "")
    endif()
    set(${OUT_VAR} "${_VALUE}" PARENT_SCOPE)
endfunction()

# Fails if stage_setup() has not been called for the target
function(_stage_check_setup TARGET)
    if(NOT TARGET ${TARGET})
        message(FATAL_ERROR "'${TARGET}' is not a target.")
    endif()
    get_target_property(_CONFIGURED ${TARGET} STAGE_CONFIGURED)
    if(NOT _CONFIGURED)
        message(FATAL_ERROR "Call stage_setup(${TARGET} ...) before staging files for it.")
    endif()
endfunction()

# Resolves a relative source path against the caller's source folder
function(_stage_resolve_source SRC OUT_VAR OUT_HAS_GENEX)
    string(GENEX_STRIP "${SRC}" _STRIPPED)
    if(NOT _STRIPPED STREQUAL SRC)
        set(${OUT_VAR} "${SRC}" PARENT_SCOPE)
        set(${OUT_HAS_GENEX} TRUE PARENT_SCOPE)
        return()
    endif()

    set(_PATH "${SRC}")
    if(NOT IS_ABSOLUTE "${_PATH}")
        set(_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${_PATH}")
    endif()
    set(${OUT_VAR} "${_PATH}" PARENT_SCOPE)
    set(${OUT_HAS_GENEX} FALSE PARENT_SCOPE)
endfunction()

# Resource folder beside the binary (Contents/Resources inside a macOS bundle)
function(_stage_build_resource_dir TARGET OUT_VAR)
    get_target_property(_IS_BUNDLE ${TARGET} MACOSX_BUNDLE)
    if(APPLE AND _IS_BUNDLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(${OUT_VAR} "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET}>/Resources" PARENT_SCOPE)
    else()
        set(${OUT_VAR} "$<TARGET_FILE_DIR:${TARGET}>" PARENT_SCOPE)
    endif()
endfunction()

# All resource roots (build folder and staging areas), with an optional subfolder appended
function(_stage_resource_dirs TARGET DEST_REL OUT_VAR)
    _stage_build_resource_dir(${TARGET} _BUILD_DIR)
    _stage_get(${TARGET} APP_DATA_NAME _APP_DATA_NAME)
    _stage_get(${TARGET} WINDOWS_DIR _WINDOWS_DIR)
    _stage_get(${TARGET} APPIMAGE_DIR _APPIMAGE_DIR)
    _stage_get(${TARGET} DEB_DIR _DEB_DIR)
    _stage_get(${TARGET} RPM_DIR _RPM_DIR)

    set(_DIRS "${_BUILD_DIR}")
    if(_WINDOWS_DIR)
        list(APPEND _DIRS "${_WINDOWS_DIR}/resources")
    endif()
    if(_APPIMAGE_DIR)
        list(APPEND _DIRS "${_APPIMAGE_DIR}/usr/bin")
    endif()
    if(_DEB_DIR)
        list(APPEND _DIRS "${_DEB_DIR}/usr/share/${_APP_DATA_NAME}")
    endif()
    if(_RPM_DIR)
        list(APPEND _DIRS "${_RPM_DIR}/usr/share/${_APP_DATA_NAME}")
    endif()

    if(NOT DEST_REL STREQUAL "")
        list(TRANSFORM _DIRS APPEND "/${DEST_REL}")
    endif()
    set(${OUT_VAR} "${_DIRS}" PARENT_SCOPE)
endfunction()

# All translation folders for a language (build folder and staging areas).
# With PRIVATE, the DEB and RPM folders are under the app's data folder instead of the system one.
function(_stage_locale_dirs TARGET LANG PRIVATE OUT_VAR)
    _stage_build_resource_dir(${TARGET} _BUILD_DIR)
    _stage_get(${TARGET} APP_DATA_NAME _APP_DATA_NAME)
    _stage_get(${TARGET} WINDOWS_DIR _WINDOWS_DIR)
    _stage_get(${TARGET} APPIMAGE_DIR _APPIMAGE_DIR)
    _stage_get(${TARGET} DEB_DIR _DEB_DIR)
    _stage_get(${TARGET} RPM_DIR _RPM_DIR)

    set(_DIRS "${_BUILD_DIR}/${LANG}")
    if(_WINDOWS_DIR)
        list(APPEND _DIRS "${_WINDOWS_DIR}/resources/${LANG}")
    endif()
    if(_APPIMAGE_DIR)
        list(APPEND _DIRS "${_APPIMAGE_DIR}/usr/share/locale/${LANG}/LC_MESSAGES")
    endif()
    set(_PACKAGE_LOCALE_DIR "usr/share/locale")
    if(PRIVATE)
        set(_PACKAGE_LOCALE_DIR "usr/share/${_APP_DATA_NAME}/locale")
    endif()
    foreach(_ROOT IN ITEMS "${_DEB_DIR}" "${_RPM_DIR}")
        if(_ROOT)
            list(APPEND _DIRS "${_ROOT}/${_PACKAGE_LOCALE_DIR}/${LANG}/LC_MESSAGES")
        endif()
    endforeach()
    set(${OUT_VAR} "${_DIRS}" PARENT_SCOPE)
endfunction()

# Copies a file into each destination folder (as DEST_NAME), creating the folders first
function(_stage_copy_file TARGET SRC LABEL DEST_DIRS DEST_NAME)
    set(_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E echo "Staging ${LABEL} into build and installer folders.")
    foreach(_DIR IN LISTS DEST_DIRS)
        list(APPEND _COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC}" "${_DIR}/${DEST_NAME}")
    endforeach()

    add_custom_command(TARGET ${TARGET} POST_BUILD ${_COMMANDS} VERBATIM)
endfunction()

# Stages a translation catalog, compiling it from its .po file if there is no prebuilt .mo
function(_stage_catalog TARGET LANG SRC_MO SRC_PO BIN_MO DEST_NAME LABEL PRIVATE)
    if(EXISTS "${SRC_MO}")
        set(_USE_MO "${SRC_MO}")
    elseif(SRC_PO AND EXISTS "${SRC_PO}" AND GETTEXT_MSGFMT_EXECUTABLE)
        set(_USE_MO "${BIN_MO}")
        get_filename_component(_BIN_DIR "${BIN_MO}" DIRECTORY)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${SRC_PO} into ${BIN_MO}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_BIN_DIR}"
            COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" -o "${BIN_MO}" "${SRC_PO}"
            VERBATIM)
    elseif(SRC_PO AND EXISTS "${SRC_PO}")
        message(STATUS "gettext (msgfmt) not found. Cannot compile '${SRC_PO}'.")
        return()
    else()
        set(_EXPECTED "'${SRC_MO}'")
        if(SRC_PO)
            string(APPEND _EXPECTED " or '${SRC_PO}'")
        endif()
        message(STATUS
            "No ${LABEL} '${LANG}' translation found (expected ${_EXPECTED}). Skipping.")
        return()
    endif()

    _stage_locale_dirs(${TARGET} ${LANG} ${PRIVATE} _DEST_DIRS)
    _stage_copy_file(${TARGET} "${_USE_MO}" "${LABEL} '${LANG}' translations"
        "${_DEST_DIRS}" "${DEST_NAME}")
endfunction()
