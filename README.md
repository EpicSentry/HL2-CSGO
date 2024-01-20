# Half-Life 2 in the CSGO engine (Atleast, an attempt of doing so)

This project is aimed to port HL2 to the leaked CSGO engine while keeping the benefits of the CSGO branch (such as the better lighting).<br>

Partially used code from https://github.com/SwagSoftware/Kisak-Strike (some econ stuff, weapon recoil and IVP) and https://github.com/tupoy-ya/Kisak-Strike.

**Features:**
- Removed Scaleform.
- Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc)
- Some VGUI stuff and complete GameUI ported from TF2 leak.
- CSGO Usermessage system, everything now makes use of the CUsrMsg class.
- VPhysics DLL code included.
- 64-bit build support. (it only builds, literally only builds. it's very broken)<br>

**Currently known bugs:**
- sv_pure is most likely broken due to differences between CS:GO and TF2 implementations.
- In game console is really unreliable and can only be opened if the pause menu is opened first.
- Random vphysics.dll crashes.
- Flashlight doesn't work.
- Physics save/restore is broken, play with ``save_disable 1`` for the time being.
- 64-bit does not work, it crashes on the VGUI2 DLL for some unknown reason.
- If you are building x64 you will lose out on:<br>
HRTF Audio<br>
Steam Datagram Sockets<br>
JPEG support for GameUI<br>

**Using and building HL2GO:**
1. You must have the 2017 CSGO depot downloaded as you need a few resources from it.<br>
2. HL2GO can be built with VS2015 (and 2022 with 2015 build tools installed).<br>
3. After building, copy the ``matchmaking.dll`` from ``game/csgo/bin`` to ``game/hl2/bin`` and copy your HL2 files over. It's recommended you extract the VPK's as they override some of the assets that we'll be copying in the next steps.<br>
4. Then copy the ``materials/dev/`` folder from the CSGO depot into your HL2 folder, overwriting if necessary.<br>
5. Next copy the ``platform`` folder from the CSGO depot into your game folder.<br>
6. Delete the ``scenes.image`` file or rebuild it with the CSGO hlfaceposer.<br>
7. Delete the ``models/humans`` and ``models/weapons`` and any loose ragdoll models from ``models/`` in your HL2 folder or recompile them using the original sources and the CSGO studiomdl.<br>
8. Now you can play the game!<br>
