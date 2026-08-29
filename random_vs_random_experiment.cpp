#include "rule.h"
#include "state_utils.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <unordered_map>

namespace
{
    constexpr int kTotalGames = 10000;

    struct Stats
    {
        int total = 0;
        int first_wins = 0;
        int second_wins = 0;
        int draws = 0;
    };

    int play_random_game(std::mt19937& rng)
    {
        GameState state;
        std::unordered_map<std::string, int> seen_states;
        seen_states.reserve(256);
        int loop_counter = 0;

        while (true)
        {
            search_canblocked_place(state);
            if (state.canblocked_size == 0) break;

            std::uniform_int_distribution dist(0, state.canblocked_size - 1);
            const auto [fst, snd] = state.canblocked[dist(rng)];
            const GameResult result = process_move(state, fst, snd);

            if (check_repeated_state(seen_states, state)) return 1; // draw
            if (++loop_counter > 2000) break;
            if (result != GameResult::IN_PROGRESS) break;
        }

        if (state.points.first > state.points.second) return 2; // circle wins
        if (state.points.first < state.points.second) return 0; // x wins
        return 1; // draw
    }
}

int main()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    Stats stats;
    stats.total = kTotalGames;

    for (int game = 0; game < kTotalGames; ++game)
    {
        const bool a_is_first = (game % 2 == 0);
        const int result = play_random_game(rng);

        if (result == 1)
        {
            ++stats.draws;
            continue;
        }

        const bool circle_wins = (result == 2);
        if (const bool first_wins = a_is_first ? circle_wins : !circle_wins) ++stats.first_wins;
        else ++stats.second_wins;
    }

    const double total = stats.total;
    const double first_rate = static_cast<double>(stats.first_wins) / total;
    const double second_rate = static_cast<double>(stats.second_wins) / total;
    const double draw_rate = static_cast<double>(stats.draws) / total;

    std::cout << "total_games: " << stats.total << '\n';
    std::cout << "first_wins: " << stats.first_wins << '\n';
    std::cout << "second_wins: " << stats.second_wins << '\n';
    std::cout << "draws: " << stats.draws << '\n';
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "first_win_rate: " << first_rate << '\n';
    std::cout << "second_win_rate: " << second_rate << '\n';
    std::cout << "draw_rate: " << draw_rate << '\n';
    std::cout << "second_minus_first_rate: " << (second_rate - first_rate) << '\n';

    return 0;
}
