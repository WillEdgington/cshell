# cshell

A zero-configuration POSIX command-line shell engineered in pure C, featuring dynamic multi-stage execution pipelines and persistent background job tracking.

Built as part of a [self-directed C curriculum](https://github.com/WillEdgington/c-curriculum).

---

## Build

Requires `gcc` and `make`. Once the repository is cloned from the remote, you can run these build commands:

**runtime builds:**
```bash
make setup_deps # Clone the dependency repos
make            # Compiles the executable cshell binary
```

**dev builds:**
```bash
make test       # Compiles and runs the test suite (./tests)
make debug      # Builds with debug symbols and sanitisers
```

**clean:**
```bash
make clean_deps # Cleans compiled binaries, .o, .a, and .d files (including compiled dependencies)
make clean      # Cleans compiled binaries, .o, and .d files (just for cshell)
```

---

## Project Structure

```
include/cshell/       # Public interface definitions
├── test_framework.h  # Basic test framework macros
├── executor.h        # Pipeline execution, forking, and descriptor hygiene
├── expansion.h       # Environment variable token identification and translation
├── parser.h          # Lexical analysis and Pipeline/Command AST generation
├── prompt.h          # Context-aware path formatting and ANSI escape logic
├── tracker.h         # Persistent background job tables and state interfaces
├── terminal.h        # Raw mode state changes, termios configuration, and teardown
├── history.h         # Fixed-capacity circular history ring buffer definitions
├── linereader.h      # ANSI escape state machine and horizontal inline editing
├── completion.h      # PATH binary and local directory autocomplete scanning
├── filereader.h      # Consolidated sequential line stream processing engine
├── handler.h         # Multi-pipeline short-circuit operator dispatcher
├── startup.h         # Boot configuration sequence initialisation (.cshellrc)
└── runtime.h         # Central shell runtime limits and exit state registry
src/                  # Core implementation (.c files)
tests/                # Per-module unit tests
```

---

## Native Built-in Commands

To prevent subshell forks from discarding state mutations, `cshell` routes session state management, directory adjustments, and job manipulation directly to internal functions executing within the parent process context.

|Command|Syntax|Operational Summary|
|---|---|---|
|`cd`|`cd [path]`|Mutates the current working directory via `chdir()`. Evaluates runtime path expansions (e.g., `cd $HOME`).|
|`export`|`export KEY=VALUE`|Updates or assigns variables in the session's active environment block via `setenv()`.|
|`clear`|`clear`|Transmits ANSI screen-erasure codes directly to standard output, resets tracking tabs, and returns status `0`.|
|`source`/`.`|`source <file>`|Streams and processes external script rows synchronously in-place, allowing commands to alter the active shell environment.|
|`jobs`|`jobs`|Lists the sequence ID, PID, operational state (`RUNNING`/`STOPPED`), and command text of all background entries.|
|`fg`|`fg <job_id>`|Restores terminal group ownership to a background task via `tcsetpgrp()`, sends `SIGCONT`, and blocks until foreground completion.|
|`bg`|`bg <job_id>`|Resumes a suspended background task in place by forwarding `SIGCONT` without altering foreground terminal group status.|
|`exit`|`exit`|Breaks the interactive REPL execution loop to initiate a graceful shell process teardown.|

---

## Core Interactive Features

### 1. Dynamic, Context-Aware Prompt

The prompt dynamically evaluates the user's workspace on every REPL iteration. To maintain screen real estate, it detects the `HOME` environment variable prefix and cleanly truncates it to a `~` shorthand while isolating the path visually using high-contrast ANSI colour-coding

```sh
cshell:/mnt/c/Users> cd $HOME
cshell:~> cd ./projects/cshell
cshell:~/projects/cshell>
```
> **Note:** The ANSI colour-coding is not possible to convey in this documentation. If you run the `cshell` binary, you will see that the path (between `:` and `>`) is in pink and the rest is in default terminal colours (white for me).

### 2. Multi-Stage Pipelines & Subshell Isolation

`cshell` orchestrates arbitrary N-stage pipelines. It distinguishes between isolated subshell execution and parent-process evaluation: single built-in commands run in-place to mutate the parent environment, whereas pipelined built-ins automatically cascade into isolated child subshells to protect parent state.

```sh
cshell:~> echo systems_programming | grep -o programming
programming
```

### 3. Stream Redirection Engine

External process execution safely handles low-level input (`<`) and output (`>`) file descriptor redirection, routing byte streams seamlessly across disk boundaries.

```sh
cshell:~> echo hello > source.txt # write "hello" to source.txt
cshell:~> cat < source.txt > destination.txt # "hello" copies to destination.txt
```

### 4. Persistent Background Job Tracking

Trailing `&` operators intercept parsing boundaries to launch non-blocking background pipelines. Job lifetimes are managed natively by a dedicated tracking engine that prints clean, lifecycle status updates immediately before rendering the next prompt.

```sh
cshell:~> sleep 10 &
[1] 11754
cshell:~> # wait 10 seconds
[1]+  Done                    sleep 10 &
```

### 5. Environment Variable Expansion

Tokens prefixed with `$` are scanned, extracted, and resolved via process environment blocks at runtime prior to pipeline execution routing, ensuring universal variable availability across both external binaries and internal built-ins.

As discussed in the [Native Built-in Commands section](#native-built-in-commands), it is possible to create environment variables through `export KEY=VALUE`.

```sh
cshell:~> export PROJECT_DIR=/tmp/test
cshell:~> cd $PROJECT_DIR
cshell:/tmp/test>
```

---

## Project Architecture & Components

The design of `cshell` centers around absolute memory determinism and safe systems orchestration without third-party library wrappers.

### Epoch-Based Arena Allocation
To eliminate heap fragmentation, tracking overhead, and complex pointer bookkeeping, memory requested during tokenisation and pipeline generation is bound to a single execution epoch. Once a pipeline completes its execution lifecycle, the arena offset is reset to zero in a single, deterministic O(1) operation, bypassing individual node destruction loops.

### Rolling Descriptor Pipeline Loops
The multi-stage execution engine drives an N-stage rolling file descriptor allocation loop. Instead of generating a massive global matrix of pipes ahead of time, standard descriptors are created and rotated progressively. Child contexts copy active boundaries via `dup2`, while rigorous parent-process descriptor hygiene explicitly closes unused write ends to guarantee that pipelines never deadlock or leak descriptors.

### Hardened Signal Barriers & Synchronous Harvesting
`cshell` utilises signal blocking barriers around critical process forks and transferring process harvesting ownership entirely to a synchronous tracking engine. Active background jobs are monitored via non-blocking `waitpid(..., WNOHANG)` inquiries executed strictly at the top of the REPL loop, preserving the `SA_RESTART` integrity of your user input streams.

---

## Author

Created by [**WillEdgington**](https://github.com/WillEdgington)

📧 [**willedge037@gmail.com**](mailto:willedge037@gmail.com) &nbsp;|&nbsp; 🔗 [**LinkedIn**](https://www.linkedin.com/in/williamedgington/)
