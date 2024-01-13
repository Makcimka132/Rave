#!/usr/bin/sh

# Rave Dependencies Installer

SUDO=sudo
if [ -z "$(command -v sudo)" ]; then
    SUDO=doas
fi

if [ -f "/etc/arch-release" ]; then
    echo Arch Linux detected.
    $SUDO pacman -S llvm15 clang15
    echo Done.
else
    if [ -f "/etc/debian_version" ]; then
        echo Debian-based detected.
        $SUDO apt install llvm-15 llvm-15-dev clang-15 gcc-mingw-w64 binutils-mingw-w64
        echo Done.
    else
        if [ ! -z "$(command -v xbps-install)" ]; then
            echo Void Linux detected.
            $SUDO xbps-install -S clang llvm
            ./llvm-patch.sh
            echo Done.
        else
            echo Unsupported platform detected.
        fi
    fi
fi