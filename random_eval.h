#ifndef CLION_CPP_RANDOM_EVAL_H
#define CLION_CPP_RANDOM_EVAL_H
#pragma once

#include "rule.h"
#include "state_utils.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace random_eval
{
    struct RandomEvalResult
    {
        int wins = 0;
        int draws = 0;
        int losses = 0;
    };

    inline pair<int, int> select_random_move(const GameState& state)
    {
        GameState s = state;
        search_canblocked_place(s);
        if (s.canblocked_size == 0)
        {
            return {-1, -1};
        }
        uniform_int_distribution<int> dist(0, s.canblocked_size - 1);
        return s.canblocked[dist(engine)];
    }

    // Play from an arbitrary starting state where the non-AI player is random
    inline int play_from_state_vs_random(const individual& ai_ind, bool ai_is_circle, int ai_depth, GameState start)
    {
        GameState state = start;
        unordered_map<string, int> seen_states;
        seen_states.reserve(256);

        int loop_counter = 0;
        while (true)
        {
            search_canblocked_place(state);
            if (state.canblocked_size == 0) break;

            pair<int,int> move;
            const bool ai_turn = (state.is_circle_turn == ai_is_circle);
            if (ai_turn)
            {
                move = minimax(ai_depth, state, state.is_circle_turn, ai_ind);
            }
            else
            {
                move = select_random_move(state);
            }

            if (move.first == -1) break;
            const GameResult result = process_move(state, move.first, move.second);
            if (check_repeated_state(seen_states, state)) return 1;
            if (++loop_counter > 2000) return 1;
            if (result != GameResult::IN_PROGRESS) break;
        }

        if (state.points.first == state.points.second) return 1;
        if (ai_is_circle) return state.points.first > state.points.second ? 2 : 0;
        return state.points.second > state.points.first ? 2 : 0;
    }

    inline int play_vs_random(const individual& ai_ind, bool ai_is_circle, int ai_depth)
    {
        GameState state;
        return play_from_state_vs_random(ai_ind, ai_is_circle, ai_depth, state);
    }

    // Generate start state for 2.3.1: each side makes moves_each random moves
    inline GameState generate_random_walk_start_231(int moves_each = 2)
    {
        GameState state;
        for (int m = 0; m < moves_each * 2; ++m)
        {
            search_canblocked_place(state);
            if (state.canblocked_size == 0) break;
            auto mv = select_random_move(state);
            if (mv.first == -1) break;
            process_move(state, mv.first, mv.second);
        }
        return state;
    }

    // Helper: find symmetric column move for a given column
    inline pair<int,int> symmetric_response_move(const GameState& state, int col)
    {
        int sym_col = 4 - col;
        // find lowest free row in sym_col
        for (int row = 0; row < state.freed; ++row)
        {
            if (state.board[sym_col][row] == ' ')
            {
                if (row == 0 || state.board[sym_col][row-1] != ' ') return {sym_col, row};
            }
        }
        // fallback: random move
        return select_random_move(state);
    }

    // Generate start state for 2.3.2: first plays 3 random moves, second responds symmetrically
    inline GameState generate_random_walk_start_232()
    {
        GameState state;
        for (int step = 0; step < 3; ++step)
        {
            // first (circle) random move
            search_canblocked_place(state);
            if (state.canblocked_size == 0) break;
            auto mv = select_random_move(state);
            if (mv.first == -1) break;
            process_move(state, mv.first, mv.second);

            // second (x) symmetric response to the last move's column
            // determine last move column: it's mv.first
            search_canblocked_place(state);
            if (state.canblocked_size == 0) break;
            auto resp = symmetric_response_move(state, mv.first);
            if (resp.first == -1) break;
            process_move(state, resp.first, resp.second);
        }
        return state;
    }

    // Enumerate all possible start states up to total_moves (total moves, not per-side)
    inline void enumerate_walk_states(const GameState& state, int moves_left, vector<GameState>& out)
    {
        if (moves_left == 0)
        {
            out.push_back(state);
            return;
        }
        GameState s = state;
        search_canblocked_place(s);
        if (s.canblocked_size == 0)
        {
            out.push_back(s);
            return;
        }
        for (int i = 0; i < s.canblocked_size; ++i)
        {
            GameState ns = s;
            process_move(ns, s.canblocked[i].first, s.canblocked[i].second);
            enumerate_walk_states(ns, moves_left - 1, out);
        }
    }

    inline pair<RandomEvalResult, RandomEvalResult> evaluate_against_random_walk_231_all(const individual& ai_ind, int ai_depth, int total_moves)
    {
        RandomEvalResult ai_first; // AI plays as circle
        RandomEvalResult ai_second; // AI plays as X

        vector<GameState> starts;
        GameState init;
        enumerate_walk_states(init, total_moves, starts);

        // deduplicate by serialized key
        unordered_set<string> seen;
        vector<GameState> unique_starts;
        unique_starts.reserve(starts.size());
        for (auto &s : starts)
        {
            const string key = serialize_state_for_cycle_detection(s);
            if (seen.insert(key).second)
                unique_starts.push_back(s);
        }

        for (const auto &s : unique_starts)
        {
            int r1 = play_from_state_vs_random(ai_ind, true, ai_depth, s);
            if (r1 == 2) ai_first.wins++;
            else if (r1 == 1) ai_first.draws++;
            else ai_first.losses++;

            int r2 = play_from_state_vs_random(ai_ind, false, ai_depth, s);
            if (r2 == 2) ai_second.wins++;
            else if (r2 == 1) ai_second.draws++;
            else ai_second.losses++;
        }
        return {ai_first, ai_second};
    }

    // Enumerate all symmetric start states for 2.3.2 (first plays 3 moves, second responds symmetrically)
    inline void enumerate_walk232_states(const GameState& state, int steps_left, vector<GameState>& out)
    {
        if (steps_left == 0)
        {
            out.push_back(state);
            return;
        }
        GameState s = state;
        search_canblocked_place(s);
        if (s.canblocked_size == 0)
        {
            out.push_back(s);
            return;
        }
        // iterate over possible first-player moves
        for (int i = 0; i < s.canblocked_size; ++i)
        {
            GameState ns = s;
            auto mv = s.canblocked[i];
            process_move(ns, mv.first, mv.second); // first's move
            // compute symmetric response on updated state
            auto resp = symmetric_response_move(ns, mv.first);
            if (resp.first == -1)
            {
                // fallback: if no symmetric legal move, just add state without response
                enumerate_walk232_states(ns, steps_left - 1, out);
            }
            else
            {
                process_move(ns, resp.first, resp.second);
                enumerate_walk232_states(ns, steps_left - 1, out);
            }
        }
    }

    inline pair<RandomEvalResult, RandomEvalResult> evaluate_against_random_walk_232_all(const individual& ai_ind, int ai_depth)
    {
        RandomEvalResult ai_first;
        RandomEvalResult ai_second; // left zeroed for compatibility; AI will always be evaluated as first (circle)
        vector<GameState> starts;
        GameState init;
        enumerate_walk232_states(init, 3, starts); // 3 first-player moves + 3 symmetric responses

        // deduplicate by serialized key
        unordered_set<string> seen;
        vector<GameState> unique_starts;
        unique_starts.reserve(starts.size());
        for (auto &s : starts)
        {
            const string key = serialize_state_for_cycle_detection(s);
            if (seen.insert(key).second)
                unique_starts.push_back(s);
        }

        for (const auto &s : unique_starts)
        {
            // Evaluate only with AI as first (circle). The opponent in walk232 is a rule-based, second-only agent.
            int r1 = play_from_state_vs_random(ai_ind, true, ai_depth, s);
            if (r1 == 2) ai_first.wins++;
            else if (r1 == 1) ai_first.draws++;
            else ai_first.losses++;
        }
        return {ai_first, ai_second};
    }

    // Original random evaluation (keep for compatibility)
    // Evaluate against pure random opponent, returning results when AI is first and when AI is second separately
    inline pair<RandomEvalResult, RandomEvalResult> evaluate_against_random(const individual& ai_ind, int ai_depth, int games_per_side)
    {
        RandomEvalResult ai_first; // AI as circle
        RandomEvalResult ai_second; // AI as X
        for (int i = 0; i < games_per_side; ++i)
        {
            const int as_circle = play_vs_random(ai_ind, true, ai_depth);
            if (as_circle == 2) ai_first.wins++;
            else if (as_circle == 1) ai_first.draws++;
            else ai_first.losses++;

            const int as_x = play_vs_random(ai_ind, false, ai_depth);
            if (as_x == 2) ai_second.wins++;
            else if (as_x == 1) ai_second.draws++;
            else ai_second.losses++;
        }
        return {ai_first, ai_second};
    }

}

#endif
