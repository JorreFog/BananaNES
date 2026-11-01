#!/bin/bash
# check-tests.sh <ref.so> <core.so> [frames]: runs the community accuracy test ROMs (blargg, Quietust, ... from
# github.com/christopherpow/nes-test-roms, cloned to ~/dl-nes-tests/src, NOT in this repo) on both cores and compares the CRCs:
# every CPU/PPU/APU/mapper timing test must come out exactly as with the upstream core (pass or fail alike).
export NESRUN_AUTOPLAY=
unset NESRUN_AUTOPLAY
T=${TESTS:-$HOME/dl-nes-tests/src}; B=$1; M=$2; N=${3:-1800}; bad=0; n=0
for d in apu_test blargg_apu_2005.07.30 blargg_nes_cpu_test5 cpu_dummy_reads cpu_dummy_writes cpu_interrupts_v2 cpu_timing_test6 dmc_dma_during_read4 \
         dmc_tests instr_misc instr_test-v5 instr_timing mmc3_irq_tests mmc3_test mmc3_test_2 mmc5test_v2 nmi_sync oam_read oam_stress ppu_open_bus \
         ppu_read_buffer ppu_vbl_nmi sprdma_and_dmc_dma sprite_hit_tests_2005.10.05 sprite_overflow_tests scanline branch_timing_tests apu_reset cpu_reset full_palette; do
  for r in $(find $T/$d -name '*.nes' | sort); do
    a=$(~/banana-emu/bin/nesrun $B $r $N 2>/dev/null | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' ')
    b=$(~/banana-emu/bin/nesrun $M $r $N 2>/dev/null | tr ' ' '\n' | grep -E "^(video|audio|state|srm)_crc" | tr '\n' ' ')
    n=$((n+1)); [ -n "$a" ] && [ "$a" = "$b" ] || { echo "DIFF ${r#$T/}"; bad=$((bad+1)); }
  done
done
echo "$n test ROMs, $bad different"; exit $((bad>0))
