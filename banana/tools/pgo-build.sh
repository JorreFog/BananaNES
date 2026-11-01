#!/bin/bash
# pgo-build.sh [board]: after pgo-train.sh brought the .gcda files into build/<board>-pgo, rebuild that tag with -fprofile-use.
# The source must be the one the -fprofile-generate build was made from (mismatching functions are just not optimised from the profile).
B=${1:-rgds}; H=$(cd "$(dirname "$0")/../.." && pwd); D=$H/build/$B-pgo
n=$(find $D -name '*.gcda' | wc -l); echo "$n gcda files"; [ "$n" -gt 0 ] || exit 1
find $D -name '*.o' -delete; rm -f $D/banananes_libretro.so
~/overnight-2026-11-01/buildslot.sh make -C $H/banana -j4 BOARD=$B BTAG=pgo \
  OPT="-O3 -flto=4 -fno-semantic-interposition -fprofile-use -fprofile-correction -Wno-missing-profile -Wno-coverage-mismatch" > /tmp/claude-1000/b_pgouse.log 2>&1
ls -la $D/banananes_libretro.so; grep -c "warning: .*profile" /tmp/claude-1000/b_pgouse.log
