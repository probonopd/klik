#!/bin/sh
KLIK_PATH=`cd \`dirname $0\` && cd .. && pwd`
PREFIX_PATH=`kde-config --prefix`

if [ "`whoami`" != "root" ]; then
    echo "You must be root in order to run $0"
    exit 1
fi

if [ ! -x "$1/bin/fusioniso" ]; then
    echo "syntax: $0 klik2_path"
    exit 1
fi

install ${KLIK_PATH}/binaryparts/lib/kde3/* ${PREFIX_PATH}/lib/kde3/
install -m 644 ${KLIK_PATH}/binaryparts/share/services/* ${PREFIX_PATH}/share/services/
for lang in es fr; do
  mkdir -p ${PREFIX_PATH}/share/locale/$lang/LC_MESSAGES
  install ${KLIK_PATH}/binaryparts/share/locale/$lang/LC_MESSAGES/kfile_cmg.mo.$lang ${PREFIX_PATH}/share/locale/$lang/LC_MESSAGES/kfile_cmg.mo
done
ldconfig -n ${PREFIX_PATH}/lib/kde3

for user in `ls -1 /home`; do
    test -L /home/$user && continue
    TEMP=`mktemp`
    cat > ${TEMP} << EOF
[Desktop Entry]
Comment=
Comment[fr]=
Encoding=UTF-8
EOF
    echo "Exec=$1/bin/install_kde_user.sh" >> ${TEMP}
    cat >> ${TEMP} << EOF
GenericName=To enable thumbnails and recipe informations
GenericName[fr]=Pour activer l'affichage des icônes et de l'information contextuelle sur les applications Klik2
Icon=filetypes
MimeType=
Name=Click here to enable KLIK2 integration
Name[fr]=Cliquez ici pour terminer l'intégration de Klik2 sous KDE
Path=
StartupNotify=true
Terminal=false
TerminalOptions=
Type=Application
X-DCOP-ServiceType=
X-KDE-SubstituteUID=false
X-KDE-Username=
X-Ubuntu-Gettext-Domain=desktop_kdebase
EOF
    DESKTOP_NAME=`su - $user -c "kde-config --userpath desktop"`
    test -d ${DESKTOP_NAME} || continue
    cp ${TEMP} "${DESKTOP_NAME}/To enable KLIK2 integration.desktop"
    group=`id --group --name $user`
    chown $user:$group "${DESKTOP_NAME}/To enable KLIK2 integration.desktop"
    chmod +x "${DESKTOP_NAME}/To enable KLIK2 integration.desktop"
done
exit 0
