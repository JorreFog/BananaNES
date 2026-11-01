; fuzz: a register fuzzer for BananaNES' exactness checks (GPL-3.0-or-later). The same 8 KiB image (code + a per-bank random
; table) sits in every 8 KiB PRG bank, so it runs under any mapper whatever the banking scheme. A seeded LFSR picks, all
; frame long, a mapper/APU/expansion register or PRG-RAM address and a value and writes it, reads random addresses into an
; accumulator that colours the picture, and moves the scroll / emphasis mid-frame. Two cores are the same if the picture,
; the sound and the save state agree after N frames. It is not a game and plays nothing sensible; it reaches the mapper
; IRQ, bank-switch, expansion-audio, DMC-DMA and open-bus paths that the stress ROM (MMC3) and the homebrew do not.
.p02
.ifndef SEED
SEED = $1234
.endif
.segment "ZEROPAGE"
lo: .res 1
hi: .res 1
acc: .res 1
ptr: .res 2
fr: .res 1
tmp: .res 1
cnt: .res 1
.segment "CODE"
rnd:                    ; A = next random byte (8 LFSR steps)
  lda #8
  sta cnt
@l:
  lsr hi
  ror lo
  bcc @n
  lda hi
  eor #$B4
  sta hi
@n:
  dec cnt
  bne @l
  lda lo
  rts
reset:
  sei
  cld
  ldx #$FF
  txs
  inx
  stx $2000
  stx $2001
  stx $4015
  lda #$40
  sta $4017
: bit $2002
  bpl :-
  ldx #0
  txa
: sta $00,x
  sta $0100,x
  sta $0300,x
  sta $0400,x
  sta $0500,x
  sta $0600,x
  sta $0700,x
  inx
  bne :-
: bit $2002
  bpl :-
  lda #<SEED
  sta lo
  lda #>SEED
  ora #$01
  sta hi
  ; palettes
  lda #$3F
  sta $2006
  lda #0
  sta $2006
  ldx #32
: jsr rnd
  sta $2007
  dex
  bne :-
  ; four nametables of random tiles + attributes
  lda #$20
  sta $2006
  lda #0
  sta $2006
  ldy #16
: ldx #0
: jsr rnd
  sta $2007
  inx
  bne :-
  dey
  bne :--
  ; OAM page
  ldx #0
: jsr rnd
  sta $0200,x
  inx
  bne :-
  lda #$88
  sta $2000
  lda #$1E
  sta $2001
  cli
main:
  jsr rnd
  and #15
  tay
  jsr rnd
  sta ptr
  jsr rnd
  sta ptr+1
  jsr rnd
  sta tmp
  cpy #6
  bcs @o6
  ; mapper window $8000-$FFFF
  lda ptr+1
  ora #$80
  sta ptr+1
  jmp @wr
@o6:
  cpy #7
  bne @o7
  lda ptr+1
  and #$1F
  ora #$40
  sta ptr+1
  cmp #$40
  beq @fl
  jmp @wr
@fl:
  lda ptr
  ora #$20
  sta ptr
  jmp @wr
@o7:
  cpy #8
  bne @o8
  lda ptr+1
  and #$1F
  ora #$60
  sta ptr+1
  jmp @wr
@o8:
  cpy #9
  bne @o9
  lda #$40
  sta ptr+1
  lda ptr
  and #$1F
  sta ptr
  jmp @wr
@o9:
  cpy #12
  bcs @o12
  ; read a random address >= $4020 into acc
  lda ptr+1
  ora #$40
  sta ptr+1
  cmp #$40
  bne @rd
  lda ptr
  ora #$20
  sta ptr
@rd:
  ldy #0
  lda (ptr),y
  eor acc
  sta acc
  jmp main
@o12:
  cpy #13
  bne @o13
  lda acc
  sta $2005
  lda tmp
  sta $2005
  jmp main
@o13:
  cpy #14
  bne @o14
  lda tmp
  and #$E0
  ora #$1E
  sta $2001
  jmp main
@o14:
  cpy #15
  bne @o15
  ldx tmp
  inx
: dex
  bne :-
  jmp main
@o15:
  bit $2002
  lda tmp
  and #$3F
  tax
  lda acc
  sta $0200,x
  jmp main
@wr:
  ldy #0
  lda tmp
  sta (ptr),y
  jmp main
nmi:
  pha
  txa
  pha
  inc fr
  lda #$3F
  sta $2006
  lda #0
  sta $2006
  lda acc
  sta $2007
  eor #$2A
  sta $2007
  lda #0
  sta $2003
  lda #2
  sta $4014
  lda #$88
  sta $2000
  lda #0
  sta $2005
  sta $2005
  pla
  tax
  pla
irq:
  rti
.segment "VECTORS"
  .word nmi, reset, irq
