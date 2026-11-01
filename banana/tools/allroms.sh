#!/bin/bash
# usage: allroms2.sh candidate.so [frames]
B=${ALLROMS_BASE:-$HOME/ovn-nes-base/build/host-base889/banananes_libretro.so}; M=$1; F=${2:-1200}; n=0; bad=0; empty=0
sig() { ~/banana-emu/bin/nesrun "$1" "$2" "$F" 2>/dev/null | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' '; }
while IFS= read -r -d '' r; do
  a=$(sig "$B" "$r"); b=$(sig "$M" "$r"); n=$((n+1))
  [ -z "$a" ] && { empty=$((empty+1)); echo "EMPTY $r"; }
  [ "$a" = "$b" ] || { echo "DIFF $r"; bad=$((bad+1)); }
done < <(find "$HOME/dl-nes-tests/src" -iname '*.nes' -print0 | sort -z)
echo "ALLROMS2 $n roms, $bad different, $empty without output"
