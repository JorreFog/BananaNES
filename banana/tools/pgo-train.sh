#!/bin/bash
# pgo-train.sh <plus|rgds> <pgo-core.so> : (build the core with: make -C banana BOARD=rgds PGO_FIXED=1 NO_PGO=1 OPT="-O3 -flto=4 -fno-semantic-interposition -fprofile-generate -fprofile-update=single")
# PGO training run on the handheld (aarch64): runs the -fprofile-generate core
# on the stress ROM + homebrew through nesrun, brings the .gcda files back to where the objects were built.
# Run under locked.sh (a few minutes of one core, no game may run).  Then rebuild the same BTAG with -fprofile-use.
H=$(cd "$(dirname "$0")/../.." && pwd); D=$1; CORE=$2
dev() { ~/bananaos-0.4.3/dev.sh "$D" "$@"; }
dev 'rm -rf /run/pgotrain; mkdir -p /run/pgotrain/gcda'
put() { dev "cat > /run/pgotrain/$2 && chmod +x /run/pgotrain/$2" < "$1"; }
put ${NESRUN_A64:-$HOME/banana-emu/bin/nesrun-a64} nesrun; put "$CORE" core.so
for r in $H/stressrom/out/stress-nes.nes $(ls $H/banana/roms/*.nes); do put $r $(basename $r); done
for r in stress-nes.nes nova-the-squirrel.nes thwaite.nes 240p-test-suite.nes; do
  n=6000; [ $r = stress-nes.nes ] && n=10800
  dev "cd /run/pgotrain && GCOV_PREFIX=/run/pgotrain/gcda NESRUN_AUTOPLAY=1 ./nesrun ./core.so $r $n" | cut -c1-80
done
# the core options and mappers that the stress ROM does not reach (code the profile never saw is compiled cold: Very High sound
# ran 78 % slower in the first PGO build than without PGO)
put $H/stressrom/irqtest/irqtest.nes irqtest.nes
dev "cd /run/pgotrain && GCOV_PREFIX=/run/pgotrain/gcda NESRUN_AUTOPLAY=1 ./nesrun ./core.so irqtest.nes 3000" | cut -c1-80
for o in "fceumm_sndquality=High" "fceumm_sndquality=Very High" "fceumm_ntsc_filter=composite" "fceumm_nospritelimit=enabled" "fceumm_overclocking=2x-Postrender" "fceumm_region=PAL" "fceumm_region=Dendy" "fceumm_sndlowpass=enabled" "fceumm_overscan_h_left=8"; do
  dev "cd /run/pgotrain && GCOV_PREFIX=/run/pgotrain/gcda NESRUN_AUTOPLAY=1 ./nesrun ./core.so stress-nes.nes 2400 -v '$o'" | cut -c1-60
done
F=${FUZZ_TRAIN_MAPPERS:-"1 2 3 4 5 7 9 10 11 16 18 19 21 22 23 24 25 26 28 30 32 33 34 48 64 65 66 67 68 69 71 73 75 78 80 82 88 90 105 118 119 163 206 210 228"}
TD=$(mktemp -d)
for m in $F; do python3 $H/stressrom/fuzz/fuzzgen.py $m 1 $TD/f.nes 16 16 && put $TD/f.nes f.nes && dev "cd /run/pgotrain && GCOV_PREFIX=/run/pgotrain/gcda ./nesrun ./core.so f.nes 300" > /dev/null; done; rm -rf $TD
T=$(mktemp -d); dev 'cd /run/pgotrain/gcda && tar -cf - .' | tar -C $T -xf - 2>/dev/null
d=$(find $T -type d -path '*build/pgo' | head -1)
[ -n "$d" ] && { rm -rf "$H/banana/pgo"; mkdir -p "$H/banana/pgo"; cp -r "$d"/. "$H/banana/pgo/"; }
echo "gcda files: $(find $H/banana/pgo -name '*.gcda' | wc -l) (banana/pgo)"; rm -rf $T
dev 'rm -rf /run/pgotrain'
