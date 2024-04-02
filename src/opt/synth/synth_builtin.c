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
  ALIAS(0x02,0x00) // Electric Grand Piano TODO
  ALIAS(0x03,0x00) // Honky-tonk Piano TODO
  ALIAS(0x04,0x00) // EP 1 (Rhodes) TODO
  FMABS(0x05, // EP 2 (Chorus). The Spooky Organ.
    .rate=0x0800,
    .scale=0x01,
    .range=0x0f30,
    .level=PLUCK|ATTACK(2)|RELEASE(4),
  )
  ALIAS(0x06,0x00) // Harpsichord TODO
  ALIAS(0x07,0x00) // Clavinet TODO
  
// 8..15: Chromatic
  FMREL(0x08, // Celesta
    .rate=0x7a,
    .scale=0x87,
    .range=0xf87a,
    .level=IMPULSE|ATTACK(0)|RELEASE(6),
  )
  ALIAS(0x09,0x08) // Glockenspiel TODO
  ALIAS(0x0a,0x08) // Music Box TODO
  ALIAS(0x0b,0x08) // Vibraphone TODO
  ALIAS(0x0c,0x08) // Marimba TODO
  ALIAS(0x0d,0x08) // Xylophone TODO
  FMREL(0x0e, // Tubular Bells
    .rate=0x38,
    .scale=0x67,
    .range=0xf8ff,
    .level=IMPULSE|ATTACK(1)|RELEASE(6),
  )  
  ALIAS(0x0f,0x08) // Dulcimer TODO
  
// 16..23: Organ
  BLIP(0x10) // Drawbar Organ TODO
  ALIAS(0x11,0x10) // Percussive Organ TODO
  ALIAS(0x12,0x10) // Rock Organ TODO
  ALIAS(0x13,0x10) // Church Organ TODO
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
  ALIAS(0x19,0x18) // Steel Acoustic Guitar TODO
  ALIAS(0x1a,0x18) // Jazz Electric Guitar TODO
  ALIAS(0x1b,0x18) // Clean Electric Guitar TODO
  ALIAS(0x1c,0x18) // Muted Electric Guitar TODO
  FX(0x1d, // Overdriven Guitar
    .rangeenv=0xff80,
    .rangelfo=0x40,
    .rate=0x20,
    .scale=0x50,
    .level=PLUCK|ATTACK(1)|RELEASE(4),
    .detune_rate=0x40,
    .detune_depth=0x02,
    .overdrive=0x60,
    .delay_rate=0x10,
    .delay_depth=0x20,
  )
  ALIAS(0x1e,0x18) // Distortion Guitar TODO
  ALIAS(0x1f,0x18) // Guitar Harmonics TODO
  
// 32..39: Bass
  BLIP(0x20) // Acoustic Bass TODO
  ALIAS(0x21,0x20) // Fingered Electric Bass TODO
  ALIAS(0x22,0x20) // Picked Electric Bass TODO
  ALIAS(0x23,0x20) // Fretless Bass TODO
  FMREL(0x24, // Slap Bass 1
    .rate=0x08,
    .scale=0x28,
    .range=0x6f61,
    .level=PLUCK|ATTACK(1)|RELEASE(3),
  )
  ALIAS(0x25,0x20) // Slap Bass 2 TODO
  ALIAS(0x26,0x20) // Synth Bass 1 TODO
  ALIAS(0x27,0x20) // Synth Bass 2 TODO
  
// 40..47: Solo String
  BLIP(0x28) // Violin TODO
  ALIAS(0x29,0x28) // Viola TODO
  ALIAS(0x2a,0x28) // Cello TODO
  ALIAS(0x2b,0x28) // Contrabass TODO
  ALIAS(0x2c,0x28) // Tremolo Strings TODO
  ALIAS(0x2d,0x28) // Pizzicato Strings TODO
  ALIAS(0x2e,0x28) // Orchestral Harp TODO
  ALIAS(0x2f,0x28) // Timpani TODO
  
