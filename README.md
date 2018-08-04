# GBEmu

GBEmu (working title) is a Game Boy Emulator written in a C-style C++ (using some C++11 features like auto) and SDL2 (and a tiny bit of GTK3 on Linux). Some distinctive features include a ROM debugger, a rewind system and easy-to-use quick save and restore slots.

Right now it works on Mac, Windows and Linux.  It should be noted it was primarily developed on Mac/Linux and then "ported" over to Windows.  I don't currently have prepackaged binaries, but they should be coming in the future.
## Usage
### Starting GBEmu
GBEmu can be run from a standard GUI file manager (e.g. Finder, Windows Explorer) or the command line.  When running from the GUI an open dialog will be prompt to open a Game Boy ROM from the `ROMs` folder in the `Home Diretory` (see the **Home Diretory**). A ROM can also be opened from the command line. This is the command line usage:
* `gbemu [-d] path_to_ROM`
	* `-d` -- Start with the debugger screen up and emulator paused.

### Controls
GBEmu supports both keyboard and controller input.  Any controller supported by SDL2 should be supported. Xbox One/360, PS4, PS3, controllers are included. A list can be found [here](https://github.com/gabomdq/SDL_GameControllerDB/blob/master/gamecontrollerdb.txt).  Right, now mappings are fixed, but customization will be coming in the future.  

There is a bunch of functionality available on the keyboard that is not currently mapped to the controller (e.g. pausing emulation).  More thought needs to be put into how it should map to the controller.
#### Controller
- `D-Pad or Left Analog Stick` -- Up, Down, Left, Right
- `A B` -- A, B respectively
- `Start Back (Options Touchpad Button for PS4)` -- Start, Select respectively
- `Left Bumper (L1 for PS4)` -- Rewind


#### Keyboard
The following mapping is based on the US QWERTY keyboard:

- `W S A D` -- Up, Down, Left, Right, respectively
- `/ .` -- A, B, respectively
- `Enter \` -- Start, Select, respectively
- `P` -- Pause and unpause emulation
- `Ctrl (Command on Mac) - R` -- Reset emulation
- `Ctrl (Command on Mac) - F` -- Toggle fullscreen
- `Left Arrow` -- Rewind
- `M` -- Mute sound
- `H` -- Briefly display **Home Directory** in the title bar
- `B` -- Launch debugger screen
- `0 to 9` -- Load saved state from slots 0 to 9
- `N` -- Next instruction when emulator is paused, including when a breakpoint is hit.
- `C` -- Continue from breakpoint.
- `Ctrl (Command on Mac) - 0 to 9` -- Save current states 0 to 9

### Full Screen
GBEmu supports full screen mode.  It can be toggled using `Ctrl (Command on Mac) - F`.  It should be noted that notifications currently don't show in full screen mode.  So, for example, you won't get any message for saving a save state like you do in windowed mode.

An on-screen notification system will be added in the future.

### Home Directory
The **Home Directory** is a directory where GBEmu stores its save states and Battery RAM files.  This directory also contains the `ROMs` directory where you can store your ROMs.  It's not required to store them here, but the **Open ROM** screen will default to this directory.

This directory also contains a subdirectories for each ROM loaded into GBEmu.  Each subdirectory is named `<ROM Name>`. The subdirectory contains the save states and the Battery RAM File for that ROM.

When started up for the first time, you are prompted with instructions to choose or create a **Home Directory**.  There are currently 3 options:

1. Choose a directory to be the **Home Directory**.
2. Create a directory called `gbemu_home` in your user home directory (e.g. `/home/<user_name>` on Linux or `C:\Users\<user_name>` on Windows) and use that as the **Home Directory**.
3. Before setting up GBEmu, set the chosen **Home Directory** path to an environment variable called `GBEMU_HOME`.

If options `1.` and `2.` are chosen, then a file is created in the user home directory (e.g. `/home/<user_name>`) called `gbemu_home_path.txt` which stores the path of the **Home Directory**.  The contents of this file can be changed before starting up GBEmu to change the **Home Directory**.  Deleting this file will cause GBEmu to re-prompt you on start up to choose the **Home Directory** again.

It's important to note that the **Home Directory** must be writable by you.

### Rewind
GBEmu supports a rewind feature.  A state is recorded every 5 seconds. You can rewind up to 300 seconds into the past. Press `Left Arrow` on the keyboard or `Left Bumper` on the controller to rewind.  It's important to note that you can rewind before a loaded state.  So using rewind, you can essentially "undo" a save state load.

### Saving and Loading Save State
GBEmu has 10 save state slots to save states to. Each save state slot is unique to a given **ROM Name** (shown at the top of the emulator window). For example, if you save to slot 1 for Pokemon Red, it won't conflict with slot 1 on Super Mario Bros.  However, if you use a hacked Pokemon Red and that has the same ROM Name as the original Pokemon Red, those save slots will conflict.

To save to a slot hold Ctrl (Command on Mac) and hit the number of the slot you want to save to (0 to 9).  To load a save state, simply hit the number of the slot you want to restore.  As stated in the `Rewind` section, you can undo loading of a save state by rewinding.

### Battery RAM File
If the ROM you are running uses battery-backed RAM, then a RAM file for that ROM is maintained in the gbemu_home directory.  This file has the file name `<ROM_Name>.sav`. This is where in-game saves are stored (e.g. Pokémon).  Also, it is where the Real-Time Clock is persisted if the game supports that feature (e.g. Pokémon Silver). Like save states, each Battery RAM file is unique per ROM Name.  However, unlike save states, there is only on RAM file per ROM Name.

This `sav` files are the same format as the `sav` files in the BGB Game Boy emulator.  You can copy over a BGB `sav` file into the `<ROM_NAME>` directory in the **Home Directory**.  You must rename the `sav` file to `<ROM_Name>.sav` for it to be picked up by GBEmu.

Referring back to save state slots, when a save state slot is loaded, the RAM file in gbemu_home is overwritten with the save state's own RAM data.  If this was done on accident, then you can use the rewind feature to undo it.  



### Debugger/Options Screen
GBEmu has a second screen where you can access certain settings and a simple ROM debugger.  You can access this screen by hitting the `B` key on the keyboard. From this screen you can do the following:

- Control and mute the volume
- Pause and Reset the emulator.
- Set breakpoints, including hardware breakpoint.
	- Can also break when setting or clearing certain bits.
		- For example: You can set it to break when the LCD is turned off, which is clearing bit 7 of address $FF40. To do this, set **When to break:** to `Bits are cleared` and set the **Value:** field to `80` and **Address:** to `FF40`.  The 7th bit is set in the **Value:** field so the debugger knows to listen for the clearing of the 7th bit on address $FF40.
- Look at the disassembly.  Breakpoints can be set from the disassembler.
	- Can search source and go to address.
- Viewing the memory map.  Breakpoints can be set from the memory map.
- View the sprite and background tiles.
- View the status LCD Driver, including the current LCD Mode.
- View the status of each sound channel.
- Mute any sound channel.

## Compile
I have tested compilation on clang and gcc.  As of now it, doesn't work on MSVC because I use case ranges (sorry!).  The goal is to eventually have it compile on more than clang and gcc but that is not a priority right now.  There are warnings pertaining to integer conversion.  I will get rid of them in the future.

### Mac
1. Requirements:
	* clang  -- Tested on clang 9.
	- Tested on MacOS High Sierra.
2. To compile:
 1. Go to the source directory in the Terminal.
 - Run `cd mac`
 - Run `./build.sh`
 - The executable is located in the build directory

### Linux
- Requirements:
	* clang  -- Tested on clang 9.
	* gcc -- Tested on GCC 5.4 (which is what comes with Ubuntu 16.04).
	* GTK3
	* Install SDL2 library on your favorite distro.
	* Tested on Arch Linux and Ubuntu 16.04 LTS.
- To compile:
 1. Install SDL2 and GTK3 libs
	- For example, on Ubuntu: `sudo apt install libsdl2-dev libgtk-3-dev`
 2. Go to the source directory in the Terminal.
 3. `make release`
 4. The executable is located in the build directory

### Windows
- Requirements:
	* clang -- Clang is required for Windows.  It does not yet support MSVC.
	* SDL2 is included in the source tree.
	* Tested on Windows 10.
- To compile:
	1. Go to the source directory in the CMD.
	* Run `cd windows`
	* Run `build`
	* The executable is located in the build directory.
