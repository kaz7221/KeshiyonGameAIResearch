#include "game_state.h"
#include "rule.h"
#include <random>
#include <iostream>
#include <vector>
#include <chrono>
#include <bits/stdc++.h>
#include "state_utils.h"
using namespace std;


vector<pair<int,int>> gen_moves_old(const GameState& state)
{
    vector<pair<int,int>> out;
    for (int col = 0; col < 5; ++col)
    {
        for (int row = 0; row < state.freed; ++row)
        {
            if (state.board[col][row] == ' ')
            {
                if (row == 0 || state.board[col][row - 1] != ' ')
                {
                    out.emplace_back(col, row);
                }
                break; // original behavior: stop after first empty
            }
        }
    }
    return out;
}

vector<pair<int,int>> gen_moves_new(const GameState& state)
{
    vector<pair<int,int>> out;
    for (int col = 0; col < 5; ++col)
    {
        for (int row = 0; row < state.freed; ++row)
        {
            if (state.board[col][row] == ' ')
            {
                if (row == 0 || state.board[col][row - 1] != ' ')
                {
                    out.emplace_back(col, row);
                }
                // no break: collect all in column
            }
        }
    }
    return out;
}

int play_random_game_with_gen(function<vector<pair<int,int>>(const GameState&)> gen, std::mt19937 &rng, int &out_moves, int &out_expansions)
{
    GameState state;
    unordered_map<string,int> seen;
    seen.reserve(256);
    int loop_counter = 0;
    out_moves = 0;
    out_expansions = 0;
    while (true)
    {
        auto moves = gen(state);
        if (moves.empty()) break;
        uniform_int_distribution<int> dist(0, (int)moves.size()-1);
        auto mv = moves[dist(rng)];
        int prev_freed = state.freed;
        GameResult result = process_move(state, mv.first, mv.second);
        if (state.freed > prev_freed) ++out_expansions;
        // repetition
        if (check_repeated_state(seen, state)) { out_moves++; return 1; }
        if (++loop_counter > 2000) { out_moves++; return 1; }
        out_moves++;
        if (result != GameResult::IN_PROGRESS) break;
    }
    if (state.points.first > state.points.second) return 2;
    if (state.points.first < state.points.second) return 0;
    return 1;
}

void test_specific_hole()
{
    cout << "Test: hole detection" << endl;
    GameState s;
    s.freed = 6;
    // create column 0: row0 filled, row1 empty (hole), row2 filled
    s.board[0][0] = 'o';
    s.board[0][1] = ' ';
    s.board[0][2] = 'x';
    // ensure above cells empty
    for (int r=3;r<6;++r) s.board[0][r] = ' ';

    auto old_moves = gen_moves_old(s);
    auto new_moves = gen_moves_new(s);
    cout << "Old moves in col0:";
    for (auto &p: old_moves) if (p.first==0) cout << " ("<<p.first<<","<<p.second<<")";
    cout << "\nNew moves in col0:";
    for (auto &p: new_moves) if (p.first==0) cout << " ("<<p.first<<","<<p.second<<")";
    cout << endl;
}

void test_max15()
{
    cout << "Test: max 15 legal moves scenario" << endl;
    GameState s;
    s.freed = 6;
    for (int col=0; col<5; ++col)
    {
        for (int row=0; row<6; ++row)
        {
            if (row % 2 == 0) s.board[col][row] = 'o';
            else s.board[col][row] = ' ';
        }
    }
    auto moves = gen_moves_new(s);
    cout << "new moves count=" << moves.size() << " (expect 15)" << endl;
    // check GameState canblocked capacity
    GameState s2;
    size_t cap = sizeof(s2.canblocked)/sizeof(s2.canblocked[0]);
    cout << "GameState canblocked capacity=" << cap << endl;
}

int main()
{
    cout << "Running tests for canblocked generation" << endl;
    test_specific_hole();
    test_max15();

    // run 1000 random games each
    const int N = 1000;
    std::random_device rd;
    std::mt19937 rng(rd());

    long long total_moves_old = 0, total_moves_new = 0;
    int draws_old = 0, draws_new = 0;
    int expansions_old = 0, expansions_new = 0;

    for (int i=0;i<N;++i)
    {
        int moves, exp;
        int res_old = play_random_game_with_gen(gen_moves_old, rng, moves, exp);
        total_moves_old += moves;
        if (res_old == 1) draws_old++;
        expansions_old += exp;

        int moves2, exp2;
        int res_new = play_random_game_with_gen(gen_moves_new, rng, moves2, exp2);
        total_moves_new += moves2;
        if (res_new == 1) draws_new++;
        expansions_new += exp2;
    }

    cout << "\nAfter "<<N<<" random games:\n";
    cout << "OLD: avg_moves=" << (double)total_moves_old/N << ", draw_rate=" << (double)draws_old/N << ", avg_expansions=" << (double)expansions_old/N << "\n";
    cout << "NEW: avg_moves=" << (double)total_moves_new/N << ", draw_rate=" << (double)draws_new/N << ", avg_expansions=" << (double)expansions_new/N << "\n";
    return 0;
}
