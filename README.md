# gpupanic

make your gpu crash for educational purposes

## ⚠️ WARNING

**THIS WILL CRASH YOUR GPU**

- screen goes black
- fans spin to 100%
- might need hard reboot
- use at your own risk

## why is there no exe?

unlike my other projects, i'm not providing prebuilt binaries for this one. if you can't compile it yourself, you probably shouldn't be running it. this tool can crash your gpu - make sure you understand what you're doing first.

## what it does

sends an infinite loop compute shader to your nvidia gpu. the gpu tries to run it forever and dies.

| mode | what happens | recovery |
|------|--------------|----------|
| `--safe` | ~2 sec hang | automatic |
| `--medium` | ~20 sec hang | automatic |
| `--nuclear` | infinite hang | hard reboot |

## building

needs visual studio / msvc

```batch
build.bat
```

## usage

```batch
# see your gpus
gpupanic.exe --list

# safe test (recommended first)
gpupanic.exe --safe

# longer test
gpupanic.exe --medium

# the real deal
gpupanic.exe --nuclear
```

## for full nuclear mode

```batch
# 1. disable TDR (as admin)
gpupanic.exe --disable-tdr

# 2. reboot

# 3. rip gpu
gpupanic.exe --nuclear
```

TDR is windows' thing that auto-recovers gpu hangs. gotta disable it for true panic.

## how to not lose your screen

if you have integrated gpu (intel/amd) + nvidia:

1. plug monitor into motherboard
2. run gpupanic
3. nvidia dies, igpu keeps display alive
4. watch the chaos

if only one gpu: set up remote access first

## tested on

- RTX series gpu
- should work on any nvidia with dx11

## license

MIT - do whatever

made by [Tymbark7372](https://github.com/Tymbark7372)

