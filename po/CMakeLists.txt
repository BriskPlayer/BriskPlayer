# Internationalization setup for BriskPlayer using CMake

if(NOT ENABLE_NLS)
    return()
endif()

find_package(Gettext)

if(NOT GETTEXT_FOUND)
    message(STATUS "Gettext tools not found - skipping translation support")
    return()
endif()

# Languages currently supported
set(LANGUAGES de en)

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
        # Convert relative path to absolute
        if(NOT IS_ABSOLUTE "${line}")
            set(line "${CMAKE_SOURCE_DIR}/${line}")
        endif()
        if(EXISTS "${line}")
            list(APPEND TRANSLATABLE_SOURCES "${line}")
        endif()
    endif()
endforeach()

# Custom target to update the POT file
add_custom_target(update-pot
    COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE}
        --keyword=_
        --keyword=N_
        --keyword=T
        --keyword=TW
        --keyword=C_:1c,2
        --keyword=NC_:1c,2
        --from-code=UTF-8
        --add-comments=TRANSLATORS:
        --package-name=BriskPlayer
        --package-version=${PROJECT_VERSION}
        --msgid-bugs-address=zachbacon@proton.me
        --copyright-holder="Zach Bacon"
        --output=${POT_FILE}
        ${TRANSLATABLE_SOURCES}
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
        # Compile PO to MO
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${GETTEXT_MSGFMT_EXECUTABLE}
                --output-file=${MO_FILE}
                ${PO_FILE}
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
