# Half-Life 2 in the CSGO engine (Atleast, an attempt of doing so)

This project is aimed to port HL2 to the leaked CSGO engine while keeping the benefits of the CSGO branch (such as the better lighting).<br>

Partially used code from https://github.com/tupoy-ya/Kisak-Strike and https://github.com/nillerusr/source-engine.

**Features:**
- Removed Scaleform.
- Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc)
- Some VGUI stuff and complete GameUI ported from TF2 leak.
- CSGO Usermessage system, everything now makes use of the CUsrMsg class.
- VPhysics DLL code included.
- CVAR culling disabled by default.

**Currently known bugs:**
- sv_pure is most likely broken due to differences between CS:GO and TF2 implementations.
- In game console is really unreliable and can only be opened if the pause menu is opened first.
- Random vphysics.dll crashes.
- Physics save/restore is broken, play with ``save_disable 1`` for the time being.
- For now you'll need to bind f (or your desired flashlight key) to ``toggleflashlight``.

**Differences/stuff removed in HL2GO:**<br>
Unfortunately, in order to get certain features to work or to prevent crashes, the following have been removed completely:<br>
- SourceTV/GOTV, a nightmare
- The map compile tools, they just crash.

**Using and building HL2GO:**
currently being reworked
