# Egg Song Format

Our synthesizer is designed with MIDI in mind but doesn't match exactly.
So our song format, same deal. Helps if you understand MIDI first.

Key differences from MIDI:
 - Notes contain their release time in the press event.
 - No aftertouch events.
 - No control or program changes.
 - Single track.
 - Timing is in milliseconds everywhere, no global tempo or division.
 - 8 addressable channels instead of 16.
 - There's a separate global channel header. No initialization events in the song body.
 
## Format

Begins with a fixed header:

```
0000   4 Signature: "\xbe\xee\xeeP"
0004   2 Start position.
0006   2 Repeat position.
0008  32 Channel initializers. 8 of:
           0000   1 Program ID. First 128 are General MIDI.
           0001   1 Volume.
           0002   1 Pan. 0..128..255 = left..center..right
           0003   1 reserved
0028
```

Start Position and Repeat Position are in bytes from the start of the file.
Must be >=40.

Followed by events until end of file.
Events are identifiable by their first byte.

```
00000000                   EOF. Stop reading. Return to the Repeat Position or stop.
0ttttttt                   DELAY. (t) nonzero ms.
1000vvvv cccnnnnn nntttttt NOTE. duration=(t<<5)ms (~2s max)
1001vvcc cnnnnnnn          FIREFORGET. Same as NOTE but duration zero (and coarser velocity).
10100ccc wwwwwwww          WHEEL. 8 bits unsigned. 0x40 by default.
10101xxx                   RESERVED
1011xxxx                   RESERVED
11xxxxxx                   RESERVED
```
