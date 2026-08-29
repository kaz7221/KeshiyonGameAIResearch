#ifndef CLION_CPP_GAME_MODES_H
#define CLION_CPP_GAME_MODES_H
#pragma once

#include "rule.h"
#include "state_utils.h"
#include "game_engine.h"
#include <unordered_map>
#include <fstream>

namespace game_modes
{
    namespace
    {

        int play_game_internal(const individual& circle_ind, const individual& x_ind, int ai_depth)
        {
            GameState state;
            unordered_map<string, int> seen_states;
            seen_states.reserve(256);
            int loop_counter = 0;

            while (true)
            {
                search_canblocked_place(state);

                if (state.canblocked_size == 0)
                {
                    // 置けるマスがない → 終了
                    break;
                }

                int x, y;
                if (state.is_circle_turn)
                {
                    auto move = minimax( ai_depth, state, true, circle_ind);
                    x = move.first;
                    y = move.second;
                }
                else
                {
                    auto move = minimax( ai_depth, state, false, x_ind);
                    x = move.first;
                    y = move.second;
                }

                if (x == -1 || y == -1)
                    break;

                // §1.4.3 の処理順序に従った統合的な手の処理
                GameResult result = process_move(state, x, y);

                // §1.5.2 千日手判定
                if (check_repeated_state(seen_states, state))
                {
                    return 1; // Draw
                }

                if (++loop_counter > 2000)
                {
                    break;
                }

                if (result != GameResult::IN_PROGRESS)
                    break;
            }

            if (state.points.first > state.points.second)
                return 2; // 〇 wins
            if (state.points.first < state.points.second)
                return 0; // X wins
            return 1; // Draw
        }
    }

    // Play a single game for GA evaluation
    inline int play_game(const individual& circle_ind, const individual& x_ind, int ai_depth)
    {
        return play_game_internal(circle_ind, x_ind, ai_depth);
    }

    inline void log_generation(ofstream& ofs, int generation, const vector<pair<int, individual>>& population)
    {
        long long sum = 0;
        for (const auto& p : population) sum += p.first;
        const double avg = static_cast<double>(sum) / static_cast<double>(population.size());
        ofs << "generation=" << generation
            << ", best=" << population.front().first
            << ", worst=" << population.back().first
            << ", avg=" << avg << '\n';
    }
}

#endif
