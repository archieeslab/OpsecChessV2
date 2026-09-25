# OpsecChess V2

Offline Windows chess game written in C++17 with native Win32/GDI.

## Features

- Local two-player chess
- Play vs AI
- Easy, Medium, Hard and Expert difficulty
- Check / checkmate / stalemate
- Castling and en passant
- Pawn promotion
- Fifty-move rule
- Threefold repetition
- Insufficient-material draw
- Move history
- Move, capture and special-move highlighting
- Settings, audio toggle, About and Credits pages
- Embedded chess icon for the EXE, window, taskbar and Task Manager
- No networking, accounts, telemetry or cloud features

## Project structure

```text
OpsecChess/
├── assets/
│   └── icons/
│       ├── chess_piece.png
│       └── OpsecChess.ico
├── src/
│   └── main.cpp
├── .gitignore
├── LICENSE
├── OpsecChess.rc
├── OpsecChess.sln
├── OpsecChess.vcxproj
└── README.md
```

## Build

1. Open `OpsecChess.sln` in Visual Studio 2022.
2. Install the `Desktop development with C++` workload if needed.
3. Select x64.
4. Build and run.

No third-party packages are required.

## Credits

**Archie B. - Developer**


## V2 Interface

- Full-screen borderless presentation
- Animated startup/loading sequence
- Scaled interface for the current display
- Dark cinematic design with gold accents
- Hover-responsive controls
- Game-style home screen and feature card
- ESC returns to the menu; ESC on the menu exits
- Credits: Archie B. - Developer


## V3 graphics/audio

- Native-resolution back buffer instead of stretching a low-resolution frame
- Code-drawn vector chess pieces instead of font glyphs
- Cleaner move dots, capture rings and special-move markers
- Local WAV sound effects for UI, moves, captures and game events
- No Windows MessageBeep sounds

## Branding

V2 OpsecChess branding and icon set retained. V3 changes are graphics/rendering/audio improvements only.
