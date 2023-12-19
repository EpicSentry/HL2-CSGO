# Half-Life 2 in the CSGO engine (Atleast, an attempt of doing so)

This project is aimed to improve the leaked CS:GO code, to make it more stable and to make it playable.
Use resources from CS:S to get the HUD and GameUI to work properly.

Partially used code from https://github.com/SwagSoftware/Kisak-Strike (some econ stuff, weapon recoil).
Forked from https://github.com/SourceSDK2013Ports/csgo-src.

Features:
- Removed Scaleform;
- Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc);
- Some VGUI stuff and complete GameUI ported from TF2 leak;

Currently known bugs:
- sv_pure is most likely broken due to differences between CS:GO and TF2 implementations;
- Game currently crashes after shutting down (something related to g_pCVar and outputting console messages,
  I think g_pCVar gets destructed before it is used for outputting console messages or something);
