# SPCMechanic

Simple PS2 shmup-like game with a puzzle twist.
Made initially for [Global Game Jam 2020](https://globalgamejam.org/2020/games/spcmechanic-9)

## How to play it

### On real hardware:
Either burn the iso to a disc or copy everything (excluding the iso) to a usb flash drive and boot it using uLaunchELF.
Both methods require a way of running homebrew on the console.

### On PCSX2:
Either load up the ISO and select System->Boot ISO (fast) or Run ELF and select `spcmechanic.elf`.
The run ELF option may not work on PCSX2 1.4 or earlier, and the Boot ISO (full) option results in an error.

## Note:
This currently does not work on early PS2 BIOSes due to lack of LIBSD in it. Make sure you use a newer PS2 BIOS, or run this on a newer console.
Tested on SCPH-75004.
