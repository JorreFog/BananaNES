#!/bin/bash
# pgo-train.sh <plus|rgds> <pgo-core.so> : PGO training run on the handheld (aarch64): runs the -fprofile-generate core
# on the stress ROM + homebrew through nesrun, brings the .gcda files back to where the objects were built.
# Run under locked.sh (a few minutes of one core, no game may run).  Then rebuild the same BTAG with -fprofile-use.
H=$(cd "$(dirname "$0")/../.." && pwd); D=$1; CORE=$2
dev() { ~/bananaos-0.4.3/dev.sh "$D" "$@"; }
dev 'rm -rf /run/pgotrain; mkdir -p /run/pgotrain/gcda'
put() { dev "cat > /run/pgotrain/$2 && chmod +x /run/pgotrain/$2" < "$1"; }
put $H/build/nesrun-a64 nesrun; put "$CORE" core.so
for r in $H/stressrom/out/stress-nes.nes $(ls $H/banana/roms/*.nes); do put $r $(basename $r); done
for r in stress-nes.nes nova-the-squirrel.nes thwaite.nes 240p-test-suite.nes; do
  n=6000; [ $r = stress-nes.nes ] && n=10800
  dev "cd /run/pgotrain && GCOV_PREFIX=/run/pgotrain/gcda NESRUN_AUTOPLAY=1 ./nesrun ./core.so $r $n" | cut -c1-80
done
dev 'cd /run/pgotrain/gcda && tar -cf - .' | tar -C / -xf - && echo "gcda files: $(find /home -newer $CORE -name '*.gcda' 2>/dev/null | wc -l)"
dev 'rm -rf /run/pgotrain'
