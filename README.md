# Half-Life 2 in the CSGO engine (Atleast, an attempt of doing so)

This project is aimed to port HL2 to the leaked CSGO engine while keeping the benefits of the CSGO branch (such as the better lighting).<br>

### HL2GO doesn't actually work, see below as to why.
You can compile both client and server DLLs and you can get in game, but you'll only see a black screen and nothing else. You can see a row of pixels verically along the left side of the window moving based on player activity, so something works at least. If anyone knows how to fix this that would be great.<br>

Partially used code from https://github.com/SwagSoftware/Kisak-Strike (some econ stuff, weapon recoil).
Forked from https://github.com/SourceSDK2013Ports/csgo-src.

Features:
- Removed Scaleform;
- Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc);
- Some VGUI stuff and complete GameUI ported from TF2 leak;
- CSGO Usermessage system, everything now makes use of the CUsrMsg class.

Currently known bugs:
- sv_pure is most likely broken due to differences between CS:GO and TF2 implementations;
- Game currently crashes after shutting down (something related to g_pCVar and outputting console messages,
  I think g_pCVar gets destructed before it is used for outputting console messages or something);
