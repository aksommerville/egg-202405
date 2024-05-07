# sfg Sound Format

There's a text format in your project's source, and a binary format that ships with the game.
I'm taking pains to keep them close to each other so the compiler can be as dumb as possible.

In the text format only, files may contain multiple resources with fences like so:
```
sound ID # 1..65535 or C identifier
  ...
end
```

Binary format begins with a fixed header:
```
u16  Duration, ms.
u8.8 Master.
```
Duration in text is inferred from the longest `level` command.
Master in text must appear before any other command, or defaults to 1:
```
master FLOAT
```

After the header, you provide any number of voices.

Binary:
```
u8 Features:
     0x01 shape
     0x02 harmonics
     0x04 fm
     0x08 fmenv
     0x10 rate
     0x20 ratelfo
     0xc0 reserved
(shape):
  u8 (0,1,2,3,4,5,6)=(sine,square,sawup,sawdown,triangle,noise,silence)
(harmonics):
  u8 count
  u0.8[] coefficients
(fm): FIXED(u8.8:rate, u8.8:scale)
(fmenv): ENV(u0.16)
(rate): ENV(u16 hz)
(ratelfo): FIXED(u8.8:hz, u16:cents)
... ops, distinguishable by their leading byte:
  0x01 level ENV(u0.16)
  0x02 gain FIXED(u8.8)
  0x03 clip FIXED(u0.8)
  0x04 delay FIXED(u16:ms, u0.8:dry, u0.8:wet, u0.8:sto, u0.8:fbk)
  0x05 bandpass FIXED(u16:mid hz, u16:width hz)
  0x06 notch FIXED(u16:mid hz, u16:width hz)
  0x07 lopass FIXED(u16:hz)
  0x08 hipass FIXED(u16:hz)
u8 0x00 End Of Voice
```

Text, the feature commands must appear in this order, before any positional commands:
```
shape sine|square|sawup|sawdown|triangle|noise|silence
harmonics FLOAT...
fm RATE SCALE
fmenv ENV(FLOAT)
rate ENV(INT hz)
ratelfo HZ CENTS
```

Then ops may appear in any order:
```
level ENV(FLOAT)
gain MLT
clip LEVEL
delay PERIOD DRY WET STO FBK
bandpass MIDHZ WIDTHHZ
notch MIDHZ WIDTHHZ
lopass HZ
hipass HZ
```

Finally, all but the last voice must be explicitly terminated:
```
endvoice
```

ENV is `VALUE [MS VALUE...]`

## Notes

Global Header:
  duration u16:ms # If voices run longer they get cut off cold.
  master: u8.8    # Applied after mixing voices.
  
Voice:
  Fixed Order:
    shape u8:enum                                 # Sine if omitted.
    harmonics u8:count u0.8[]:coef                # Fundamental is the shape. [1] if omitted.
    fm u8.8:rate u8.8:scale                       # Carrier comes from (shape,harmonics).
    fmenv u0.16:init u8:count (u16:delay u0.16:v) # Peak is the scale from 'fm' command.
    rate u16:hz u8:count (u16:delay u16:hz)       # Constant 440 if omitted.
    ratelfo u8.8:hz u16:cents
  Positional:
    level u0.16:v u8:count (u16:delay u0.16:v)    # Required at least once.
    gain u8.8
    clip u0.8
    delay u16:ms u0.8:dry u0.8:wet u0.8:sto u0.8:fbk
    bandpass u16:center u16:width
    notch u16:center u16:width
    lopass u16:hz
    hipass u16:hz
    
No fmlfo. It's complicated, and you might as well simulate it with fmenv if desired.
Fixed Order applies to both text and binary.

Binary voice begins with feature selection byte:
  0x01 shape
  0x02 harmonics
  0x04 fm
  0x08 fmenv
  0x10 rate
  0x20 ratelfo
  0xc0 reserved
  - Decoder must fail if a reserved bit is set.
  - Otherwise all bits are legal in all cases, even if they won't be needed.
Followed by a headerless block for each of those features, in that order.
Followed by positional ops until EOV.

All primitive command types:
  - enum: u8
  - - shape: sine,square,sawup,sawdown,triangle,noise,silence
  - - op: EOV,level,gain,clip,delay,bandpass,notch,lopass,hipass
  - normarray: u8:count u0.8[]:v
  - - harmonics
  - env: i16:init u8:count (u16:ms i16:v)
  - - fmenv: u0.16
  - - rate: u16
  - - level: u0.16
  - scalars
  - - fm: u8.8 u8.8
  - - ratelfo: u8.8 u16
  - - gain: u8.8
  - - clip: u0.8
  - - delay: u16 u0.8 u0.8 u0.8 u0.8
  - - bandpass,notch: u16 u16
  - - lopass,hipass: u16
