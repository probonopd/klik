#!/bin/sh
DIRNAME=`cd \`dirname $0\` && cd .. && pwd`

mkdir -p ~/.kde/share/mimelnk/all
cat > ~/.kde/share/mimelnk/all/cmg.desktop <<\EOF
[Desktop Entry]
Comment=
Hidden=false
Icon=view_remove
MimeType=application/x-extension-cmg
Patterns=*.cmg
Type=MimeType
EOF

mkdir -p ~/.kde/share/applnk/.hidden
cat > ~/.kde/share/applnk/.hidden/AppRun.desktop << EOF
[Desktop Entry]
Comment=
Exec=$DIRNAME/bin/klik-run %U
Icon=view_remove
InitialPreference=2
MimeType=application/x-extension-cmg;Application
Name=
ServiceTypes=
Terminal=false
Type=Application
EOF
