/* synth_builtin.c
 * Definitions for the 128 General MIDI instruments.
 */
 
#include "synth_internal.h"

// Tiny envelope.
#define IMPULSE 0x00
#define PLUCK   0x40
#define TONE    0x80
#define BOW     0xc0
#define ATTACK(zero_thru_seven) (zero_thru_seven<<3)
#define RELEASE(zero_thru_seven) (zero_thru_seven)

#define BLIP(pid) [pid]={.mode=SYNTH_CHANNEL_MODE_BLIP},
#define WAVE(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_WAVE,.wave={__VA_ARGS__}},
#define ROCK(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_ROCK,.rock={__VA_ARGS__}},
#define FMREL(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FMREL,.fmrel={__VA_ARGS__}},
#define FMABS(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FMABS,.fmabs={__VA_ARGS__}},
#define SUB(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_SUB,.sub={__VA_ARGS__}},
#define FX(pid,...) [pid]={.mode=SYNTH_CHANNEL_MODE_FX,.fx={__VA_ARGS__}},
#define ALIAS(pid,otherpid) [pid]={.mode=SYNTH_CHANNEL_MODE_ALIAS,.alias=otherpid},

const struct synth_builtin synth_builtin[0x80]={

// 0..7: Piano
  BLIP(0x00) // Acoustic Grand Piano TODO
  ALIAS(0x01,0x00) // Bright Acoustic Piano TODO
  FMREL(0x02, // Electric Grand Piano
    .rate=0x20,
    .scale=0x40,
    .range=0x8fc8,
    .level=TONE|ATTACK(0)|RELEASE(4),
  )
  FMREL(0x03, // Honky-Tonk Piano
    .rate=0x18,
    .scale=0x40,
    .range=0xff82,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  ROCK(0x04, // EP 1 (Rhodes)
    .wave={0x80,0xc0,0x40,0x20,0x08},
    .mix=0x0f80,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  FMABS(0x05, // EP 2 (Chorus). The Spooky Organ.
    .rate=0x0800,
    .scale=0x01,
    .range=0x0f30,
    .level=PLUCK|ATTACK(2)|RELEASE(4),
  )
  FMREL(0x06, // Harpsichord
    .rate=0x50,
    .scale=0x50,
    .range=0xfff0,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FMREL(0x07, // Clavinet
    .rate=0x40,
    .scale=0x50,
    .range=0x8f00,
    .level=PLUCK|ATTACK(0)|RELEASE(5),
  )
  
// 8..15: Chromatic
  FMREL(0x08, // Celesta
    .rate=0x7a,
    .scale=0x87,
    .range=0xf87a,
    .level=IMPULSE|ATTACK(0)|RELEASE(6),
  )
  FMREL(0x09, // Glockenspiel
    .rate=0x30,
    .scale=0x40,
    .range=0xfff4,
    .level=IMPULSE|ATTACK(0)|RELEASE(3),
  )
  FMREL(0x0a, // Music Box
    .rate=0x62,
    .scale=0x80,
    .range=0xff84,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FMREL(0x0b, // Vibraphone
    .rate=0x40,
    .scale=0x40,
    .range=0x0ff4,
    .level=IMPULSE|ATTACK(1)|RELEASE(5),
  )
  FMREL(0x0c, // Marimba
    .rate=0x80,
    .scale=0x38,
    .range=0x8ff4,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FMREL(0x0c, // Xylophone
    .rate=0x80,
    .scale=0x30,
    .range=0xfff4,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FMREL(0x0e, // Tubular Bells
    .rate=0x38,
    .scale=0x67,
    .range=0xf8ff,
    .level=IMPULSE|ATTACK(1)|RELEASE(6),
  )  
  FMREL(0x0f, // Dulcimer
    .rate=0x60,
    .scale=0x70,
    .range=0x8f84,
    .level=IMPULSE|ATTACK(0)|RELEASE(6),
  )
  
// 16..23: Organ
  ROCK(0x10, // Drawbar Organ
    .wave={0xff,0x02,0x55,0x01,0x33,0x00,0x11},
    .mix=0x2f8c,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  ROCK(0x11, // Percussive Organ
    .wave={0x00,0xc0,0x04,0x40,0x01},
    .mix=0xc2f4,
    .level=PLUCK|ATTACK(0)|RELEASE(5),
  )
  ROCK(0x12, // Rock Organ
    .wave={0xc0,0x00,0x60,0x00,0x30,0x00,0x18},
    .mix=0x0f84,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  ROCK(0x13, // Church Organ
    .wave={0xc0,0xa0,0x60,0x00,0x30,0x00,0x18},
    .mix=0xfff0,
    .level=TONE|ATTACK(3)|RELEASE(6),
  )
  ROCK(0x14, // Reed Organ
    .wave={0x00,0x7c,0xa5,0x80,0x33,0x60,0x11,0x00},
    .mix=0x0ff0,
    .level=TONE|ATTACK(2)|RELEASE(3),
  )
  ALIAS(0x15,0x10) // Accordion (French) TODO
  ALIAS(0x16,0x10) // Harmonica TODO
  ALIAS(0x17,0x10) // Tango Accordion TODO
  
// 24..31: Guitar
  FMREL(0x18, // Nylon Acoustic Guitar
    .rate=0x20,
    .scale=0x10,
    .range=0x4fc0,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FX(0x19, // Steel Acoustic Guitar
    .rangeenv=0x0f80,
    .rangelfo=0x00,
    .rangelfo_depth=0x00,
    .rate=0x60,
    .scale=0x78,
    .detune_rate=0x40,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x10,
    .level=PLUCK|ATTACK(0)|RELEASE(4),
  )
  FX(0x1a, // Jazz Electric Guitar
    .rangeenv=0x0fff,
    .rangelfo=0x10,
    .rangelfo_depth=0x08,
    .rate=0x20,
    .scale=0x40,
    .detune_rate=0x40,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x00,
    .delay_depth=0x00,
    .level=PLUCK|ATTACK(0)|RELEASE(4),
  )
  FX(0x1b, // Clean Electric Guitar
    .rangeenv=0x7f84,
    .rangelfo=0x20,
    .rangelfo_depth=0x10,
    .rate=0x20,
    .scale=0x40,
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x08,
    .delay_depth=0x40,
    .level=PLUCK|ATTACK(1)|RELEASE(4),
  )
  FX(0x1c, // Muted Electric Guitar
    .rangeenv=0x0ff0,
    .rangelfo=0x20,
    .rangelfo_depth=0x20,
    .rate=0x20,
    .scale=0x40,
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0x10,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=PLUCK|ATTACK(0)|RELEASE(4),
  )
  FX(0x1d, // Overdriven Guitar
    .rangeenv=0xff80,
    .rangelfo=0x40,
    .rate=0x20,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(4),
    .detune_rate=0x40,
    .detune_depth=0x02,
    .overdrive=0x60,
    .delay_rate=0x10,
    .delay_depth=0x20,
  )
  FX(0x1e, // Distortion Guitar
    .rangeenv=0xff44,
    .rangelfo=0x20,
    .rangelfo_depth=0x08,
    .rate=0x08,
    .scale=0x30,
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0xe0,
    .delay_rate=0x10,
    .delay_depth=0x10,
    .level=PLUCK|ATTACK(3)|RELEASE(5),
  )
  FX(0x1f, // Guitar Harmonics
    .rangeenv=0x0ff0,
    .rangelfo=0x00,
    .rangelfo_depth=0x00,
    .rate=0x77,
    .scale=0x80,
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x10,
    .level=PLUCK|ATTACK(3)|RELEASE(5),
  )
  
// 32..39: Bass
  FMREL(0x20, // Acoustic Bass
    .rate=0x08,
    .scale=0x18,
    .range=0x6fd8,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  FX(0x21, // Fingered Electric Bass
    .rangeenv=0x8fc4,
    .rangelfo=0x20,
    .rangelfo_depth=0x04,
    .rate=0x08,
    .scale=0x20,
    .detune_rate=0x00,
    .detune_depth=0x00,
    .overdrive=0x00,
    .delay_rate=0x00,
    .delay_depth=0x00,
    .level=PLUCK|ATTACK(0)|RELEASE(3),
  )
  WAVE(0x22, // Picked Electric Bass
    .wave={0xc0,0xc0,0x30,0x00,0x10,0x80,0x40,0x20},
    .level=PLUCK|ATTACK(1)|RELEASE(3),
  )
  FMREL(0x23, // Fretless Bass
    .rate=0x08,
    .scale=0x20,
    .range=0x4f80,
    .level=TONE|ATTACK(3)|RELEASE(4),
  )
  FMREL(0x20,
    .rate=0x08,
    .scale=0x40,
    .range=0xf888,
    .level=BOW|ATTACK(3)|RELEASE(5),
  )
  FMREL(0x24, // Slap Bass 1
    .rate=0x08,
    .scale=0x28,
    .range=0x6f61,
    .level=PLUCK|ATTACK(1)|RELEASE(3),
  )
  ROCK(0x25, // Slap Bass 2
    .wave={0x40,0xff,0xc0,0x80,0x40,0x10,0x08,0x02},
    .mix=0x0f40,
    .level=PLUCK|ATTACK(2)|RELEASE(4),
  )
  FMREL(0x26, // Synth Bass 1
    .rate=0x08,
    .scale=0x30,
    .range=0x4fc2,
    .level=PLUCK|ATTACK(2)|RELEASE(2),
  )
  FMREL(0x27, // Synth Bass 2
    .rate=0x08,
    .scale=0x50,
    .range=0x8ff0,
    .level=PLUCK|ATTACK(1)|RELEASE(4),
  )
  
// 40..47: Solo String
  ROCK(0x28, // Violin
    .wave={0x20,0xc0,0x70,0x58,0x40,0x20,0x08},
    .mix=0x8fc0,
    .level=BOW|ATTACK(3)|RELEASE(5),
  )
  ROCK(0x29, // Viola
    .wave={0xff,0x80,0x55,0x40,0x33,0x20,0x08},
    .mix=0x8fc0,
    .level=BOW|ATTACK(3)|RELEASE(5),
  )
  ROCK(0x2a, // Cello
    .wave={0x40,0xff,0x80,0x55,0x40,0x33,0x20,0x08},
    .mix=0x8fc0,
    .level=BOW|ATTACK(3)|RELEASE(4),
  )
  FMREL(0x2b, // Contrabass
    .rate=0x08,
    .scale=0x40,
    .range=0xf888,
    .level=BOW|ATTACK(3)|RELEASE(5),
  )
  FX(0x2c, // Tremolo Strings
    .rangeenv=0x0fc4,
    .rangelfo=0x08,
    .rangelfo_depth=0x02,
    .rate=0x10,
    .scale=0x20,
    .detune_rate=0x00,
    .detune_depth=0x00,
    .overdrive=0x00,
    .delay_rate=0x00,
    .delay_depth=0x00,
    .level=BOW|ATTACK(0)|RELEASE(6),
  )
  FX(0x2d, // Pizzicato Strings
    .rangeenv=0x0fc4,
    .rangelfo=0x08,
    .rangelfo_depth=0x08,
    .rate=0x10,
    .scale=0x30,
    .detune_rate=0x04,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x10,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FX(0x2e, // Orchestral Harp
    .rangeenv=0x8fc4,
    .rangelfo=0x00,
    .rangelfo_depth=0x00,
    .rate=0x30,
    .scale=0x30,
    .detune_rate=0x04,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=IMPULSE|ATTACK(0)|RELEASE(6),
  )
  SUB(0x2f, // Timpani
    .width1=50,
    .width2=10,
    .gain=150,
    .level=IMPULSE|ATTACK(1)|RELEASE(3),
  )
  
// 48..55: String Ensemble
  FX(0x30, // String Ensemble 1
    .rangeenv=0x8fc4,
    .rangelfo=0x40,
    .rangelfo_depth=0x08,
    .rate=0x18,
    .scale=0x20,
    .detune_rate=0x10,
    .detune_depth=0x08,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  ALIAS(0x31,0x30) // String Ensemble 2 TODO
  FX(0x32, // Synth Strings 1
    .rangeenv=0xcfe4,
    .rangelfo=0x40,
    .rangelfo_depth=0x08,
    .rate=0x08,
    .scale=0x50,
    .detune_rate=0x10,
    .detune_depth=0x08,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  ALIAS(0x33,0x30) // Synth Strings 2 TODO
  ALIAS(0x34,0x30) // Choir Aahs TODO
  ALIAS(0x35,0x30) // Voice Oohs TODO
  ALIAS(0x36,0x30) // Synth Voice TODO
  ALIAS(0x37,0x30) // Orchestra Hit TODO
  
// 56..63: Brass
  ROCK(0x38, // Trumpet
    .wave={0x20,0xc0,0x60,0x50,0x40,0x10,0x08,0x02},
    .mix=0xf0f4,
    .level=BOW|ATTACK(3)|RELEASE(3),
  )
  ALIAS(0x39,0x38) // Trombone TODO
  ALIAS(0x3a,0x38) // Tuba TODO
  ALIAS(0x3b,0x38) // Muted Trumpet TODO
  ALIAS(0x3c,0x38) // French Horn TODO
  ALIAS(0x3d,0x38) // Brass Section TODO
  ALIAS(0x3e,0x38) // Synth Brass 1 TODO
  ALIAS(0x3f,0x38) // Synth Brass 2 TODO
  
// 64..71: Solo Reed
  FMREL(0x40, // Soprano Sax
    .rate=0x40,
    .scale=0x30,
    .range=0x4fc2,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  ALIAS(0x41,0x40) // Alto Sax TODO
  ALIAS(0x42,0x40) // Tenor Sax TODO
  ALIAS(0x43,0x40) // Baritone Sax TODO
  ALIAS(0x44,0x40) // Oboe TODO
  ALIAS(0x45,0x40) // English Horn TODO
  ALIAS(0x46,0x40) // Bassoon TODO
  ROCK(0x47, // Clarinet
    .wave={0x20,0x8c,0xe0,0x00,0x73,0x00,0x31,0x00},
    .mix=0x00ff,
    .level=PLUCK|ATTACK(1)|RELEASE(4),
  )
  
// 72..79: Solo Flute
  BLIP(0x48) // Piccolo TODO
  ALIAS(0x49,0x48) // Flute TODO
  WAVE(0x4a, // Recorder
    .wave={0x80,0x10,0xf5,0x00,0x33,0x00,0x11,0x04},
    .level=TONE|ATTACK(4)|RELEASE(3),
  )
  ALIAS(0x4b,0x48) // Pan Flute TODO
  SUB(0x4c, // Blown Bottle
    .width1=25,
    .width2=15,
    .gain=45,
    .level=TONE|ATTACK(1)|RELEASE(4),
  )
  FMREL(0x4d, // Shakuhachi
    .rate=0x43,
    .scale=0x18,
    .range=0x0f30,
    .level=PLUCK|ATTACK(2)|RELEASE(4),
  )
  ALIAS(0x4e,0x48) // Whistle TODO
  FMREL(0x4f, // Ocarina
    .rate=0x20,
    .scale=0x28,
    .range=0x8f00,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  
// 80..87: Synth Lead
  WAVE(0x50, // Square Lead
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x10},
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  WAVE(0x51, // Saw Lead
    .wave={0xff,0x80,0x55,0x40,0x33,0x20,0x10,0x08},
    .level=TONE|ATTACK(2)|RELEASE(3),
  )
  ROCK(0x52, // Calliope
    .wave={0x00,0xff,0x20},
    .mix=0x0f80,
    .level=TONE|ATTACK(1)|RELEASE(5),
  )
  BLIP(0x53) // Chiffer. This is intentionally BLIP, not a placeholder.
  ROCK(0x54, // Sounds exactly like whatever the hell a "Charang" is.
    .wave={0x00,0xff,0x40,0x20,0x10,0x08},
    .mix=0x00ff,
    .level=TONE|ATTACK(1)|RELEASE(4),
  )
  FMREL(0x55, // Voice Solo
    .rate=0x28,
    .scale=0x20,
    .range=0xf8c4,
    .level=BOW|ATTACK(3)|RELEASE(4),
  )
  FMREL(0x56, // Fifths
    .rate=0x18,
    .scale=0x40,
    .range=0x8fc4,
    .level=TONE|ATTACK(3)|RELEASE(4),
  )
  ROCK(0x57, // Bass and Lead
    .wave={0x00,0x00,0x00,0xff},
    .mix=0xcf00,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  
// 88..95: Synth Pad
  FX(0x58, // Fantasia Pad
    .rangeenv=0xffff,
    .rangelfo=0x80,
    .rangelfo_depth=0x20,
    .rate=0x10,
    .scale=0x10,
    .detune_rate=0x10,
    .detune_depth=0x18,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=BOW|ATTACK(0)|RELEASE(7),
  )
  WAVE(0x59, // Warm Pad
    .wave={0xff,0x10,0x08,0x02},
    .level=BOW|ATTACK(0)|RELEASE(3),
  )
  FX(0x5a, // Polysynth Pad
    .rangeenv=0xcf40,
    .rangelfo=0x40,
    .rangelfo_depth=0x10,
    .rate=0x10,
    .scale=0x40,
    .detune_rate=0x40,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  SUB(0x5b, // Choir Space Voice
    .width1=40,
    .width2=25,
    .gain=40,
    .level=BOW|ATTACK(3)|RELEASE(4),
  )
  SUB(0x5c, // Bowed Glass
    .width1=20,
    .width2=40,
    .gain=30,
    .level=BOW|ATTACK(2)|RELEASE(5),
  )
  FX(0x5d, // Metallic Pad
    .rangeenv=0xfff0,
    .rangelfo=0x40,
    .rangelfo_depth=0x04,
    .rate=0x76,
    .scale=0x30,
    .detune_rate=0x80,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x08,
    .level=BOW|ATTACK(2)|RELEASE(6),
  )
  ROCK(0x5e, // Halo Pad
    .wave={0x00,0xff,0x20,0x10,0x08,0x04,0x02,0x01},
    .mix=0x04f0,
    .level=BOW|ATTACK(3)|RELEASE(6),
  )
  FX(0x5f, // Sweep Pad
    .rangeenv=0xffff,
    .rangelfo=0x40,
    .rangelfo_depth=0x20,
    .rate=0x40,
    .scale=0x30,
    .detune_rate=0x40,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x10,
    .delay_depth=0x08,
    .level=BOW|ATTACK(2)|RELEASE(6),
  )
  
// 96..103: Synth Effects
  FX(0x60, // Rain
    .rangeenv=0x8ff0,
    .rangelfo=0x80,
    .rangelfo_depth=0x80,
    .rate=0x38,
    .scale=0x60,
    .detune_rate=0x00,
    .detune_depth=0x00,
    .overdrive=0x00,
    .delay_rate=0x08,
    .delay_depth=0xa0,
    .level=IMPULSE|ATTACK(2)|RELEASE(3),
  )
  ALIAS(0x61,0x60) // Soundtrack TODO
  ALIAS(0x62,0x60) // Crystal TODO
  ALIAS(0x63,0x60) // Atmosphere TODO
  ALIAS(0x64,0x60) // Brightness TODO
  FX(0x65, // Goblins. My favorite instrument here. (being a goblin and all)
    .rangeenv=0x8ff0,
    .rangelfo=0x40,
    .rangelfo_depth=0x40,
    .rate=0x08,
    .scale=0x40,
    .detune_rate=0x00,
    .detune_depth=0x00,
    .overdrive=0x40,
    .delay_rate=0x10,
    .delay_depth=0x80,
    .level=PLUCK|ATTACK(2)|RELEASE(3),
  )
  FX(0x66, // Echoes, Drops
    .rangeenv=0xf0ff,
    .rangelfo=0x40,
    .rangelfo_depth=0x80,
    .rate=0x20,
    .scale=0x10,
    .detune_rate=0x10,
    .detune_depth=0x20,
    .overdrive=0x00,
    .delay_rate=0x08,
    .delay_depth=0xa0,
    .level=PLUCK|ATTACK(2)|RELEASE(3),
  )
  FX(0x67, // Sci-Fi Star Theme
    .rangeenv=0x0fff,
    .rangelfo=0x80,
    .rangelfo_depth=0x18,
    .rate=0x20,
    .scale=0x60,
    .detune_rate=0x10,
    .detune_depth=0x08,
    .overdrive=0x08,
    .delay_rate=0x10,
    .delay_depth=0x20,
    .level=BOW|ATTACK(1)|RELEASE(5),
  )
  
// 104..111; World
  BLIP(0x68) // Sitar TODO
  ALIAS(0x69,0x68) // Banjo TODO
  ALIAS(0x6a,0x68) // Shamisen TODO
  ALIAS(0x6b,0x68) // Koto TODO
  ALIAS(0x6c,0x68) // Kalimba TODO
  ALIAS(0x6d,0x68) // Bag Pipe TODO
  ALIAS(0x6e,0x68) // Fiddle TODO
  ALIAS(0x6f,0x68) // Shanai TODO
  
// 112..119: Percussion
  FMREL(0x70, // Tinkle Bell
    .rate=0x90,
    .scale=0xa0,
    .range=0xff8f,
    .level=IMPULSE|ATTACK(0)|RELEASE(6),
  )
  FMREL(0x71, // Agogo
    .rate=0x57,
    .scale=0x40,
    .range=0xffff,
    .level=IMPULSE|ATTACK(0)|RELEASE(5),
  )
  FMREL(0x72, // Steel Drums
    .rate=0x58,
    .scale=0x40,
    .range=0xf0ff,
    .level=IMPULSE|ATTACK(1)|RELEASE(5),
  )
  FMREL(0x73, // Wood Block
    .rate=0x18,
    .scale=0x20,
    .range=0x0fff,
    .level=IMPULSE|ATTACK(0)|RELEASE(2),
  )
  FMREL(0x74, // Taiko
    .rate=0x04,
    .scale=0x30,
    .range=0xf0ff,
    .level=IMPULSE|ATTACK(1)|RELEASE(5),
  )
  FMREL(0x75, // Melodic Tom
    .rate=0x18,
    .scale=0x38,
    .range=0xf8f0,
    .level=IMPULSE|ATTACK(1)|RELEASE(3),
  )
  FX(0x76, // Synth Drum
    .rangelfo=0x0a,
    .rangelfo_depth=0x20,
    .rate=0x08,
    .scale=0x20,
    .rangeenv=0xff80,
    .detune_rate=0x08,
    .detune_depth=0x04,
    .level=IMPULSE|ATTACK(0)|RELEASE(3),
  )
  ALIAS(0x77,0x70) // Reverse Cymbal TODO
  
// 120..127: Insert Joke Here
  FMREL(0x78, // Guitar Fret Noise
    .rate=0x58,
    .scale=0xa0,
    .range=0xf88f,
    .level=IMPULSE|ATTACK(3)|RELEASE(4),
  )
  SUB(0x79, // Breath Noise
    .width1=40,
    .width2=40,
    .gain=30,
    .level=PLUCK|ATTACK(3)|RELEASE(3),
  )
  SUB(0x7a, // Seashore
    .width1=200,
    .width2=100,
    .gain=5,
    .level=BOW|ATTACK(3)|RELEASE(7),
  )
  FMABS(0x7b, // Bird Tweet
    .rate=0x0800,
    .scale=0x12,
    .range=0xf400,
    .level=PLUCK|ATTACK(1)|RELEASE(5),
  )
  FMABS(0x7c, // Telephone Ring
    .rate=0x1000,
    .scale=0x08,
    .range=0xcfc0,
    .level=IMPULSE|ATTACK(3)|RELEASE(3),
  )
  FMABS(0x7d, // Helicopter. The kind the invaders from Mars fly.
    .rate=0x0800,
    .scale=0x10,
    .range=0x0ff0,
    .level=TONE|ATTACK(1)|RELEASE(5),
  )
  BLIP(0x7e) // Applause. There will be none.
  SUB(0x00, // Gunshot
    .width1=400,
    .width2=400,
    .gain=1,
    .level=IMPULSE|ATTACK(1)|RELEASE(4),
  )
  
};
