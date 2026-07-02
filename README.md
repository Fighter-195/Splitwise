# Splitwise (C++)

A command-line expense-splitter, written from scratch in modern C++ with a
clean object-oriented design. Add people to a group, log who paid for what
(split evenly, by exact amounts, or by percentage), and it works out **who owes
whom** — then reduces everything to the **minimum number of payments** needed to
settle up.

Think of the "settle up" screen in the Splitwise app, built as a small,
self-contained C++ program.

```
==== Splitwise (C++) — group: Goa Trip ====
  1) add member          5) show balances
  2) add expense         6) settle up (minimal payments)
  3) list members        7) save to file
  4) list expenses       8) load from file
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
  fewest transactions (match the biggest debtor to the biggest creditor).
- **Persistence** — save and load a group to a plain CSV text file, with input
  validation throughout.

## Design

The project is deliberately split into an **engine** (pure logic, no
input/output) and a **front-end** (the CLI). The engine has no idea whether a
terminal or a GUI is driving it — so the interface can be swapped without
touching the logic.

```
splitwise/
├── engine/        the "brain" — pure logic, no printing
│   ├── User        a person
│   ├── Expense     abstract base + EqualSplit / ExactSplit / PercentSplit
│   ├── Group       owns members and expenses
│   └── Ledger      computes balances + minimal settlement
├── storage/
│   └── CsvStore    save / load a Group to a CSV file
└── cli/
    └── main.cpp    menu-driven terminal front-end (pure "view")
```

### The OOP idea at the core

Different bills split differently, but the `Ledger` never asks *"what kind of
split is this?"*. Every expense answers one polymorphic question about itself:

```cpp
virtual double shareFor(int userId) const = 0;   // "how much does this user owe me?"
```

`EqualSplit`, `ExactSplit`, and `PercentSplit` each override it with their own
maths. The `Ledger` just calls `shareFor(...)` and the right calculation happens
automatically — polymorphism doing real work, not a toy `if/else`.

## Build & run

Needs a C++14 compiler (tested with MinGW g++ 6.3 on Windows).

```sh
make            # or: g++ -std=c++14 -o splitwise engine/*.cpp storage/*.cpp cli/main.cpp
make run        # build and launch
```

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

## Roadmap

- [x] Engine: users, groups, three split types, balances, minimal settlement
- [x] Menu-driven CLI with CSV save/load and input validation
- [ ] Optional GUI skin (Dear ImGui) over the same engine
