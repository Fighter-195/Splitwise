# Splitwise (C++)

An expense-splitter written from scratch in modern C++ with a clean
object-oriented design. Add people to a group, log who paid for what (split
evenly, by exact amounts, or by percentage), and it works out **who owes whom** —
then reduces everything to the **minimum number of payments** needed to settle up.

The same engine drives **two interchangeable front-ends**:

- a **menu-driven CLI**, and
- a **desktop GUI** built with [Dear ImGui](https://github.com/ocornut/imgui)
  (Win32 + Direct3D 9).

That's the whole point of the design: the logic lives in an engine that has no
idea whether a terminal or a GUI window is driving it, so the interface can be
swapped without touching a line of the maths.

```
┌──────────────┐     ┌──────────────┐
│  CLI (text)  │     │  GUI (ImGui) │      two front-ends…
└──────┬───────┘     └──────┬───────┘
       └───────────┬────────┘
             ┌──────▼──────┐
             │   engine    │             …one shared engine
             │  (pure C++) │
             └─────────────┘
```

## What it does

- **Groups & members** — a named group holding people.
- **Three ways to split a bill**
  - *equal* — divide evenly among the chosen people
  - *exact* — each person owes a specific amount
  - *percentage* — each person owes a share of the total (must sum to 100%)
- **Balances** — the net position of every person (owed / owes), always summing
  to zero.
- **Minimal settlement** — a greedy algorithm that settles all debts in the
  fewest transactions (repeatedly match the biggest debtor to the biggest
  creditor).
- **Persistence** — save and load a group to a plain CSV text file, with input
  validation throughout.

## The GUI

The Dear ImGui front-end (`gui/main_gui.cpp`) presents the whole workflow on one
screen:

- **Top bar** — rename the group; save/load it to a CSV file.
- **Left column** — add members; build an expense (pick the payer, choose a
  split type, tick participants, and — for exact/percentage splits — type each
  person's amount or percentage).
- **Right column** — the live results: every expense (expand a row to see each
  person's share), the current balances (green = owed, red = owes), and the
  minimal set of payments to settle up.
- **Status bar** — the result of the last action, or a validation message.

Everything recomputes every frame straight from the engine, so balances and the
settle-up plan update the instant you add an expense.

### Why Win32 + DirectX 9?

DirectX 9 and the Win32 API ship with Windows and link out-of-the-box under
MinGW, so the GUI builds with **no external libraries to download or install** —
unlike the more common (but heavier) GLFW + OpenGL setup. Dear ImGui itself is
vendored in `gui/imgui/`, so the repository is self-contained.

## Layout

```
splitwise/
├── engine/            the "brain" — pure logic, no printing
│   ├── User            a person
│   ├── Expense         abstract base + EqualSplit / ExactSplit / PercentSplit
│   ├── Group           owns members and expenses
│   └── Ledger          computes balances + minimal settlement
├── storage/
│   └── CsvStore        save / load a Group to a CSV file
├── cli/
│   └── main.cpp        menu-driven terminal front-end (pure "view")
└── gui/
    ├── main_gui.cpp    Dear ImGui desktop front-end (pure "view")
    ├── win32_compat.h  shim for old MinGW.org headers (see "Building the GUI")
    └── imgui/          vendored Dear ImGui + Win32/DX9 backend
```

### The OOP idea at the core

Different bills split differently, but the `Ledger` never asks *"what kind of
split is this?"*. Every expense answers one polymorphic question about itself:

```cpp
virtual double shareFor(int userId) const = 0;   // "how much does this user owe me?"
```

`EqualSplit`, `ExactSplit`, and `PercentSplit` each override it with their own
maths. The `Ledger` just calls `shareFor(...)` and the right calculation happens
automatically — polymorphism doing real work, not a toy `if/else`. Adding a new
kind of split is a new subclass; nothing else changes.

## Build & run

Needs a C++14 compiler. Tested with MinGW g++ 6.3 on Windows.

### CLI (any platform)

```sh
make            # or: g++ -std=c++14 -o splitwise engine/*.cpp storage/*.cpp cli/main.cpp
make run        # build and launch
```

### GUI (Windows)

```sh
make gui        # builds ./splitwise-gui.exe
make run-gui    # build and launch
```

The GUI links only against libraries that ship with Windows (`d3d9`, `gdi32`,
`imm32`), so nothing extra needs installing. Object files land in `build/` so the
(large) Dear ImGui sources only compile once.

#### A note on old MinGW headers

The GUI is developed on **MinGW.org g++ 6.3**, whose Win32 headers predate a few
symbols the modern ImGui backend expects. The build handles this without
patching ImGui's logic:

- `-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD` — skips the XInput gamepad path
  (no `<xinput.h>` on this toolchain).
- `-include gui/win32_compat.h` — supplies the handful of missing symbols
  (`TME_NONCLIENT`, `GET_XBUTTON_WPARAM`, `RTL_OSVERSIONINFOEXW`).

Both are **no-ops on a modern mingw-w64 / MSVC toolchain**, so the project builds
cleanly there too.

## Example

```
Goa Trip: Alice, Bob, Carol
  Dinner  — Alice paid 300, split equally      -> each owes 100
  Taxi    — Bob paid 90, split exactly 30 each

Balances:   Alice is owed 170.00
            Bob owes 40.00
            Carol owes 130.00

Settle up:  Carol -> Alice : 130.00
            Bob   -> Alice : 40.00
```

## File format

One record per line; the first field is the record type:

```
GROUP,<name>
USER,<id>,<name>
EXPENSE,<TYPE>,<description>,<paidBy>,<total>,<payload>
```

The same file is read and written by both front-ends.

## Roadmap

- [x] Engine: users, groups, three split types, balances, minimal settlement
- [x] Menu-driven CLI with CSV save/load and input validation
- [x] Desktop GUI (Dear ImGui, Win32 + DX9) over the same engine
