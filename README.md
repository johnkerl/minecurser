ms.c:  A Minesweeper clone with a character interface.  Uses vi
keystrokes (e.g. h, j, k, l) to move around the screen (using curses), so you
know it's got to be wonderful.

John R. Kerl
12/23/95

# Compiling and running
Compile with
```
  gcc ms.c -lcurses -o ms
```

Run with
```
  ./ms
```

For help:
```
  ./ms -h
```

# Usage
* Cursor motion as in vi: `h j k l 0 $ H M L`
* Set flags with `f`
* Step with `s`
* There is no question-mark-setting feature
* Control-C to quit early
* Carriage return to quit at end of game

# Screenshots

Start of game (blinking cursor invisible here):
```
 + - - - - - - - - - - - - - - - +
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 | . . . . . . . . . . . . . . . |
 + - - - - - - - - - - - - - - - +
 40/40
```

After one flag set and several steps:
```
 + - - - - - - - - - - - - - - - +
 | . . . . . 1     1 . . . . . . |
 | . . . . . 1     1 2 . . . . . |
 | . . . . . 1       2 . . . . . |
 | . . . . . 1       1 . . . . . |
 | . . . . . 2       1 2 3 . . . |
 | . . . . . 2           1 . . . |
 | . . . . . 2 1 1     1 2 . . . |
 | 1 1 2 1 1 1 # 1   1 2 . . . . |
 |           1 1 1   2 . . . . . |
 |             1 1 2 3 . . . . . |
 |             1 . . . . . . . . |
 | 2 2 1       2 . . . . . . . . |
 | . . 1       1 . . . . . . . . |
 | . . 3 1     1 2 3 . . . . . . |
 | . . . 1         1 . . . . . . |
 + - - - - - - - - - - - - - - - +
 39/40
```

