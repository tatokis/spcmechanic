#!/usr/bin/bash
if [[ ! -f "dist/0.0" ]]; then
    gunzip -k dist/0.0.gz
fi
mkisofs -f -V TEST -sysid PLAYSTATION -xa2 -o SPCMechanic.iso dist
