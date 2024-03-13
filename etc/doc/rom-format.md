# Egg ROM Format

Binary file, integers are big-endian.

Starts with Header, minimum 16 bytes:

```
  0000   4 Signature: "\x00\x0egg" (0x00,0x00,0x67,0x67)
  0004   4 Header length (>=16)
  0008   4 TOC length
  000c   4 Heap length
  0010
```
  
We may define more header fields in the future.
If you see a header length longer than 16 it is safe to ignore the excess.

TOC begins immediately after Header, and Heap immediately after TOC.
You must read TOC and Heap at the same time, with a state machine:

```
int tocp = 0
int heapp = 0
int tid = 1
int qual = 0
int rid = 1
```

Read from TOC. One byte tells you the next command:

```
00dddddd                   : NEXTTYPE : tid+=d+1, qual=0, rid=1
01000000 dddddddd dddddddd : NEXTQUAL : qual+=d+1, rid=1
10llllll                   : SHORT    : Add resource, length (l). rid+=1, heapp+=l
1100llll llllllll llllllll : LONG     : Add resource, length (l). rid+=1, heapp+=l
1101xxxx                   : RESERVED
1110xxxx                   : RESERVED
1111xxxx                   : RESERVED
01xxxxxx (for nonzero x)   : RESERVED
```

Decoder must check for overflow carefully: (tid) must not exceed 0xff, (qual) 0xffff, or (rid) 0xffff.
It is legal for (rid) to cross 0xffff if the next command is NEXTTYPE or NEXTQUAL, or at the end of the TOC.
(if that were not so, it would be impossible to have rid 65535).

A consequence of this format is that the resources must be sorted (tid,qual,rid).

Zero-length resources are legal but at runtime are not distinguishable from missing ones.

It is helpful to keep your resource IDs packed as much as possible; you have to spend 1 byte to skip each unused rid.

Every resource is unique identified by a 40-bit integer: 8 tid, 16 qual, 16 rid.
`qual` is usually zero. Otherwise it depends on the type, but will usually be a big-endian ISO 631 language code.
Length of individual resources is limited to 1 MB.

## Resource Types

| tid  | name      | qual      | comment |
|------|-----------|-----------|---------|
| 0x01 | metadata  | ---       | General data about the game, for indexing tools. metadata:1 must be the first resource, if present. |
| 0x02 | wasm      | ---       | Executable WebAssembly module. wasm:1 should contain main, if present. |
| 0x03 | js        | ---       | Executable Javascript module. js:1 should contain main, if present. |
| 0x04 | image     | lang      | Image, in any of several formats. (qual) is usually zero but can be used for pictures of text. |
| 0x05 | string    | lang      | Loose text. string:1 should be the language's own name. |
| 0x06 | song      | ---       | Sourced from MIDI but encoded here in our own format. |
| 0x07 | sound     | lang      | PCM or synth data for one sound effect. |
| 0x08 | map       | ---       | Map data in our standard format. |
| 0x09..0x1f | reserved | ---  | May be standardized in the future. |
| 0x20..0x7f | custom | ---    | Yours to define. |
| 0x80..0xff | reserved | ---  | May be standardized in the future. |
