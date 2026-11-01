# Azahar keyboard debugging

The development PC has a selected **Codex Keyboard** profile. The previous
Default profile is preserved and its configuration was backed up before editing.
On another PC, enter these mappings in **Emulation → Configure → Controls**.

| 3DS input | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| Circle Pad | W/A/S/D |
| A / B / X / Y | J / K / U / I |
| L / R | Q / E |
| Start / Select | Enter / Backspace |
| C-stick up/left/down/right | T / F / G / H |

For ISO-PAL01, Left/Right select stages, J jumps between banner and clear,
Q/E orbit, K resets, U switches colour, I toggles depth, Enter exits.
Circle Pad and C-stick bindings are prepared for future prototypes; this banner
viewer does not implement captain movement or C-stick camera control.

Focus the emulated display before sending keys. **View → Single Window Mode**
can be unchecked to expose a separate emulation window for debugging. On the
development PC it is currently detached. The user confirmed J selects the real
banner, and the visible result was captured. Desktop automation still did not
reliably deliver controller keystrokes; that tooling limitation is separate from
the user-confirmed keyboard binding. Other input actions still need verification.

Keyboard configuration does not change physical New 3DS controls, and emulator
success does not establish that the physical rendering failure is fixed.
