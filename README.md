# sb_nes

mini demo:

SMB:<br>
<img src="./assets/supermariobrosvideo.gif" alt="SMB gameplay" width="480" style="max-width: 100%;">

pacman:<br>
<img src="./assets/pacman.gif" alt="pacman gameplay" width="480" style="max-width: 100%;">

<br>
there is still no audio output stuff(APU).
Currently supports only iNES format ROMs with mapper 0 (NROM). NES 2.0 format and all bank-switching mappers (MMC1, MMC3, etc.) are not yet implemented.

deps: 
- [SDL3](https://github.com/libsdl-org/SDL) (need to be installed manually)
- [nob.h](https://github.com/tsoding/nob.h) (already in this repo)

to build this project, `nob.c` need to be build first, then just run it
```bash
cc ./nob.c -o nob

./nob
```

to run the emulator, just add path of the rom:
```bash
./build/sb_nes ./yourrom.nes
```

All of the test i get it from here https://github.com/christopherpow/nes-test-roms
