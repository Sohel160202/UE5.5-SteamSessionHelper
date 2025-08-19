# UE5.5-SteamSessionHelper
Blueprint-friendly fix for Steam hosting/joining issues in Unreal Engine 5.5+.
This plugin wraps the Steam Online Subsystem (OSS) with async Blueprint nodes and applies fixes for common UE5.5 Steam SDK session problems that affect indie developers.

Features:
- Create Steam Sessions (properly configured for Steam presence + lobbies)
- Find Steam Sessions (returns clean results with map, owner, slots, ping, keyword tag)
- Join Steam Sessions (with automatic flag normalization to fix UE5.5 join failures)
- Blueprint Async Nodes (OnSuccess / OnFailure pins, no C++ required)
- Indie developer–friendly: works out of the box without modifying engine code

Why This Plugin? (The Problem It Solves):
- Unreal Engine 5.5 introduced quirks in OnlineSubsystemSteam that break multiplayer workflow:
- JoinSession() often fails due to mismatched lobby/presence flags.
- Sessions advertise but aren’t visible without extra settings.
- FindSessions() gives incomplete/unusable results.
- Blueprint-only projects cannot easily work around these issues.

This plugin fixes those problems and restores a reliable Steam multiplayer workflow.

Requirements: 
Enable these Unreal plugins (Edit → Plugins → Online):
- Online Subsystem Steam
- Steam Shared Module
- Steam Sockets

Installation:
- Enable Required Plugins
    Online Subsystem Steam
    Steam Shared Module
    Steam Sockets

- Restart Unreal
- Convert to C++ Project
    Add an empty C++ class (needed so plugins can compile).

- Close Unreal.
- Add Plugin
  Drop SteamSessionHelper/ into your project’s Plugins/ folder.

- Configure Steam in Config/DefaultEngine.ini
  Copy Pas

- Open your project and compile.
