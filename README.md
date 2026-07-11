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
> **Note:** The ANSI colour-coding is not possible to convey in this documentation. If you run the `cshell` binary, you will see that the path (between `:` and `>`) is in pink and the rest is in default terminal colours.

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

Trailing `&` operators intercept parsing boundaries to launch non-blocking background pipelines. Additionally, active foreground processes can be suspended dynamically by sending the `CTRL+Z` interrupt signal (`SIGTSTP`), transferring execution ownership from the terminal foreground to the tracking stack.

Job lifetimes are managed natively by a dedicated tracking engine that prints clean lifecycle status updates immediately before rendering the next prompt.

```sh
cshell:~> sleep 10 &
[1] 11754
cshell:~> sleep 100
^Z
[2]+  Stopped                 sleep 100
cshell:~> jobs
[1]   Running                    sleep 10 &
[2]+  Stopped                    sleep 100
cshell:~> bg 2
[2]+ sleep 100 &
cshell:~> # wait 10 seconds
[1]+  Done                    sleep 10 &
cshell:~> fg 2
sleep 100
# wait for it to end
cshell:~>
```

### 5. Environment Variable Expansion

Tokens prefixed with `$` are scanned, extracted, and resolved via process environment blocks at runtime prior to pipeline execution routing. This includes the unique tracking token `$?`, which dynamically expands to the 8-bit ASCII string representation of the most recently executed foreground pipeline's exit status.

As discussed in the [Native Built-in Commands section](#native-built-in-commands), it is possible to create environment variables through `export KEY=VALUE`.

```sh
cshell:~> export PROJECT_DIR=/tmp/test
cshell:~> cd $PROJECT_DIR
cshell:/tmp/test> false
cshell:/tmp/test> echo $?
1
```

### 6. Short-Circuiting Logical Chains

Pipelines can be conditionally chained using short-circuiting logical operators (`&&` and `||`). The execution handler evaluates the list sequentially, short-circuiting downstream execution paths as soon as a preceding pipeline's exit status violates the logical invariant.

```sh
cshell:~> echo "step 1" && false && echo "step 2 hidden" || echo "step 3 executed"
step 1
step 3 executed
```

> This example shows `&& command` will execute if the command before returns an exit status of `0` (hence why `&& echo "step 2 hidden"` was ignored) and `|| command` will only execute if the command before returns a non-zero exit status (`|| echo "step 3 executed"` ran because `false` returns a non-zero exit status). It is an illegal command to have dangling logical operators (i.e. at the end or at the start). 

### 7. Interactive Raw-Mode Line Editing & History

The prompt transitions the host terminal from standard line-buffered (canonical) mode into raw character-by-character input mode. This powers an internal ANSI escape state machine supporting mid-line gap character insertion/deletion and full terminal line erasure.

Additionally, a persistent 32-slot circular ring buffer tracks unique executed entries, allowing users to scroll sequentially through command histories via the UP and DOWN arrow keys.

```sh
cshell:~> echo mid-line text
echo mid-line text
# Pressing UP arrow pulls the previous command line into the buffer:
cshell:~> echo mid-line text
echo mid-line text
# User presses UP arrow once, LEFT arrow 13 times and then types "custom ":
cshell:~> echo custom mid-line text
custom mid-line text
```
> **Note:** Hotkeys such as `CTRL+L` instantly flush ANSI clear sequences (`\033[2J\033[3J\033[H`) to erase the screen viewport while cleanly preserving and re-rendering active mid-line text buffers at the top-left coordinate.

### 8. Context-Aware Tab Completion

Pressing the horizontal tab character (`\t`) intercepts input to calculate prefix matches using an independent scanning architecture.

If the token is the initial command word, `cshell` evaluates binary candidates across the system `PATH` environment array; if it is a trailing argument parameter, it scans local directory paths via file-system pointers

```sh
cshell:~/projects/cshell> ./t[TAB] # Completes inline to "./tests/"
cshell:~/projects/cshell> ./tests/[TAB] # Completes inline to "./tests/test_"
cshell:~/projects/cshell> ./tests/test_[TAB][TAB] # No single match on first try, prints options on second [TAB]
test_history.c      test_parser.c       test_executor.c     test_completion.... test_expansion.c... test_handler.c      
test_filereader.... test_terminal.c     test_prompt.c       test_tracker.c      test_linereader.... test_startup.c  
cshell:~/projects/cshell> ./tests/test_ # Re-prints line after listing options
cshell:~/projects/cshell> ffm[TAB] # Completes in-line to "ffmpeg "
```

### 9. Synchronous Startup Profiles

Upon process initialisation, `cshell` automatically resolves the user's home path via environment hooks to locate an optional `~/.cshellrc` startup file. If detected, the file is processed synchronously line-by-line via a unified file reader, executing workspace defaults and configuration variables directly within the main parent process before spinning up the interactive REPL.

```sh
# Inside ~/.cshellrc:
# export PROFILE=developer
# clear

cshell:~> echo $PROFILE
developer
```

---

## Project Architecture & Components

The design of `cshell` centers around absolute memory determinism and safe systems orchestration without third-party library wrappers.

### Epoch-Based Arena Allocation
To eliminate heap fragmentation, tracking overhead, and complex pointer bookkeeping, memory requested during tokenisation and pipeline generation is bound to a single execution epoch. Once a pipeline completes its execution lifecycle, the arena offset is reset to zero in a single, deterministic O(1) operation, bypassing individual node destruction loops.

### Rolling Descriptor Pipeline Loops
The multi-stage execution engine drives an N-stage rolling file descriptor allocation loop. Instead of generating a massive global matrix of pipes ahead of time, standard descriptors are created and rotated progressively. Child contexts copy active boundaries via `dup2`, while rigorous parent-process descriptor hygiene explicitly closes unused write ends to guarantee that pipelines never deadlock or leak descriptors.

### Hardened Signal Barriers & Synchronous Harvesting
`cshell` utilises signal blocking barriers around critical process forks and transferring process harvesting ownership entirely to a synchronous tracking engine. To cleanly support background execution control without deadlocks, every forked pipeline context invokes `setpgid` across parent and child scopes to establish distinct process groups.

The parent shell explicitly masks out terminal line disruption signals (`SIGTSTP`, `SIGTTIN`, `SIGTTOU`), gracefully surrendering and reclaiming foreground terminal device group boundaries using `tcsetpgrp()`. Fully intergrated process monitoring occurs cleanly via non-blocking status calls (`waitpid(..., WNOHANG | WUNTRACED | WCONTINUED)`) executed strictly at the top of the REPL loop, preserving the `SA_RESTART` integrity of interactive character-by-character input streams.

---

## Author

Created by [**WillEdgington**](https://github.com/WillEdgington)

📧 [**willedge037@gmail.com**](mailto:willedge037@gmail.com) &nbsp;|&nbsp; 🔗 [**LinkedIn**](https://www.linkedin.com/in/williamedgington/)
