[ -f /etc/bash.bashrc ] && . /etc/bash.bashrc
#[ -f ~/.bashrc ] && . ~/.bashrc
alias test='${KLIK_BIN_PATH}/test'
alias ldd='${KLIK_BIN_PATH}/ldd'
OLD_PS1="${PS1}"
export PS1="[klik2 shell] ${OLD_PS1}"
cd
