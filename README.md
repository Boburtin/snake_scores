
# Snake Highscore Database Server

This project aims to manage the submission/retrieval of user scores for a snake game. It's extremely rudimentary at the moment, as most of my work thus far has been learning Winsock2 and sqlite3.

## Building

- Requires at least GCC 14

- Uses UCRT64 and GNU Make

To build:

```bash
make
```

To clear bin/, build/, and scores.db:

```bash
make clean
```

## Dependencies

- SQLite
