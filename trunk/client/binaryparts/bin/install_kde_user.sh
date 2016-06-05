#!/bin/sh
PREFIX_PATH=`kde-config --userpath desktop`
DESKTOP_NAME=`basename ${PREFIX_PATH}`
PIDOF_BIN="/bin/pidof"
test -x /sbin/pidof && PIDOF_BIN="/sbin/pidof"
${PIDOF_BIN} kdesktop || exit 0
kwriteconfig --group "PreviewSettings" --key MaximumSize 101816730
kwriteconfig --group "PreviewSettings" --key UseFileThumbnails true
LIST=`kreadconfig --group "Desktop Icons" --key "Preview" --file kdesktoprc`
if [ $LIST == "" ]; then
  kwriteconfig --group "Desktop Icons" --key "Preview" --file kdesktoprc isofsthumbnail
else
  (echo $LIST | grep isofsthumbnail) || kwriteconfig --group "Desktop Icons" --key "Preview" --file kdesktoprc "$LIST,isofsthumbnail"
fi
dcop kdesktop MainApplication-Interface quit
while [ ! -z `${PIDOF_BIN} kdesktop` ]; do
    sleep 1
done
kdesktop
kdialog --msgbox "Thanks for using Klik2 !"
rm -f "$HOME/${DESKTOP_NAME}/To enable KLIK2 integration.desktop"
exit 0
