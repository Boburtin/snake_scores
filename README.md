
# Snake Highscore Database Server

This project aims to manage the submission/retrieval of user scores for a snake game. It's extremely rudimentary at the moment, as most of my work thus far has been learning Winsock2 and sqlite3.

## Building

- Requires at least GCC 14

- Uses UCRT64 and GNU Make

## Dependencies

- SQLite (mingw-w64-ucrt-x86_64-sqlite3)

- Sodium (mingw-w64-ucrt-x86_64-libsodium)

You can use the MSYS2 package manager, pacman, to install the SQLite and Sodium libraries' so that they're directly available to the linker without needing to create a libs folder and include them as source/a files. It's also probably not optimal to use the -static linker flag since it makes the binary massive.

On UCRT64, if you need the dependencies:

```bash
pacman -S mingw-w64-ucrt-x86_64-sqlite3
```

```bash
pacman -S mingw-w64-ucrt-x86_64-libsodium
```

To build:

```bash
make
```

To run:

```bash
make run
```

To clear artifacts:

```bash
make clean
```

Clear artifacts and build/run:

```bash
make clean && make run
```
