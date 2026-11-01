#!/bin/bash
# fuzzrun.sh <candidate.so> [frames] [seeds] [mappers...]: base core vs candidate on the register-fuzz ROM for every mapper
# (default 0..255); prints DIFF lines and a summary. The base is ~/ovn-nes-base's upstream-exact host build.
B=${FUZZ_BASE:-$HOME/ovn-nes-base/build/host-base888/banananes_libretro.so}; C=$1; F=${2:-900}; S=${3:-1}; shift 3 2>/dev/null
M=${*:-$(seq 0 255)}; H=$(dirname "$(readlink -f "$0")"); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
sig() { ~/banana-emu/bin/nesrun "$1" "$2" "$F" 2>/dev/null | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' '; }
n=0; bad=0; skip=0; declare -A seen
for m in $M; do for s in $(seq 1 $S); do
  python3 "$H/fuzzgen.py" $m $s "$T/f.nes" 16 16 || continue
  a=$(sig "$B" "$T/f.nes"); [ -z "$a" ] && { skip=$((skip+1)); continue; }
  b=$(sig "$C" "$T/f.nes"); n=$((n+1)); seen[${a%% *}]=1
  [ "$a" = "$b" ] || { echo "DIFF mapper $m seed $s"; echo " base $a"; echo " cand $b"; bad=$((bad+1)); }
done; done
echo "FUZZ $n runs ($skip unsupported), ${#seen[@]} distinct pictures, $bad different"
