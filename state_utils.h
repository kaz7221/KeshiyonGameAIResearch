#ifndef CLION_CPP_STATE_UTILS_H
#define CLION_CPP_STATE_UTILS_H
#pragma once

#include "rule.h"
#include <unordered_map>

//盤面を文字列化して、千日手検出用のキーを生成する
//得点は含めない
inline string serialize_state_for_cycle_detection(const GameState& state)
{
    string key;
    key.reserve(5 * 6 + 32);
    key += (state.is_circle_turn ? '1' : '0');
    key += (state.will_be_deleted ? '1' : '0');
    key += '|';
    key += to_string(state.freed);
    key += '|';
    key += to_string(state.points.first - state.points.second);
    key += '|';
    key += to_string(state.d_count % 2);
    key += '|';
    for (int r = 0; r < state.freed; ++r)
    {
        for (int c = 0; c < 5; ++c)
        {
            key += state.board[c][r];
        }
        key += ';';
    }
    return key;
}


// 同一局面の検出（千日手判定）
// PDF 1.5.2 では、同一盤面が 3 回出現すれば千日手成立とする。
inline bool check_repeated_state(unordered_map<string, int>& seen_states, const GameState& state)
{
    const string key = serialize_state_for_cycle_detection(state);
    seen_states[key]++;
    if (seen_states[key] >= 3)
    {
        return true; // State repeated
    }
    return false; // new State
}
#endif
