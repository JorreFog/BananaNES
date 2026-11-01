#!/bin/bash
# check.sh <core.so> [make-ref]: exactness gate. Runs the stress ROMs (L1, L5, L10) and any ROMs in banana/roms/*.nes
# (freely licensed homebrew, not in git) through nesrun and compares video/audio/save-RAM/state CRCs with banana/ref-crc.txt
# (made once from the unmodified upstream build). A change that alters any CRC is not exact: opt-in only, or dropped.
export NESRUN_AUTOPLAY=1
H=$(cd "$(dirname "$0")/../.." && pwd); core=$1; mode=${2:-check}
roms="$H/stressrom/out/stress-nes-L1.nes $H/stressrom/out/stress-nes.nes $H/stressrom/out/stress-nes-L10.nes $(ls $H/banana/roms/*.nes 2>/dev/null)"
refs=$H/banana/ref-crc.txt; rc=0; [ "$mode" = make-ref ] && : > $refs.new
for r in $roms; do
  [ -f "$r" ] || continue
  n=10800; case $r in */roms/*) n=7200;; esac
  out=$(~/banana-emu/bin/nesrun "$core" "$r" $n | tr ' ' '\n' | grep -E "^(video_crc|audio_crc|srm_crc|state_crc)=" | tr '\n' ' ')
  key="$(basename $r) $(md5sum < $r | cut -c1-8) $n"
  if [ "$mode" = make-ref ]; then echo "$key $out" >> $refs.new
  else want=$(grep -F "$key " $refs | head -1 | cut -d' ' -f4-); if [ "$want" = "$out" ]; then echo "ok   $(basename $r)"; else echo "DIFF $(basename $r)"; echo "  want $want"; echo "  got  $out"; rc=1; fi; fi
done
[ "$mode" = make-ref ] && mv $refs.new $refs && cat $refs
exit $rc