// 48..55: String Ensemble
  BLIP(0x30) // String Ensemble 1 TODO
  ALIAS(0x31,0x30) // String Ensemble 2 TODO
  ALIAS(0x32,0x30) // Synth Strings 1 TODO
  ALIAS(0x33,0x30) // Synth Strings 2 TODO
  ALIAS(0x34,0x30) // Choir Aahs TODO
  ALIAS(0x35,0x30) // Voice Oohs TODO
  ALIAS(0x36,0x30) // Synth Voice TODO
  ALIAS(0x37,0x30) // Orchestra Hit TODO
  
// 56..63: Brass
  BLIP(0x38) // Trumpet TODO
  ALIAS(0x39,0x38) // Trombone TODO
  ALIAS(0x3a,0x38) // Tuba TODO
  ALIAS(0x3b,0x38) // Muted Trumpet TODO
  ALIAS(0x3c,0x38) // French Horn TODO
  ALIAS(0x3d,0x38) // Brass Section TODO
  ALIAS(0x3e,0x38) // Synth Brass 1 TODO
  ALIAS(0x3f,0x38) // Synth Brass 2 TODO
  
// 64..71: Solo Reed
  BLIP(0x40) // Soprano Sax TODO
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
  ALIAS(0x4f,0x48) // Ocarina TODO
  
// 80..87: Synth Lead
  BLIP(0x50) // Square Lead. This should always be BLIP, it's not a placeholder.
  ALIAS(0x51,0x50) // Saw Lead TODO
  ALIAS(0x52,0x50) // Calliope TODO
  ALIAS(0x53,0x50) // Chiffer TODO
  ALIAS(0x54,0x50) // Charang TODO
  ALIAS(0x55,0x50) // Voice Solo TODO
  ALIAS(0x56,0x50) // Fifths TODO
  ALIAS(0x57,0x50) // Bass and Lead TODO
  
// 88..95: Synth Pad
  BLIP(0x58) // Fantasia Pad TODO
  ALIAS(0x59,0x58) // Warm Pad TODO
  ALIAS(0x5a,0x58) // Polysynth Pad TODO
  SUB(0x5b, // Choir Space Voice
    .width1=40,
    .width2=25,
    .gain=40,
    .level=BOW|ATTACK(3)|RELEASE(4),
  )
  ALIAS(0x5c,0x58) // Bowed Glass TODO
  ALIAS(0x5d,0x58) // Metallic Pad TODO
  ALIAS(0x5e,0x58) // Halo Pad TODO
  ALIAS(0x5f,0x58) // Sweep Pad TODO
  
// 96..103: Synth Effects
  BLIP(0x60) // Rain TODO
  ALIAS(0x61,0x60) // Soundtrack TODO
  ALIAS(0x62,0x60) // Crystal TODO
  ALIAS(0x63,0x60) // Atmosphere TODO
  ALIAS(0x64,0x60) // Brightness TODO
  ALIAS(0x65,0x60) // Goblins TODO
  ALIAS(0x66,0x60) // Echoes, Drops TODO
  ALIAS(0x67,0x60) // Sci-Fi Star Theme TODO
  
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
  BLIP(0x70) // Tinkle Bell TODO
  ALIAS(0x71,0x70) // Agogo TODO
  ALIAS(0x72,0x70) // Steel Drums TODO
  ALIAS(0x73,0x70) // Wood Block TODO
  ALIAS(0x74,0x70) // Taiko TODO
  ALIAS(0x75,0x70) // Melodic Tom TODO
  ALIAS(0x76,0x70) // Synth Drum TODO
  ALIAS(0x77,0x70) // Reverse Cymbal TODO
  
// 120..127: Insert Joke Here
  BLIP(0x78) // Guitar Fret Noise TODO
  ALIAS(0x79,0x78) // Breath Noise TODO
  ALIAS(0x7a,0x78) // Seashore TODO
  ALIAS(0x7b,0x78) // Bird Tweet TODO
  ALIAS(0x7c,0x78) // Telephone Ring TODO
  ALIAS(0x7d,0x78) // Helicopter TODO
  ALIAS(0x7e,0x78) // Applause TODO
  ALIAS(0x7f,0x78) // Gunshot TODO
  
