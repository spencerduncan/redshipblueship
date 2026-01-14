#pragma once

namespace Combo {

/**
 * Minimal game interface for Phase 1.
 *
 * This provides just enough abstraction to select and run either game.
 * Full game state manipulation (IGameState with health/magic/items)
 * will be added in Phase 2 when combo logic needs it.
 */
class IGame {
public:
    virtual ~IGame() = default;

    /**
     * Get the game's display name.
     */
    virtual const char* GetName() const = 0;

    /**
     * Get the game's short identifier (used for --game flag).
     */
    virtual const char* GetId() const = 0;

    /**
     * Initialize the game (load archives, set up subsystems).
     * Call once before Run().
     */
    virtual void Init(int argc, char** argv) = 0;

    /**
     * Run the game's main loop.
     * This typically doesn't return until the game exits.
     */
    virtual void Run() = 0;
};

} // namespace Combo
