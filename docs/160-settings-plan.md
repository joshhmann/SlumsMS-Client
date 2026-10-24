# Ticket 73: SlumsMS settings inside the game

Base: client-1.5.0-fixes3 at 85b1f5b. Branch client-160-settings, own worktree.

Add a small owned, modeless Windows settings panel through the existing game-window procedure. Open it with
Ctrl+F10 or the window system menu. Only explicit player input opens/focuses it. Keep the normal game message
loop running. Offer only supported options: attack walk/jump/charge movement, double-tap Ninja Bounce, quest
lightbulbs, character text colour, Discord activity, and weather sway/rope sway when compiled. Read the same
config keys and legacy movement fallback; write only those keys on Save. Cancel writes nothing. State clearly
that restart applies saved options. Preserve host/port, unrelated options and all current defaults. Leave game
resolution in the existing game Options window because the current renderer stores it separately.

No new fixed client-address patch, packet or server change. Add a runtime off switch, document the shortcut and
add portable tests for defaults, legacy fallback, known-key-only writes and save failures. Build MSVC Win32 in a
new SlumsMS-dev/client-160-settings folder with the current feature flags, bounded parallelism, then run ctest.
Do not start a game while the client-MCP worker is unavailable. Record that actual visuals, input/focus and restart
behavior need a real client check; no publish until that evidence exists.
