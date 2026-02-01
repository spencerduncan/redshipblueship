# Error Handling Policy

## Principles

1. **No silent failures** - Every error must be surfaced
2. **Appropriate mechanisms** - Match error handling to context
3. **Predictable behavior** - Same patterns across codebase

## Patterns by Context

### C APIs (Cross-DLL boundaries)
- Return error codes (int or enum)
- Use out parameters for results
- Document error codes in header comments

### C++ Internals
- Use exceptions for programming errors (logic bugs)
- Use `std::optional<T>` for expected "not found" cases
- Use `std::expected<T, E>` (C++23) or custom `Result<T>` for recoverable errors

### Resource Acquisition
- Always use RAII (`ScopedLibrary`, smart pointers)
- Cleanup in destructors, not manual calls
- Document ownership transfers

### Logging
- Use libultraship logging, not `std::cerr`
- Debug output must be gated by log level
- No unconditional stderr in production code

## Anti-Patterns (Don't Do)

```cpp
// BAD: Silent null return
void* GetThing() { return nullptr; }

// BAD: Debug spam
std::cerr << "[DEBUG]" << x << std::endl;

// BAD: Swallowed exception
try { ... } catch (...) { }
```

## Examples

```cpp
// GOOD: C API with error code
int Combo_LoadGame(Game game, GameState* out_state) {
    if (game == Game::None) return COMBO_ERR_INVALID_GAME;
    // ...
    return COMBO_SUCCESS;
}

// GOOD: C++ with optional
std::optional<Entrance> FindEntrance(uint16_t id) {
    auto it = entrances.find(id);
    if (it == entrances.end()) return std::nullopt;
    return it->second;
}
```
