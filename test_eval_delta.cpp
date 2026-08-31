#include "game_state.h"
#include "rule.h"
#include "individual.h"
#include <random>
#include <iostream>

extern "C" float eval_state_for_test(const GameState& state, bool circle_player, const individual& ind);
extern "C" float eval_move_delta_for_test(const GameState& before, const GameState& after, bool circle_player, const individual& ind, int moved_x, int moved_y);

int main()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    const int N = 1000;
    int total_mismatch = 0;
    double max_err = 0.0;
    int freed_cases = 0;

    for (int i = 0; i < N; ++i)
    {
        GameState s;
        s.freed = 4;
        // play random 0..30 moves
        std::uniform_int_distribution moves_dist(0, 30);
        int moves = moves_dist(rng);
        for (int m = 0; m < moves; ++m)
        {
            search_canblocked_place(s);
            if (s.canblocked_size == 0) break;
            std::uniform_int_distribution<int> d(0, s.canblocked_size - 1);
            auto mv = s.canblocked[d(rng)];
            process_move(s, mv.first, mv.second);
        }
        // now pick a random legal move and test all legal moves
        search_canblocked_place(s);
        for (int mi = 0; mi < s.canblocked_size; ++mi)
        {
            auto mv = s.canblocked[mi];
            GameState before = s;
            auto pr = process_move_returned(s, mv.first, mv.second);
            GameState after = pr.first;
            float delta = eval_move_delta_for_test(before, after, true, individual(), mv.first, mv.second);
            float full_before = eval_state_for_test(before, true, individual());
            float full_after = eval_state_for_test(after, true, individual());
            float full_delta = full_after - full_before;
            float err = fabsf(delta - full_delta);
            if (err > 1e-5)
            {
                ++total_mismatch;
                if (err > max_err) max_err = err;
            }
            if (after.freed != before.freed) ++freed_cases;
        }
    }

    std::cout << "total_mismatch=" << total_mismatch << " max_err=" << max_err << " freed_cases_total=" << freed_cases << "\n";
    return 0;
}
