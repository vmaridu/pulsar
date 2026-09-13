# 🟨 Firmware — not written yet

This build is layout and contract only so far → [device.md](device.md).

When real firmware lands here, it follows the same flat pattern as
[ws_lcd_154](../ws_lcd_154/): every `.ino` / `.cpp` / `.h` file sits directly in this
folder, alongside this same README, `device.md` and `mockup.html` — no extra
`src/`, `docs/` or `mockups/` subfolder. Arduino requires the main `.ino` to
share its containing folder's name, so that file would be `ws_lcd_349.ino`, same
pattern as ws_lcd_154's `ws_lcd_154.ino`.
