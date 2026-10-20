# Internationalization setup for BriskPlayer using CMake

if(NOT ENABLE_NLS)
    return()
endif()

find_package(Gettext)

if(NOT GETTEXT_FOUND)
    message(STATUS "Gettext tools not found - skipping translation support")
    return()
endif()

# FindGettext only locates msgmerge/msgfmt; update-pot needs xgettext too.
# Same search path as those (the vcpkg host-tools directory added to
# CMAKE_PROGRAM_PATH by the top-level CMakeLists.txt).
get_filename_component(_gettext_bin_dir "${GETTEXT_MSGFMT_EXECUTABLE}" DIRECTORY)
find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext HINTS "${_gettext_bin_dir}")

# Languages currently supported
set(LANGUAGES ar bg cs da de el es fi fr he hu id it ja ko nb nl pl pt pt_BR ro ru sk sv tr uk vi zh_CN zh_TW)

# Gettext domain
set(GETTEXT_DOMAIN "briskplayer")

# POT file (translation template)
set(POT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${GETTEXT_DOMAIN}.pot")

# Read source files from POTFILES.in
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/POTFILES.in" POTFILES_CONTENT)
set(TRANSLATABLE_SOURCES)
foreach(line ${POTFILES_CONTENT})
    string(STRIP "${line}" line)
    # Skip empty lines and comments
    if(line AND NOT line MATCHES "^#")
        # Kept relative to the source root (update-pot runs there) so the
        # .pot's "#: file:line" references don't embed the local checkout path
        if(EXISTS "${CMAKE_SOURCE_DIR}/${line}")
            list(APPEND TRANSLATABLE_SOURCES "${line}")
        else()
            message(WARNING "po/POTFILES.in lists missing file: ${line}")
        endif()
    endif()
endforeach()

# Dialog text lives in briskplayer.rc, which xgettext can't parse; it is
# extracted into a generated C file first (see ExtractRcStrings.cmake).
set(RC_STRINGS_FILE "briskplayer_rc.c")

# Custom target to update the POT file
add_custom_target(update-pot
    COMMAND ${CMAKE_COMMAND}
        -DRC_FILE=${CMAKE_SOURCE_DIR}/briskplayer.rc
        -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${RC_STRINGS_FILE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/ExtractRcStrings.cmake
    COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE}
        # Inputs are named relative to these, keeping "#: file:line"
        # references free of local paths
        --directory=${CMAKE_SOURCE_DIR}
        --directory=${CMAKE_CURRENT_BINARY_DIR}
        --keyword=_
        --keyword=N_
        --keyword=T
        --keyword=TW
        # C_/NC_ take a context argument but look up by string alone
        # (see CPI_Gettext.h), so extract without msgctxt to match.
        --keyword=C_:2
        --keyword=NC_:2
        --from-code=UTF-8
        --add-comments=TRANSLATORS:
        --package-name=BriskPlayer
        --package-version=${PROJECT_VERSION}
        --msgid-bugs-address=zachbacon@proton.me
        "--copyright-holder=Zach Bacon"
        --output=${POT_FILE}
        ${TRANSLATABLE_SOURCES}
        ${RC_STRINGS_FILE}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Extracting translatable strings to ${GETTEXT_DOMAIN}.pot"
    VERBATIM
)

# Process each language
foreach(lang ${LANGUAGES})
    set(PO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po")
    set(MO_FILE "${CMAKE_CURRENT_BINARY_DIR}/${lang}.mo")
    
    # Check if PO file exists
    if(EXISTS ${PO_FILE})
        # Compile PO to MO and deploy to the runtime locale directory
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
                --output-file=${MO_FILE}
                ${PO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:briskplayer>/locale/${lang}/LC_MESSAGES"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MO_FILE}"
                "$<TARGET_FILE_DIR:briskplayer>/locale/${lang}/LC_MESSAGES/${GETTEXT_DOMAIN}.mo"
            DEPENDS ${PO_FILE}
            COMMENT "Compiling ${lang} translation"
            VERBATIM
        )
        
        # Install MO file
        install(FILES ${MO_FILE}
            DESTINATION share/locale/${lang}/LC_MESSAGES
            RENAME ${GETTEXT_DOMAIN}.mo
        )
        
        # Add to all target
        list(APPEND MO_FILES ${MO_FILE})
        
        # Custom target to update PO from POT
        add_custom_target(update-po-${lang}
            COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE}
                --update
                --backup=none
                ${PO_FILE}
                ${POT_FILE}
            DEPENDS update-pot
            COMMENT "Updating ${lang}.po from template"
            VERBATIM
        )
    endif()
endforeach()

# Target to build all translations
if(MO_FILES)
    add_custom_target(translations ALL DEPENDS ${MO_FILES})
endif()

# Target to update all PO files
add_custom_target(update-po)
foreach(lang ${LANGUAGES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po")
        add_dependencies(update-po update-po-${lang})
    endif()
endforeach()

message(STATUS "Translation support configured for languages: ${LANGUAGES}")
