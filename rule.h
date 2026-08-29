#ifndef CLION_CPP_RULE_H
#define CLION_CPP_RULE_H
#pragma once

#include "game_state.h"
#include "individual.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <limits>
using namespace std;

inline int check_four_in_a_row(const GameState& state, pair<int, int> match[30], int& match_size)
{
    match_size = 0;
    bool in_match[5][6] = {false};
    int setted = 0;

    // 横方向: 4マス連続を一方向につき1回まで数える
    bool found_horizontal = false;
    for (int row = 0; row < state.freed; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            if (state.board[col][row] != ' ' && state.board[col][row] != '^' &&
                state.board[col][row] == state.board[col + 1][row] &&
                state.board[col][row] == state.board[col + 2][row] &&
                state.board[col][row] == state.board[col + 3][row])
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (!in_match[col + i][row]) {
                        in_match[col + i][row] = true;
                        match[match_size++] = {col + i, row};
                    }
                }
                found_horizontal = true;
            }
        }
    }
    if (found_horizontal) setted++;

    // 縦方向
    bool found_vertical = false;
    for (int col = 0; col < 5; ++col)
    {
        for (int row = 0; row <= state.freed - 4; ++row)
        {
            if (state.board[col][row] != ' ' && state.board[col][row] != '^' &&
                state.board[col][row] == state.board[col][row + 1] &&
                state.board[col][row] == state.board[col][row + 2] &&
                state.board[col][row] == state.board[col][row + 3])
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (!in_match[col][row + i]) {
                        in_match[col][row + i] = true;
                        match[match_size++] = {col, row + i};
                    }
                }
                found_vertical = true;
            }
        }
    }
    if (found_vertical) setted++;

    // 斜め方向（左上から右下）
    bool found_diag_down = false;
    for (int col = 0; col <= 1; ++col)
    {
        for (int row = 0; row <= state.freed - 4; ++row)
        {
            if (state.board[col][row] != ' ' && state.board[col][row] != '^' &&
                state.board[col][row] == state.board[col + 1][row + 1] &&
                state.board[col][row] == state.board[col + 2][row + 2] &&
                state.board[col][row] == state.board[col + 3][row + 3])
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (!in_match[col + i][row + i]) {
                        in_match[col + i][row + i] = true;
                        match[match_size++] = {col + i, row + i};
                    }
                }
                found_diag_down = true;
            }
        }
    }
    if (found_diag_down) setted++;

    // 斜め方向（右上から左下）
    bool found_diag_up = false;
    for (int col = 4; col >= 3; --col)
    {
        for (int row = 0; row <= state.freed - 4; ++row)
        {
            if (state.board[col][row] != ' ' && state.board[col][row] != '^' &&
                state.board[col][row] == state.board[col - 1][row + 1] &&
                state.board[col][row] == state.board[col - 2][row + 2] &&
                state.board[col][row] == state.board[col - 3][row + 3])
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (!in_match[col - i][row + i]) {
                        in_match[col - i][row + i] = true;
                        match[match_size++] = {col - i, row + i};
                    }
                }
                found_diag_up = true;
            }
        }
    }
    if (found_diag_up) setted++;

    return setted;
}

inline void search_canblocked_place(GameState& state)
{
    state.canblocked_size = 0;
    for (int col = 0; col < 5; col++)
    {
        // scan entire column from bottom to top; collect every empty cell that is placeable
        for (int row = 0; row < state.freed; row++)
        {
            if (state.board[col][row] == ' ')
            {
                if (row == 0 || state.board[col][row - 1] != ' ')
                {
                    if (state.canblocked_size < static_cast<int>(sizeof(state.canblocked)/sizeof(state.canblocked[0])))
                    {
                        state.canblocked[state.canblocked_size++] = {col, row};
                    }
                }
                // do NOT break; there may be additional placeable empties higher in this column
            }
        }
    }
}

inline void set_block(GameState& state, int x, int y)
{

    if (x < 0 || x >= 5 || y < 0 || y >= state.freed)
    {
        return;
    }

    if (state.is_circle_turn)
    {
        state.board[x][y] = 'o';
    }
    else
    {
        state.board[x][y] = 'x';
    }
    state.is_circle_turn = !state.is_circle_turn;
    state.empty_cells_count--;
}