#if 0 /* XXX Early sketches. Probly nothing worth keeping.
  //WAVE(0x00,.wave={0})
  FX(0x00,
    .rangeenv=0x8fc4,
    .rangelfo=0x20,
    .rangelfo_depth=0x10,
    .rate=0x10,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(1),
    //.detune_rate=0x40,
    //.detune_depth=0x10,
    .overdrive=0xff,
    .delay_rate=0x08,
    .delay_depth=0x80,
  )
  BLIP(0x01)
  FX(0x51,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rangelfo_depth=0x08,
    .rate=0x08,
    .scale=0x30,
    .level=PLUCK|ATTACK(2)|RELEASE(3),
    .detune_rate=0x10,
    .detune_depth=0x08,
    .overdrive=0x00,
    .delay_rate=0x00,
    .delay_depth=0x00,
  )
  BLIP(0x02)
  BLIP(0x03)
  BLIP(0x04)
  BLIP(0x05)
  BLIP(0x06)
  BLIP(0x07)
  BLIP(0x08)
  BLIP(0x09)
  BLIP(0x0a)
  BLIP(0x0b)
  BLIP(0x0c)
  BLIP(0x0d)
  
  // eye-of-newt
  FX(0x0e,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rate=0x20,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(5),
    .detune_rate=0x10,
    .detune_depth=0x10,
    .overdrive=0x00,
    .delay_rate=0x20,
    .delay_depth=0x40,
  )
  ROCK(0x0f,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .mix=0x0f80,
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  FMREL(0x10,
    .rate=0x08,
    .scale=0x30,
    .range=0x0f61,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  SUB(0x11,
    .width1=40,
    .width2=20,
    .gain=30,
    .level=BOW|ATTACK(7)|RELEASE(6),
  )
  FMABS(0x12,
    .rate=0x0600,
    .scale=0x04,
    .range=0x8f20,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  
  BLIP(0x13)
  BLIP(0x14)
  BLIP(0x15)
  BLIP(0x16)
  BLIP(0x17)
  BLIP(0x18)
  BLIP(0x19)
  BLIP(0x1a)
  BLIP(0x1b)
  BLIP(0x1c)
  BLIP(0x1d)
  BLIP(0x1e)
  BLIP(0x1f)
  BLIP(0x20)
  BLIP(0x21)
  BLIP(0x22)
  BLIP(0x23)
  BLIP(0x24)
  BLIP(0x25)
  BLIP(0x26)
  BLIP(0x27)
  BLIP(0x28)
  BLIP(0x29)
  BLIP(0x2a)
  BLIP(0x2b)
  FMREL(0x2c,
    .rate=0x08,
    .scale=0x20,
    .range=0x8f84,
    .level=TONE|ATTACK(1)|RELEASE(4),
  )
  WAVE(0x71,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  ROCK(0x72,
    .wave={0xff,0x00,0x55,0x00,0x33,0x00,0x11,0x00},
    .mix=0x0f80,
    .level=PLUCK|ATTACK(0)|RELEASE(7),
  )
  FMREL(0x73,
    .rate=0x08,
    .scale=0x30,
    .range=0x0f61,
    .level=TONE|ATTACK(2)|RELEASE(4),
  )
  FMABS(0x74,
    .rate=0x0600,
    .scale=0x30,
    .range=0x8f20,
    .level=TONE|ATTACK(1)|RELEASE(3),
  )
  SUB(0x75,
    .width1=40,
    .width2=20,
    .gain=30,
    .level=BOW|ATTACK(7)|RELEASE(6),
  )
  FX(0x76,
    .rangeenv=0x0f80,
    .rangelfo=0x10,
    .rate=0x20,
    .scale=0x30,
    .level=PLUCK|ATTACK(1)|RELEASE(5),
    .detune_rate=0x10,
    .detune_depth=0x40,
    .overdrive=0x80,
    .delay_rate=0x20,/*spacelessComment*/
    .delay_depth=0x40,
  )
#endif
};
