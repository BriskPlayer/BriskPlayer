# Internationalization setup for BriskPlayer
# This file configures gettext for translating the application

# Languages currently supported or planned
languages = ['de', 'en']

# Configure gettext domain
gettext_domain = 'briskplayer'

# Set up i18n using meson's built-in support
# Generate POT file and handle PO files
i18n.gettext(gettext_domain,
    preset: 'glib',
    data_dirs: ['../res'],
    args: [
        '--keyword=_',
        '--keyword=N_',
        '--keyword=C_:1c,2', 
        '--keyword=NC_:1c,2',
        '--from-code=UTF-8',
        '--add-comments=TRANSLATORS:',
        '--package-name=BriskPlayer',
        '--package-version=' + meson.project_version(),
        '--msgid-bugs-address=zachbacon@outlook.com',
        '--copyright-holder=Zach Bacon',
    ]
)