inline void display_board(const GameState& state)
{
    cout << "\n=== Board ===" << endl;
    for (int row = state.freed - 1; row >= 0; row--)
    {
        cout << "Row " << row + 1 << ":   ";
        for (int col = 0; col < 5; col++)
        {
            cout << "[" << state.board[col][row] << "] ";
        }
        cout << endl;
    }
    cout << "Columns: [1] [2] [3] [4] [5]" << endl;
    cout << "Points: o=" << state.points.first << " x=" << state.points.second << endl;
    cout << "Empty cells: " << state.empty_cells_count << endl;
}

inline bool handle_scoring(GameState& state, const pair<int, int> match[30], int match_size, const int added_score)
{
    if (added_score == 0) return false;

    // PDF 1.3.1 の D_t は、4マス以上揃えた「得点発生ターン」の累積回数。
    // ここではそのステータスを局面キーに含めるため、得点処理発生時に記録する。
    state.d_count += 1;

    if (state.will_be_deleted)
    {
        bool to_delete_flag[5][6] = {false};
        for (int m = 0; m < match_size; ++m)
        {
            int mx = match[m].first;
            int my = match[m].second;
            to_delete_flag[mx][my] = true;
            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};
            for (int i = 0; i < 4; ++i)
            {
                int nx = mx + dx[i];
                int ny = my + dy[i];
                if (nx >= 0 && nx < 5 && ny >= 0 && ny < state.freed)
                {
                    if (state.board[nx][ny] == '^') to_delete_flag[nx][ny] = true;
                }
            }
        }

        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < state.freed; ++j)
            {
                if (to_delete_flag[i][j] && state.board[i][j] != ' ')
                {
                    state.board[i][j] = ' ';
                    state.empty_cells_count++;
                }
            }
        }
    }
    else
    {
        for (int m = 0; m < match_size; ++m)
        {
            int mx = match[m].first;
            int my = match[m].second;
            if (state.board[mx][my] != '^')
            {
                state.board[mx][my] = '^';
            }
        }
    }
    if (!state.is_circle_turn)
        state.points.first += added_score;
    else
        state.points.second += added_score;

    state.will_be_deleted = !state.will_be_deleted;
    return true;
}

// §1.4.3 処理順序に従った終了判定・上段解放
// 呼び出し前に §1.3.1 得点計算・§1.4.1 揃ったマスの処理は完了していること
inline GameResult check_end_and_expand(GameState& state)
{
    // §1.3.2 盤面が埋まることによる得点の獲得（§1.4.1の後に行われる）
    if (state.empty_cells_count == 0)
    {
        if (!state.is_circle_turn)
            state.points.first++;
        else
            state.points.second++;
    }

    // §1.4.2 上段解放の判定
    if (state.freed < 6 && state.points.first == state.points.second && state.empty_cells_count <= 2)
    {
        state.freed++;
        state.empty_cells_count += 5;
        return GameResult::IN_PROGRESS;
    }

    // §1.5 ゲーム終了判定
    // §1.5.1 置くマスがなくなることによる終了
    if (state.empty_cells_count == 0)
    {
        if (state.points.first > state.points.second)
            return GameResult::CIRCLE_WINS;
        if (state.points.first < state.points.second)
            return GameResult::X_WINS;
        return GameResult::DRAW;
    }

    return GameResult::IN_PROGRESS;
}

// 統一された手の処理：§1.4.3 の処理順序に従う
// 1. ブロック配置 2. 得点計算(§1.3.1) 3. 揃ったマスの処理(§1.4.1)
// 4. 盤面埋まり得点(§1.3.2) 5. 上段解放(§1.4.2) 6. 終了判定(§1.5)
inline GameResult process_move(GameState& state, const int x, const int y)
{
    set_block(state, x, y);
    pair<int, int> match[30];
    int match_size = 0;
    if (const int added_score = check_four_in_a_row(state, match, match_size))
    {
        handle_scoring(state, match, match_size, added_score);
    }
    return check_end_and_expand(state);
}

// minimax探索用：コピーを返す版
inline pair<GameState, GameResult> process_move_returned(GameState state, const int x, const int y)
{
    GameResult result = process_move(state, x, y);
    return {state, result};
}

extern pair<int, int> minimax(int depth, const GameState& state, bool is_circle_turn, const individual& ind, float max_time = 0.0f);


#endif //CLION_CPP_RULE_H
