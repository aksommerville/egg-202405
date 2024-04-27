# Egg Joystick Handling

Joysticks, and any generic input devices, are reported with `INPUT`, `CONNECT`, and `DISCONNECT` events.
Platform assigns a unique positive `devid` to each device.
`devid` may be reused after a `DISCONNECT`, but our current platform implementations won't.

```
EGG_EVENT_CONNECT
  v[0]: devid
  v[1]: mapping: 0=unknown, 1=standard (see below)
  
EGG_EVENT_DISCONNECT
  v[0]: devid
  
EGG_EVENT_INPUT
  v[0]: devid
  v[1]: btnid: Opaque integer selected by the driver.
  v[2]: value: As reported by the driver. If the platform uses floats, we scale them to -128..127.
```

## Manual Mapping

On a `CONNECT` event, you're expected to query the device IDs and capabilities, and map accordingly.
We provide client-side helpers for this. (but it's outside the jurisdiction of Egg proper).

```
void egg_input_device_get_button(
  int *btnid,int *hidusage,int *lo,int *hi,int *value,
  int devid,int index
);
```

Call this with successive `index`, starting with zero, until it reports a zero `btnid`.
`lo..hi` is the nominal range of the button, inclusive, eg (0,1) for a regular two-state button.
`value` is hopefully the resting value of the button, but driver might only be able to give us the current value.
Hopefully all buttons are at rest at the moment of connection, so those are the same thing.

`hidusage` is a USB-HID usage page and code (page in the high 16 bits, code in the low 16), if available.
Hardware support here is spotty but when a device reports hidusage nicely, it's a real boost.

## Standard Mapping

The W3C defines a "Standard Gamepad" that platforms should try to map to, and god bless them for it.

When a device claims to be in Standard Mapping, Egg will set `v[1]` of its `CONNECT` event to 1.
Web browsers seem to have adopted this standard. Native APIs, I haven't seen it yet.

Egg doesn't have a concept of separate Buttons and Axes, so we add 128 to the ID of Buttons, and 64 to Axes.
(This also ensures that they not be zero, which would be illegal in Egg).

https://w3c.github.io/gamepad/#remapping

| W3C | Egg  | Description |
|-----|------|-------------|
| b0  | 0x80 | South Thumb |
| b1  | 0x81 | East Thumb |
| b2  | 0x82 | West Thumb |
| b3  | 0x83 | North Thumb |
| b4  | 0x84 | L1 |
| b5  | 0x85 | R1 |
| b6  | 0x86 | L2 |
| b7  | 0x87 | R2 |
| b8  | 0x88 | Aux Left (AUX2, Select) |
| b9  | 0x89 | Aux Right (AUX1, Start) |
| b10 | 0x8a | Left Stick Plunger |
| b11 | 0x8b | Right Stick Plunger |
| b12 | 0x8c | Dpad Up |
| b13 | 0x8d | Dpad Down |
| b14 | 0x8e | Dpad Left |
| b15 | 0x8f | Dpad Right |
| b16 | 0x90 | Aux Center (AUX3, Heart) |
| a0  | 0x40 | Left X |
| a1  | 0x41 | Left Y |
| a2  | 0x42 | Right X |
| a3  | 0x43 | Right Y |

Axes are negative for left and up, positive for right and down.

## Conventions

There's disagreement over exactly how joystick buttons should be used.
I'll propose a few of my own opinions as guidance.
Would be nice if all games on Egg follow the same rough guidelines.

- Dpad and Left Stick should both be the principal control, and the game should work with either.
- Right Stick should only be used in first-person point-of-view games, which Egg doesn't really do.
- South should be the primary action, and Yes in menus.
- West should be the secondary action, and No in menus.
- "primary action" is the thing the player does the most. Typically jump in a platformer, or attack in an adventure.
- Aux1 (Start) should pause the game or open a menu.
- If Aux1 opens a menu, it should also close that menu.
- West should close any modal menu.
- Avoid using Aux2, Aux3, L2, R2, LP, RP: Not all devices have these, and many devices falsely report that they do.
- - Unless of course you actually need that many buttons.
- Ideally the game should be playable with only: Dpad, South, West, Aux1.
- Always allow the player to remap joysticks. I'll supply client libraries to make it easy.
