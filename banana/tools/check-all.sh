#!/bin/bash
# check-all.sh <core.so> [ref.so]: every PC exactness check in one go (about 6 minutes): own + homebrew ROMs, option sets,
# 136 community test ROMs (needs ~/dl-nes-tests), savestate cross-load, 60000-frame soak, scripted-pad seeds.
# ref.so defaults to the unmodified upstream build (build via a worktree of the first commit; see banana/NOTES.md).
H=$(cd "$(dirname "$0")/../.." && pwd); M=$1; B=${2:-$HOME/ovn-nes-base/build/host-base888/banananes_libretro.so}; rc=0
echo "== ROMs";    "$H/banana/tools/check.sh" "$M" || rc=1
echo "== options"; "$H/banana/tools/check-opts.sh" "$B" "$M" || rc=1
echo "== community tests"; "$H/banana/tools/check-tests.sh" "$B" "$M" 1800 | tail -3 || rc=1
echo "== savestate cross-load"
R=$H/stressrom/out/stress-nes.nes
~/banana-emu/bin/nesrun "$B" $R 3000 -w /tmp/banananes.state:2000 >/dev/null
a=$(~/banana-emu/bin/nesrun "$B" $R 3000 -l /tmp/banananes.state:2000 2>/dev/null | grep -o "video_crc=[0-9a-f]*")
b=$(~/banana-emu/bin/nesrun "$M" $R 3000 -l /tmp/banananes.state:2000 2>/dev/null | grep -o "video_crc=[0-9a-f]*")
[ "$a" = "$b" ] && echo "same ($a)" || { echo "DIFF $a $b"; rc=1; }
echo "== soak + random pads"
for seed in 3 77 4242; do for r in $H/banana/roms/nova-the-squirrel.nes $H/banana/roms/thwaite.nes; do
  x=$(NESRUN_AUTOPLAY=1 NESRUN_SEED=$seed ~/banana-emu/bin/nesrun "$B" $r 5000 | tr ' ' '\n' | grep -E "^(video|audio|state)_crc" | tr '\n' ' ')
  y=$(NESRUN_AUTOPLAY=1 NESRUN_SEED=$seed ~/banana-emu/bin/nesrun "$M" $r 5000 | tr ' ' '\n' | grep -E "^(video|audio|state)_crc" | tr '\n' ' ')
  [ "$x" = "$y" ] || { echo "DIFF seed $seed $(basename $r)"; rc=1; }
done; done
x=$(~/banana-emu/bin/nesrun "$B" $R 60000 | tr ' ' '\n' | grep -E "^(video|audio|state)_crc" | tr '\n' ' ')
y=$(~/banana-emu/bin/nesrun "$M" $R 60000 | tr ' ' '\n' | grep -E "^(video|audio|state)_crc" | tr '\n' ' ')
[ "$x" = "$y" ] && echo "same (60000 frames)" || { echo "DIFF soak"; rc=1; }
[ $rc = 0 ] && echo "ALL EXACT" || echo "SOMETHING DIFFERS"; exit $rc
