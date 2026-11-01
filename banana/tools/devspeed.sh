#!/bin/bash
# devspeed.sh <plus|rgds> <core.so>... : headless-free-ish CPU cost of each core on the stress ROM for core options (nesrun on the handheld, NESRUN_FAST=1 so
# the CRC passes don't count; clock pinned by PIN_KHZ if set). Prints host_s per core and option. Run under locked.sh.
D=$1; shift; H=$(cd "$(dirname "$0")/../.." && pwd)
dev() { ~/bananaos-0.4.3/dev.sh "$D" "$@"; }
dev 'rm -rf /run/nescheck; mkdir -p /run/nescheck'
put() { dev "cat > /run/nescheck/$2 && chmod +x /run/nescheck/$2" < "$1"; }
put ${NESRUN_A64:-$HOME/banana-emu/bin/nesrun-a64} nesrun; put $H/stressrom/out/stress-nes.nes stress-nes.nes
i=0; for c in "$@"; do put $c core$i.so; i=$((i+1)); done
[ -n "${PIN_KHZ:-}" ] && dev "for c in /sys/devices/system/cpu/cpu[0-3]/cpufreq; do echo \"\$c \$(cat \$c/scaling_min_freq) \$(cat \$c/scaling_max_freq)\"; done > /run/nescheck/pin.sav; for c in /sys/devices/system/cpu/cpu[0-3]/cpufreq; do echo $PIN_KHZ > \$c/scaling_min_freq; echo $PIN_KHZ > \$c/scaling_max_freq; done"
OPTS=${DEVSPEED_OPTS:-"fceumm_sndquality=Low|fceumm_sndquality=High|fceumm_sndquality=Very High|fceumm_ntsc_filter=composite|fceumm_nospritelimit=enabled|fceumm_overclocking=2x-Postrender"}
IFS="|"; for o in $OPTS; do IFS=" "
  i=0; for c in "$@"; do
    for rep in 1 2; do t=$(dev "cd /run/nescheck && NESRUN_FAST=1 ./nesrun ./core$i.so stress-nes.nes 3000 -v '$o'" | tr ' ' '\n' | grep -E "^host_s=" | cut -d= -f2); echo "$o core$i rep$rep host_s=$t"; done
    i=$((i+1)); done
done
[ -n "${PIN_KHZ:-}" ] && dev 'while read c mn mx; do echo 408000 > $c/scaling_min_freq; echo $mx > $c/scaling_max_freq; echo $mn > $c/scaling_min_freq; done < /run/nescheck/pin.sav' 
dev 'rm -rf /run/nescheck'
