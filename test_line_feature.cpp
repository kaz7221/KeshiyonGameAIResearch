#include "game_state.h"
#include "individual.h"
#include <iostream>
#include <random>
#include <cmath>

extern "C" void print_line_feature_windows_for_test();
extern "C" float eval_state_for_test(const GameState& state, bool circle_player, const individual& ind);

int main()
{
    std::cout << "Listing windows and mappings:\n";
    print_line_feature_windows_for_test();

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> cell_dist(0, 3); // 0:empty 1:o 2:x 3:^

    individual ind;
    int equal_count = 0;
    int first_bad = -1;

    for (int t = 0; t < 100; ++t)
    {
        GameState a;
        a.freed = 6;

        int empty = 0;
        for (int x = 0; x < 5; ++x)
        {
            for (int y = 0; y < a.freed; ++y)
            {
                int v = cell_dist(rng);
                char c = (v == 0) ? ' ' : (v == 1 ? 'o' : (v == 2 ? 'x' : '^'));
                a.board[x][y] = c;
                if (c == ' ') ++empty;
            }
        }
        a.empty_cells_count = empty;

        // 鏡像を作る（board 以外は同一）
        GameState b = a;
        for (int x = 0; x < 5; ++x)
            for (int y = 0; y < a.freed; ++y)
                b.board[4 - x][y] = a.board[x][y];

        float va = eval_state_for_test(a, true, ind);
        float vb = eval_state_for_test(b, true, ind);

        if (std::fabs(va - vb) < 1e-3f) ++equal_count;
        else if (first_bad < 0)
        {
            first_bad = t;
            std::cout << "first mismatch at t=" << t
                      << " va=" << va << " vb=" << vb
                      << " diff=" << (va - vb) << "\n";
            for (int y = a.freed - 1; y >= 0; --y)
            {
                std::cout << "  A: ";
                for (int x = 0; x < 5; ++x) std::cout << '[' << a.board[x][y] << ']';
                std::cout << "   B: ";
                for (int x = 0; x < 5; ++x) std::cout << '[' << b.board[x][y] << ']';
                std::cout << "\n";
            }
        }
    }
    std::cout << "symmetry test: equal_count=" << equal_count << "/100" << std::endl;
    return 0;
}