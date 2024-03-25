# pcmprint

System for printing short sound effects.

There's a text format only used at build time (src/tool/eggrom/eggrom_tid_sound.c),
and a binary format used at run time (src/opt/pcmprint/).

## Text Format

Each file may contain multiple sounds.

Line-oriented text, '#' anywhere begins a line comment.

Each sound is fenced with `sound` and `end`:

```
sound 123
  ...
end

sound aMadeUpName
  ...
end
```

Each line in the sound block is a command:

| Keyword   | Parameters | Description |
|-----------|------------|-------------|
| voice     |            | Begin a new voice. Commands before are processed independently of those after, and mixed. |
| gain      | FLOAT      | Multiply by this scalar. |

TODO

## Binary Format

TODO
