#!/bin/bash
# devcheck.sh <plus|rgds> <refcore.so> <core.so>...: exactness on the real CPU. Pushes nesrun (aarch64) + the ROMs + cores to
# /run/nescheck (tmpfs), runs each core on the stress ROMs (and homebrew in banana/roms) and compares the CRCs with the
# reference core's. Run under locked.sh (a few seconds of one core; no game may run).
H=$(cd "$(dirname "$0")/../.." && pwd); D=$1; shift
export NESRUN_AUTOPLAY=1
dev() { ~/bananaos-0.4.3/dev.sh "$D" "$@"; }
dev 'rm -rf /run/nescheck; mkdir -p /run/nescheck'
put() { dev "cat > /run/nescheck/$2 && chmod +x /run/nescheck/$2" < "$1"; }
put ${NESRUN_A64:-$HOME/banana-emu/bin/nesrun-a64} nesrun
roms="$H/stressrom/out/stress-nes.nes $H/stressrom/out/irqtest-vrc3.nes $([ -z "$DEVCHECK_QUICK" ] && ls $H/banana/roms/*.nes 2>/dev/null)"
for r in $roms; do put $r $(basename $r); done
i=0; for c in "$@"; do put $c core$i.so; i=$((i+1)); done
for r in $roms; do
  n=14000; case $r in */roms/*|*/irqtest*) n=3000;; esac
  b=$(basename $r); i=0; ref=
  for c in "$@"; do
    out=$(dev "cd /run/nescheck && NESRUN_AUTOPLAY=1 ./nesrun ./core$i.so $b $n" | tr ' ' '\n' | grep -E "^(video_crc|audio_crc|srm_crc|state_crc)=" | tr '\n' ' ')
    [ $i = 0 ] && ref=$out
    [ "$out" = "$ref" ] && echo "same $b core$i ($(basename $(dirname $c)))" || { echo "DIFF $b core$i ($(basename $(dirname $c)))"; echo "  ref $ref"; echo "  got $out"; }
    i=$((i+1))
  done
done
# options that take other code paths (NEON blit fallback for a width that is no multiple of 16, palette refresh, NTSC filter)
for o in "fceumm_overscan_h_left=8" "fceumm_palette=pal" "fceumm_ntsc_filter=composite"; do
  i=0; ref=
  for c in "$@"; do
    out=$(dev "cd /run/nescheck && ./nesrun ./core$i.so stress-nes.nes 1500 -v '$o'" | tr ' ' '\n' | grep -E "^(video_crc|audio_crc|state_crc)=" | tr '\n' ' ')
    [ $i = 0 ] && ref=$out
    [ "$out" = "$ref" ] && echo "same opt $o core$i" || { echo "DIFF opt $o core$i"; echo "  ref $ref"; echo "  got $out"; }
    i=$((i+1))
  done
done
dev 'rm -rf /run/nescheck'
