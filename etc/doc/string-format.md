# Egg Strings Format

The string resource stored in ROM files is plain text, recommend UTF-8.

Input files, we pack multiple resources into one file. One file per language.

```
src/
  data/
    string/
      en
      fr
      ja
```

Within each file, each line is either blank, comment, or a string resource.
Comments begin with `#`.

String resources are: `ID CONTENT`

Where `ID` is an integer or a name for the resource (`[a-zA-Z_][0-9a-zA-Z_]*`),
and `CONTENT` is loose text or a quoted JSON string.

You must be careful to use the same ID or name across all files.
