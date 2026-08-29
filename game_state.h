#ifndef CLION_CPP_GAME_STATE_H
#define CLION_CPP_GAME_STATE_H
#pragma once

#include <vector>
#include <set>
#include <utility>
#include <cstring>
using namespace std;

enum class GameResult
{
    IN_PROGRESS = -1,
    DRAW = 0,
    CIRCLE_WINS = 1,
    X_WINS = 2
};

// Game state structure
struct GameState
{
    char board[5][6]{};
    bool is_circle_turn = true;
    int freed = 4;
    int empty_cells_count = 20;  // 5 * 4 = 20
    pair<int,int> canblocked[15];
    int canblocked_size = 0;
    pair<int,int> points = {0, 0};
    int d_count = 0; // 1.4.1 の D_t に相当。4マス以上揃えた回数の累積
    bool will_be_deleted = false; //三角が次ならfalse,そうでないならtrue
    // '^' はロックされたマス（駒を置けない）

    GameState() {
        memset(board, ' ', sizeof(board));
    }
};


#endif //CLION_CPP_GAME_STATE_H

