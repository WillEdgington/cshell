# cshell

A Unix shell built as part of a [self-directed C curriculum](https://github.com/WillEdgington/c-curriculum).

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

## Author

Created by [**WillEdgington**](https://github.com/WillEdgington)

📧 [**willedge037@gmail.com**](mailto:willedge037@gmail.com) &nbsp;|&nbsp; 🔗 [**LinkedIn**](https://www.linkedin.com/in/williamedgington/)