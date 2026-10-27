# Changelog

## Rule the Kingdom 1.1 (2026-10-27)

- Only the 5.11 APK is needed: put the APK file next to the game, as it is. The game reads it
  directly; the update packs (main.27, patch.33) and the Windows 8 build's extra art now ship with
  the game.
- Downloads for Windows and Linux, beside the Mac DMG.
- Fullscreen and resizing: the HUD moves with the window, and a big window no longer shows too much
  of the map (the view is kept to what 5.11's largest screens showed).
- The inventory / crafting bar sits in the bottom-right corner at every screen size (its offsets
  are scaled as in the original).
- The Mac installer shows the game's own logo.
- The game's SDL error messages show SDL's own text again (the old font library's stand-in had
  replaced it).

## Rule the Kingdom 1.0 (2026-10-27)

The first complete release of the port: every milestone is closed.

- The whole of version 5.11 is ported function by function from the original Android library:
  the city, the economy, the hero and the army, quests and campaigns, the boss fights, crafting,
  chests, the item shop, professions, the arena and offline PvP, the calendar events, sounds and
  particles.
- Plays 100% offline. No real-money purchases: crystals are earned in play. Online features
  (Facebook profiles, friends, the social tabs) are ported but dormant.
- A native macOS app (Retina, fullscreen with F11, a frame-rate cap with F10, desktop
  notifications, screenshots saved to ~/Pictures/Rule the Kingdom).
- Needs the player's own copy of the original game files; none are included.
