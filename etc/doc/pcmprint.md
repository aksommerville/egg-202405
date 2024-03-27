# pcmprint

System for printing short sound effects.

There's a text format only used at build time (src/tool/eggrom/eggrom_tid_sound.c),
and a binary format used at run time (src/opt/pcmprint/).

In the text format, you must begin each sound with `sound ID` and end with `end`, on their own lines.
Binary format is just one sound.

Binary begins with a fixed header:

```
0000   2 Duration, ms.
0002
```

Text format may begin with `master FLOAT`, 1.0 if omitted.
That doesn't show up in the binary format. Instead, the compiler inserts a gain at the end of each voice.

### Voice

```
text: voice
bin: 01
```

Commands before and after are processed independently of each other, and mixed at the end.
If this appears at the beginning or end, or adjacent to each other, no effect.

### Wave

```
text: wave sine|square|sawup|sawdown|triangle|noise|silence
text: wave COEFFICIENTS...
text: wave fm=RATE,RANGE
bin: 02 u8:SHAPE ; (0,1,2,3,4)=(sine,square,sawup,sawdown,triangle)
bin: 03 ; noise
bin: 04 u8:COUNT u8:COEF...
bin: 05 u8.8:RATE u8.8:RANGE
```

Exactly one of these must appear exactly once in each voice.
In the binary format, it must be the first command.
The "silence" command renders a voice inert, and it will not be encoded but does still affect total length.

### Rate

```
text: rate HZ [MS HZ|@MS HZ...] ; @ for an absolute time, which must be in the future.
bin: 06 u16:HZ u8:COUNT (u16:MS u16:HZ)...
```

Should appear exactly once per voice. Not useful for noise.
Binary format, must immediately follow wave if present.

### Rate LFO

```
text: ratelfo HZ CENTS
bin: 07 u8.8:HZ u16:CENTS
```

Optional low-frequency oscillator against main rate.

### FM Range

```
text: fmrange V [MS V|@MS V...]
bin: 08 u0.16:V u8:COUNT (u16:MS u0.16:V)...
```

Optional envelope for FM range, multiplied against the nominal range.
Meaningless if you specified wave as something other than FM.

### FM Range LFO

```
text: fmlfo HZ MLT
bin: 09 u8.8:HZ u8.8:MLT
```

Optional LFO against FM rate, applied after envelope if both present.

### Level

```
text: level V [MS V|@MS V...]
bin: 0a u0.16:V u8:COUNT (u16:MS u0.16:V)...
```

Master envelope.
Must appear at least once, and it's legal to have more than one.
Position matters.

If you're using delay, it's wise to add a final envelope that holds at 1, then drops to 0 after everything, to kill the echoes.

### Gain

```
text: gain MLT
bin: 0b u8.8:MLT
```

Multiply the signal.
Wise to do this at the end of each voice to tweak mix levels.

### Clip

```
text: clip LEVEL
bin: 0c u0.8 LEVEL
```

Clamp signal hard to the given level. Produces distortion if anything hits.

### Delay

```
text: delay MS DRY WET STORE FEEDBACK
bin: 0d u16:MS u0.8:DRY u0.8:WET u0.8:STORE u0.8:FEEDBACK
```

Cheap tape delay.
Beware that this extends the sounds length, in theory to infinity.
It's wise to follow delays with a level envelope to terminate them.

### Filters

```
text: bandpass MID WIDTH
text: notch MID WIDTH
text: lopass HZ
text: hipass HZ
bin: 0e u16:MID u16:WIDTH ; bandpass
bin: 0f u16:MID u16:WIDTH ; notch
bin: 10 u16:HZ            ; lopass
bin: 11 u16:HZ            ; hipass
```

IIR filters. MID and WIDTH are in Hertz.
