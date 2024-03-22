# Half-Life 2 in the CSGO engine (Atleast, an attempt of doing so)

## HELP NEEDED! Please see the bottom of the readme!

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

# Troubleshooting compiling
Having problems building the project? Make sure you have the following:<br>
**For VS 2022 users:**
- Windows 8.1 SDK: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/<br>
- MSVC 140 (2015 Build tools): Available under the "Individual Components" section of the Visual Studio Installer.<br>
- If VPC complains about a regkey for .vcxproj files in solutions, please run .reg file included in this repository and run VPC again.<br>
VS 2015 should work out of the box with no additional changes necessary.<br>

# Debugging the engine
- Set launcher_main as the startup project (if it isn't already) by right clicking it and pressing "Set as Startup Project".
- Right click launcher_main, go to properties and click on the debugging section. Set "Command" to point to your compiled hl2.exe (in the game folder).
- Set "Command Arguments" to "-game "hl2" -insecure -sw" (feel free to add more such as +sv_cheats 1).
- Press "Local Windows Debugger" at the top of Visual Studio to then launch the game and debug it.
![image](https://github.com/EpicSentry/HL2-CSGO/assets/82910317/22c2e32f-d4d8-4c91-be39-6ca73ebfa551)<br>
Above is an example of a correctly set up debugger.<br>

# Please help make this project better!
Currently, almost 8 times out of 10 when loading into ANY Half-Life 2 map, the vphysics dll will crash at the same spot. In either: ivp_calc_next_psi_solver.cxx @ line 323 OR ivp_hull_manager_macros.hxx @ line 20.<br>
I suspect this is to do with HL2 models as when a blank map with nothing in it is loaded, it does not crash. Please help! I've tried recompiling the models from the model sources that leaked and no luck there either.<br>
