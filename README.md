# post-office
Implementation of Post Office semaphore task for VUT IOS course.

## Building
`gcc`, `make` and any version of Linux with full implementation of POSIX semaphores are required. This won't run on macOS due to usage of anonymous semaphores which are not supported in macOS.
1) Clone this repo
2) cd post-office
3) `make`

Additional debug information can be printed to `stderr`. To enable, run `make debug`.

## Running
### Usage
```
./proj2 NZ NU TZ TU F
```
### Arguments
- `NZ` - Amount of clients to serve (`NZ > 0`)
- `NU` - Amount of post office workers that are able to serve clients (`NU > 0`)
- `TZ` - Max sleep time of client, before he enters the office (`0 < TZ < 10000`)
- `TU` - Max sleep time of worker, before he starts working (`0 < TU < 100`)
- `F`  - Max time of post office being open (`0 < F < 10000`)

## Testing
Additional test scripts are in the `test` folder. Though, I don't really remember the usage of each and every of them, they do print usage information on error, so...
