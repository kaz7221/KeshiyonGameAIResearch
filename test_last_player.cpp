#include "game_state.h"
#include <iostream>

extern "C" int last_player_advantage_for_test(const GameState& state, bool circle_player);

int main()
{
    GameState s;
    s.freed = 4;
    s.empty_cells_count = 10;
    std::cout << "freed=4 => last_player_advantage: " << last_player_advantage_for_test(s, true) << " (circle)\n";

    s.freed = 5;
    s.empty_cells_count = 7;
    std::cout << "freed=5 => last_player_advantage: " << last_player_advantage_for_test(s, true) << " (circle)\n";

    s.freed = 6;
    s.empty_cells_count = 3;
    std::cout << "freed=6 => last_player_advantage: " << last_player_advantage_for_test(s, true) << " (circle)\n";

    return 0;
}
