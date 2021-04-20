# CPU core counts

This self-contained code sample shows you how to correctly detect physical core and logical processor counts on AMD processors. The sample also makes recommendations about the default thread pool size you should create for game initialization and game execution, on AMD processors.

That recommendation is guidance only. Always profile and fit the thread pool size appropriately for your game's uses, since overall processor performance in games is affected by many factors.

## How to build the sample on Windows

- Clone this repo and open a 64-bit Visual Studio Developer Command Prompt inside the [`windows`](windows) subdirectory.
- `cl.exe AMDCoreCount.cpp`
- `cl.exe` will build the code and generate a file called `AMDCoreCount.exe` which you can then run.

 ## Support and suggestions

If you spot a bug, need any guidance, or would like to see the samples evolve in a particular way, file an issue and we'll take a look!

## Changelog

- v1.0
  - Initial release

- v2.0
  - Removed the Windows XP compatibility. Windows 7 or greater is now required.
  - Added `RYZEN_CORES_THRESHOLD` to help document the thread pool sizing guidance code.
  - Show more information about the detected processor, including name and vendor.
  - More clarity that the thread pool sizes for game play and game initialization are guidance only: *always remember to profile!*

These self-contained code samples show you how to correctly detect physical and logical processor core counts.

## Supported operating systems

- [x] Windows 7
- [x] Windows 10

## Support and suggestions

If you spot a bug, or would like to see the samples evolve in a particular way, file an issue and we'll take a look!
