#!/bin/bash
# check-opts.sh <ref.so> <core.so>: the same ROMs under other core options (PAL/Dendy, sound quality, low-pass, overclocking,
# no sprite limit, duty swap, channel mutes); every CRC must equal the reference core's.
export NESRUN_AUTOPLAY=1
H=$(cd "$(dirname "$0")/../.." && pwd); B=$1; M=$2; bad=0
for opt in "fceumm_region=PAL" "fceumm_region=Dendy" "fceumm_sndquality=High" "fceumm_sndquality=Very High" "fceumm_sndlowpass=enabled" \
           "fceumm_nospritelimit=enabled" "fceumm_overclocking=2x-Postrender" "fceumm_swapduty=enabled" "fceumm_apu_1=disabled"; do
  for r in $H/stressrom/out/stress-nes.nes $H/stressrom/out/irqtest-vrc3.nes $H/banana/roms/thwaite.nes; do
    a=$(~/banana-emu/bin/nesrun $B $r 3000 -v "$opt" | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' ')
    b=$(~/banana-emu/bin/nesrun $M $r 3000 -v "$opt" | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' ')
    [ "$a" = "$b" ] || { echo "DIFF $opt $(basename $r)"; bad=1; }
  done
done
[ $bad = 0 ] && echo "all option sets identical"; exit $bad
