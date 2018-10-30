# GBEmu

![](resources/GBEmuLogo.png)

GBEmu (working title) is a Game Boy Emulator written in a C-style C++ (using some C++11 features like auto) and SDL2 (and a tiny bit of GTK3 on Linux). Some distinctive features include a ROM debugger, a rewind system and easy-to-use quick save and restore slots. Right now it works on Mac, Windows and Linux.  

## Usage

### Starting GBEmu
GBEmu can be run from a standard GUI file manager (e.g. Finder, Windows Explorer) or the command line.  When running from the GUI an open dialog will be prompt to open a Game Boy ROM from the `ROMs` folder in the `Home Diretory` (see the **Home Diretory**). A ROM can also be opened from the command line. This is the command line usage:
* `gbemu [-d] path_to_ROM`
	* `-d` -- Start with the debugger screen up and emulator paused.

### Controls
GBEmu supports both keyboard and controller input.  Any controller supported by SDL2 should be supported. Xbox One/360, PS4, PS3, controllers are included. A list can be found [here](https://github.com/gabomdq/SDL_GameControllerDB/blob/master/gamecontrollerdb.txt).  

Default emulator controls for US keyboards are in the *Keyboard* section below.  If you do not use a US keyboard, the keyboard controls are automatically determined by "translating" US keyboard mappings into your keyboard language. For example, the Game Boy A button on a standard French keyboard would be `=` instead of `/`. Both keyboard and controller controls can be changed in **config.txt**.

By default there is a bunch of functionality available on the keyboard that is not currently mapped to the controller (e.g. pausing emulation). However, any emulator action can be mapped to the controller in **config.txt**.

#### Keyboard
The following mappings are the default keyboard mappings for a US keyboard (they have changed since version 0.0.1):

- `W S A D` -- Up, Down, Left, and Right, respectively
- `/ .` -- A and B, respectively
- `Enter \` -- Start, Select, respectively
- `Left Arrow` -- Rewind
- `N` -- Next instruction when emulator is paused, including when a breakpoint is hit.
- `C` -- Continue from breakpoint.
- `H` -- Briefly display **GBEmu Home Directory** in the title bar
- `Ctrl (Command on Mac) - P` -- Pause and unpause emulation
- `Ctrl (Command on Mac) - R` -- Reset emulation
- `Ctrl (Command on Mac) - F` -- Toggle fullscreen
- `Ctrl (Command on Mac) - U` -- Mute sound
- `Ctrl (Command on Mac) - B` -- Launch debugger screen
- `Ctrl (Command on Mac) - N` -- Show emulator controls
- `0 to 9` -- Load saved state from slots 0 to 9
- `Ctrl (Command on Mac) - 0 to 9` -- Save current states 0 to 9

#### Controller
- `D-Pad or Left Analog Stick` -- Up, Down, Left, Right
- `A B` -- A, B respectively
- `Start Back (Options Touchpad Button for PS4)` -- Start, Select respectively
- `Left Bumper (L1 for PS4)` -- Rewind

### Config.txt

**config.txt** is a file that lives in the **GBEmu Home Directory** which contains configuration, such as keyboard and gamepad controls, for GBEmu.  Each line represents a different configuration.  When GBEmu starts up for the first time, this file is created and populated with default configuration.  Any changes made to this file will be reflected in GBEmu on the next launch. With one exception (see the "Key" option definition below), **config.txt** is case insensitive.

An example configuration line looks like the following:

`Up = Key w`

In this example, this tells GBEmu to map the `Up` button on the Game Boy to the W key.
The left hand side of the equals is called the **Config Option** and the right hand side is the **Config Value**.

Right now, there are 2 categories of **Config Options**, options that map controls and an option to set the initial size of GBEmu's window. The option to set the initial size accepts a number (more details about this in the table below). The following table are the supported **Config Options**:

| Config Option      | Description                                                                                                          | Example Usage                           |
|:-------------------|:---------------------------------------------------------------------------------------------------------------------|:----------------------------------------|
| `Up`               | Maps Game Boy up button                                                                                              | `Up = Key w, Gamepad Up`                |
| `Down`             | Maps Game Boy down button                                                                                            | `Down = Key s, Gamepad Down`            |
| `Left`             | Maps Game Boy left button                                                                                            | `Left = Key a, Gamepad Left`            |
| `Right`            | Maps Game Boy right button                                                                                           | `Right = Key d, Gamepad Right`          |
| `A`                | Maps Game Boy A button                                                                                               | `A = Key /, Gamepad A`                  |
| `B`                | Maps Game Boy B button                                                                                               | `B = Key ., Gamepad B`                  |
| `Start`            | Maps Game Boy Start button                                                                                           | `Start = Key Enter, Gamepad Start`      |
| `Select`           | Maps Game Boy Select button                                                                                          | `Select = Key \, Gamepad Back`          |
| `Rewind`           | Maps GBEmu's rewind functionality                                                                                    | `Rewind = Key Left, Gamepad LeftBumper` |
| `Mute`             | Mutes GBEmu                                                                                                          | `Mute = Key Ctrl-m`                     |
| `Pause`            | Pauses emulation                                                                                                     | `Pause = Key Ctrl-p`                    |
| `Reset`            | Reset emulation                                                                                                      | `Reset = Key Ctrl-r`                    |
| `FullScreen`       | Puts GBEmu in full screen                                                                                            | `FullScreen = Key Ctrl-f`               |
| `ShowHomePath`     | Flashes the **GBEmu Home Directory** on the title bar                                                                | `ShowHomePath = Key Ctrl-h`             |
| `ShowControls`     | Show all of the key and gamepad mappings in a pop up                                                                 | `ShowControls = Key Ctrl-n`             |
| `ShowDebugger`     | Brings up the ROM debugger                                                                                           | `ShowDebugger = Key Ctrl-b`             |
| `DebuggerStep`     | Steps an instruction in the debugger                                                                                 | `DebuggerStep = Key n`                  |
| `DebuggerContinue` | Continues to next breakpoint in debugger                                                                             | `DebuggerContinue = Key c`              |
| `ScreenScale`      | Determines how many times larger, in resolution, the GBEmu window is to an actual Game Boy screen, which is 160x144. | `ScreenScale = 4`                       |

The option that maps controls accept 2 types of **Config Values**:
 1. Key -- Represents a key on the keyboard. For example, `Key w` means the w key on the keyboard. International keys (e.g `ä` are supported). English letters are case insensitive. So `Key W` is the same as `Key w`, but not `Key Ä` is **NOT** the same as `Key ä`. In the case of non-English characters, the lower case version should always be used. Non-English keys are only the part of **config.txt** that is case sensitive. Number keys are NOT supported and are reserved for usage with the save state controls.

 2. Gamepad -- Represents a button on a gamepad. Names of the buttons correspond to an Xbox 360 gamepad, but can be used for most gamepads. For example `Gamepad X` means the X button on an Xbox 360 gamepad and also means the Square button on a PS4 gamepad.

 The following **Config Values** are supported for key mappings:


| Config Value                         | Meaning                                                                            |
|:-------------------------------------|:-----------------------------------------------------------------------------------|
| `Key [single character key]`         | References a letter or symbol on the keyboard. E.g. `Key a` or `Key /`             |
| `Key Enter`                          | Enter key (Return key on Mac)                                                      |
| `Key Backspace`                      | Backspace key                                                                      |
| `Key Tab`                            | Tab key                                                                            |
| `Key Spacebar`                       | Spacebar key                                                                       |
| `Key Up`                             | Up arrow key                                                                       |
| `Key Down`                           | Down arrow key                                                                     |
| `Key Left`                           | Left arrow key                                                                     |
| `Key Right`                          | Right arrow key                                                                    |
| `Key Ctrl-[any key config value]`    | Use the Ctrl key (Command on Mac) with a key E.g. `Key Ctrl-a` or `Key Ctrl-Enter` |
| `Key Command-[any key config value]` | Same as above. E.g. `Key Command-a` or `Key Command-Enter`                         |

 The following **Config Values** are supported for gamepad mappings (all are case insensitive):

| Config Value           | Meaning                              |
|:-----------------------|:-------------------------------------|
| `Gamepad Up`           | Up on D-Pad and Left Analog Stick    |
| `Gamepad Down`         | Down on D-Pad and Left Analog Stick  |
| `Gamepad Left`         | Left on D-Pad and Left Analog Stick  |
| `Gamepad Right`        | Right on D-Pad and Left Analog Stick |
| `Gamepad A`            | A Button                             |
| `Gamepad B`            | B Button                             |
| `Gamepad X`            | X Button                             |
| `Gamepad Y`            | Y Button                             |
| `Gamepad Start`        | Start Button                         |
| `Gamepad Back`         | Back Button                          |
| `Gamepad Guide`        | Guide (a.k.a Home) Button            |
| `Gamepad LeftBumper`   | Left Bumper (LB) Button              |
| `Gamepad RightBumper`  | Right Bumper (RB) Button             |
| `Gamepad LeftTrigger`  | Left Trigger (LT)                    |
| `Gamepad RightTrigger` | Right Trigger (RT)                   |


Anything that begins with a `//` is comment and is ignored by GBEmu.  Example:
```
Up = Key w //Maps up on the Game Boy to the W key
```

**Config Values** can be grouped together into one line separated by a comma.  For example, if we wanted to map the Game Boy A button to the / key, the gamepad A button and the gamepad left trigger, we could write it as:
```
A = Key /
A = Gamepad A
A = Gamepad RightTrigger
```
But, we can simplify it to:

```
A = Key w, Gamepad A, Gamepad RightTrigger
```


### Full Screen
GBEmu supports full screen mode.  By default on a US keyboard, it can be toggled using `Ctrl (Command on Mac) - F`.  It should be noted that notifications currently don't show in full screen mode.  So, for example, you won't get any message for saving a save state like you do in windowed mode.

An on-screen notification system will be added in the future.

### GBEmu Home Directory
The **GBEmu Home Directory** is a directory where GBEmu stores its save states and Battery RAM files.  This directory also contains the `ROMs` directory where you can store your ROMs.  It's not required to store them here, but the **Open ROM** screen will default to this directory.

This directory also contains a subdirectories for each ROM loaded into GBEmu.  Each subdirectory is named `<ROM Name>`. The subdirectory contains the save states and the Battery RAM File for that ROM.

When started up for the first time, you are prompted with instructions to choose or create a **GBEmu Home Directory**.  There are currently 3 options:

1. Choose a directory to be the **GBEmu Home Directory**.
2. Create a directory called `gbemu_home` in your user home directory (e.g. `/home/<user_name>` on Linux or `C:\Users\<user_name>` on Windows) and use that as the **GBEmu Home Directory**.
3. Before setting up GBEmu, set the chosen **GBEmu Home Directory** path to an environment variable called `GBEMU_HOME`.

If options `1.` and `2.` are chosen, then a file is created in the user home directory (e.g. `/home/<user_name>`) called `gbemu_home_path.txt` which stores the path of the **GBEmu Home Directory**.  The contents of this file can be changed before starting up GBEmu to change the **GBEmu Home Directory**.  Deleting this file will cause GBEmu to re-prompt you on start up to choose the **GBEmu Home Directory** again.

It's important to note that the **GBEmu Home Directory** must be writable by you.

### Rewind
GBEmu supports a rewind feature.  A state is recorded every 5 seconds. You can rewind up to 300 seconds into the past. Press `Left Arrow` on the keyboard or `Left Bumper` on the controller to rewind.  It's important to note that you can rewind before a loaded state.  So using rewind, you can essentially "undo" a save state load.

### Saving and Loading Save State
GBEmu has 10 save state slots to save states to. Each save state slot is unique to a given **ROM Name** (shown at the top of the emulator window). For example, if you save to slot 1 for Pokemon Red, it won't conflict with slot 1 on Super Mario Bros.  However, if you use a hacked Pokemon Red and that has the same ROM Name as the original Pokemon Red, those save slots will conflict.

To save to a slot hold Ctrl (Command on Mac) and hit the number of the slot you want to save to (0 to 9).  To load a save state, simply hit the number of the slot you want to restore.  As stated in the `Rewind` section, you can undo loading of a save state by rewinding.

### Battery RAM File
If the ROM you are running uses battery-backed RAM, then a RAM file for that ROM is maintained in the **GBEmu Home Directory**.  This file has the file name `<ROM_Name>.sav`. This is where in-game saves are stored (e.g. Pokémon).  Also, it is where the Real-Time Clock is persisted if the game supports that feature (e.g. Pokémon Silver). Like save states, each Battery RAM file is unique per ROM Name.  However, unlike save states, there is only on RAM file per ROM Name.

This `sav` files are the same format as the `sav` files in the BGB Game Boy emulator.  You can copy over a BGB `sav` file into the `<ROM_NAME>` directory in the **GBEmu Home Directory**.  You must rename the `sav` file to `<ROM_Name>.sav` for it to be picked up by GBEmu.

Referring back to save state slots, when a save state slot is loaded, the RAM file in gbemu_home is overwritten with the save state's own RAM data.  If this was done on accident, then you can use the rewind feature to undo it.  



### Debugger/Options Screen
GBEmu has a second screen where you can access certain settings and a simple ROM debugger.  By default on a US keyboard, you can access this screen by hitting `Ctrl (Command on Mac) - B` on the keyboard. From this screen you can do the following:

- Control and mute the volume
- Pause and Reset the emulator.
- Set breakpoints, including hardware breakpoint.
	- Can also break when setting or clearing certain bits.
		- For example: You can set it to break when the LCD is turned off, which is clearing bit 7 of address $FF40. To do this, set **When to break:** to `Bits are cleared` and set the **Value:** field to `80` and **Address:** to `FF40`.  The 7th bit is set in the **Value:** field so the debugger knows to listen for the clearing of the 7th bit on address $FF40.
- When breakpoints are set, you can enabled Debug Recording which allows you to do reverse debugging.  This feature is pretty slow right now, so only use it when your breakpoint is not too far away.
- Look at the disassembly.  Breakpoints can be set from the disassembler.
	- Can search source and go to address.
- Viewing the memory map.  Breakpoints can be set from the memory map.
- View the sprite and background tiles.
- View the status LCD Driver, including the current LCD Mode.
- View the status of each sound channel.
- Mute any sound channel.

## Compile
I have tested compilation on clang and gcc.  As of now it, doesn't work on MSVC because I use case ranges (sorry!).  The goal is to eventually have it compile on more than clang and gcc but that is not a priority right now.  There are warnings pertaining to integer conversion.  I will get rid of them in the future.
A copy of SDL2 is included for all supported platforms, so no need to manually install it.

### Mac
- Requirements:
	* clang  -- Tested on clang 9 and 10.
	- Tested on macOS Mojave.
- To compile:
 1. Go to the source directory in the Terminal.
 2. Run `cd mac`
 3. Run `./build.sh`
   - You can run `./build.sh help`for a list of options.
 4. The executable is located in the build directory

### Linux
- Requirements:
	* clang  -- Tested on clang 9.
	* gcc -- Tested on GCC 5.4 (which is what comes with Ubuntu 16.04).
	* GTK3
	* Install the sndio library on your favorite distro.
	* Tested on Arch Linux and Ubuntu 16.04 LTS.
- To compile:
 1. Install sndio and GTK3 libs
	- For example, on Ubuntu: `sudo apt install libsndio-dev libgtk-3-dev`
 2. Go to the source directory in the Terminal.
 3. Run `cd linux`
 4. Run `./build.sh`
   - You can run `./build.sh help` for a list of options.
 5. The executable is located in the build directory

### Windows
- Requirements:
	* clang -- Clang is required for Windows.  It does not yet support MSVC.
	* Tested on Windows 10.
- To compile:
	1. Go to the source directory in the CMD.
	2. Run `cd windows`
	3. Run `build`
   - You can run `build help` for a list of options.
	4. The executable is located in the build directory.

## Contact and Screenshots
This project is currently hosted on [GitHub](https://github.com/DanB91/GBEmu) and [Handmade Network](https://gbemu.handmade.network/).  I host the latest screenshots on my Handmade Network project page. If you have any comments or questions, feel free to post on the forums on Handmade Network or reach out to me directly at dan.bokser at gmail dot com or shoot me DM on Twitter at dbokser91.
