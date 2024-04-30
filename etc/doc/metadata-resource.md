# Egg Metadata Resources

Egg ROMs are expected to contain a resource `1:0:1` describing the game for external tools.

It's a packed set of Key=Value pairs, keys and values both limited to 255 bytes.
Keys are expected to be unique. Decoder behavior is undefined if a key is repeated.

```
  u8 key length
  u8 value length
  ... key
  ... value
```

All standard keys are C identifiers beginning with a lowercase letter.
It's fine to make up new keys. Wise to add a dot or space or something so it's not mistaken for a standard one.
Any text resources for human consumption may have an alternate version with "Str" appended, a string ID for translation.
If you use that, I recommend providing both keys (eg "title" and "titleStr"), with a default for readers that deign not to resolve the strings.

| Key         | Value |
|-------------|-------|
| title       | Game's full title in its original language. |
| titleStr    | Decimal rid of a string resource of the title, for translation. |
| author      | Game's author, for attribution purposes. |
| publisher   | If different from author, this is the legal body distributing the game. |
| copyright   | eg "(c) 2024 AK Sommerville" |
| freedom     | Enum, see below. |
| rating      | eg "ESRB:M", ratings from official agencies. |
| advisory    | Whether rated or not, some freeform text to advise of potentially offensive content. |
| genre       | See below. |
| tags        | See below. |
| framebuffer | "WxH", decimal. Runtime may use to guide selection of window size. |
| language    | Comma-delimited ISO 639 language codes, in order of your preference. The original should come first. |
| required    | Comma-delimited list of features that the game is not expected to work without (see below). |
| optional    | Like (required), but only suggesting "we're better if this is enabled". |
| iconImage   | Decimal rid of an image resource. Recommend 32x32 pixels. |
| posterImage | Decimal rid of a larger image resource. Screencap or promo graphics. |
| description | Freeform description of the game. |
| multiplayer | "single", "local", "online", "mixed" |
| version     | Version of this game (nothing to do with Egg's version). Recommend "vN.N.N" |
| timestamp   | Date and time of publication, ISO 8601. "YYYY-MM-DD" and optionally "THH:MM:SS" (but who cares about that). Or just "YYYY". |
| support     | URL, email address, or phone number, if you provide tech support to end users. |
| source      | URL of source code repository. |
| homepage    | URL of game's promotional website. |
| restriction | Freeform text advising of legal restrictions. "Must be 18" or "Do not distribute in Germany"...? |
| country     | "Made in USA" or wherever, if you feel like bragging about it. |

## freedom

This is a short identifier for machine consumption, as a summary of the game's license terms.
Must be one of these values:

- `restricted`: Assume that a license is required to play the game, and not allowed to redistribute or modify at all.
- `limited`: Redistribution is allowed in some cases, but please review the license first.
- `intact`: OK to redistribute but only if not modified at all.
- `free`: Public domain or close to. Users may copy and modify the game without limit.

I expect this to be helpful if there's ever a big collection of Egg games from disparate authors,
and I want to automatedly copy everything I'm allowed to, into some public distribution.

If omitted, assume "limited", ie don't assume anything without reviewing the formal license.

## Genre

Please pick a genre and go with it, it doesn't have to be perfect.
To keep things formatted consistently, try to use a string that already exists on itch.io.
No particular reason for Itch here, just a source we can all refer to.

On 2024-03-12, Itch's Genre menu shows these:

- Action
- Adventure
- Card Game
- Educational
- Fighting
- Interactive Fiction
- Platformer
- Puzzle
- Racing
- Rhythm
- Role Playing
- Shooter
- Simulation
- Sports
- Strategy
- Survival
- Visual Novel

Or of course you can make something up. But try to use one of these, it's helpful to your users.

## Tags

Comma-delimited strings.

Same idea as Genre, let's try to use tags that already exist on itch.io, just for a common reference.

## Required/Optional Features

TODO

## Input Format

Store as a file "metadata" in the project's data directory.
Line-oriented text. `#` begins a comment at the start of the line only.

Each non-empty line is `KEY = VALUE` or `KEY : VALUE`, spaces optional.

It is not possible to embed a newline in a value, and I don't feel that should be necessary.
