#include "rule.h"
#include "individual.h"
#include "game_state.h"
#include <vector>
#include <string>
#include <unordered_map>

// 3.1.2.1 と 3.1.2.2 を切り替えるためのコンパイル時フラグ。
// 0: 3.1.2.1 (リーチ特徴量) を使う
// 1: 3.1.2.2 (全ライン特徴量) を使う
// 切り替えはこの 1 行だけで可能。
#define USE_LINE_FEATURE_3122 0

namespace
{
    // PDF 3.1 では「揃えられそうな列」を特徴量にする方針を採る。
    // 実装としては、4マス方向の候補のうち、対象側が 3 マス以上並んでいて
    // まだ空きが残っているものを数える。これは単なる "即座に 4 連になるマス" の数ではなく、
    // 実際の評価関数としてはより安定する。
    int count_potential_reaches(const GameState& state, bool circle_player)
    {
        const char target = circle_player ? 'o' : 'x';
        int reach_count = 0;
        const vector<pair<int, int>> directions = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

        for (int x = 0; x < 5; ++x)
        {
            for (int y = 0; y < state.freed; ++y)
            {
                for (const auto& [dx, dy] : directions)
                {
                   int target_count = 0;
                   int empty_count = 0;
                   bool blocked = false;
                   for (int step = 0; step < 4; ++step)
                   {
                       const int nx = x + dx * step;
                       const int ny = y + dy * step;
                       if (nx < 0 || nx >= 5 || ny < 0 || ny >= state.freed)
                       {
                           blocked = true;
                           break;
                       }
                       const char cell = state.board[nx][ny];
                       if (cell == ' ')
                       {
                           ++empty_count;
                       }
                       else if (cell == target)
                       {
                           ++target_count;
                       }
                       else
                       {
                           blocked = true;
                           break;
                       }
                   }
                   if (!blocked && target_count >= 3 && empty_count > 0)
                   {
                       ++reach_count;
                   }
                }
            }
        }
        return reach_count;
    }

    int score_difference(const GameState& state, bool circle_player)
    {
        return circle_player
                   ? (state.points.first - state.points.second)
                   : (state.points.second - state.points.first);
    }

    int last_player_advantage_at_30(const GameState& state, bool circle_player)
    {
        // Corrected: consider future additions up to height 6 (remaining rows to add)
        const int remaining_until_full_height = (6 - state.freed) * 5; // additional cells that will be added if expanded to 6
        const bool circle_gets_last = ((state.empty_cells_count + remaining_until_full_height) % 2 == 1)
                                              ? state.is_circle_turn
                                              : !state.is_circle_turn;

        return circle_gets_last == circle_player ? 1 : -1; //1 if circle gets last move, -1 otherwise
    }

    // shared helpers for window keys and enumeration
    static string serialize_cells(const vector<pair<int,int>>& cells){
        string k;
        for (size_t i=0;i<cells.size();++i){
            if (i) k.push_back(';');
            k += to_string(cells[i].first);
            k.push_back(',');
            k += to_string(cells[i].second);
        }
        return k;
    }
    static vector<pair<int,int>> mirror_cells_func(const vector<pair<int,int>>& cells){
        vector<pair<int,int>> m(cells.size());
        for (size_t i=0;i<cells.size();++i) m[i] = {4 - cells[i].first, cells[i].second};
        return m;
    }
    static vector<pair<int,int>> reversed_cells(const vector<pair<int,int>>& cells){
        return vector<pair<int,int>>(cells.rbegin(), cells.rend());
    }
    static string canonical_key_cells(const vector<pair<int,int>>& cells){
        string s1 = serialize_cells(cells);
        string s2 = serialize_cells(reversed_cells(cells));
        auto m = mirror_cells_func(cells);
        string s3 = serialize_cells(m);
        string s4 = serialize_cells(reversed_cells(m));
        string minstr = s1;
        if (s2 < minstr) minstr = s2;
        if (s3 < minstr) minstr = s3;
        if (s4 < minstr) minstr = s4;
        return minstr;
    }
    static vector<vector<pair<int,int>>> enumerate_windows_for_freed(int freed){
        vector<vector<pair<int,int>>> out;
        const pair<int,int> dirs[4] = {{1,0},{0,1},{1,1},{1,-1}};
        for (int x=0;x<5;++x){
            for (int y=0;y<freed;++y){
                for (const auto &d: dirs){
                    int dx=d.first, dy=d.second;
                    vector<pair<int,int>> cells;
                    bool ok=true;
                    for (int step=0; step<4; ++step){
                        int nx = x + dx*step;
                        int ny = y + dy*step;
                        if (nx<0||nx>=5||ny<0||ny>=freed){ ok=false; break; }
                        cells.push_back({nx,ny});
                    }
                    if (!ok) continue;
                    out.push_back(cells);
                }
            }
        }
        return out;
    }

    static unordered_map<string, int> build_window_index_map()
    {
        unordered_map<string,int> map;
        const auto windows = enumerate_windows_for_freed(6);
        int next_idx = 0;
        for (const auto &cells : windows){
            string canon = canonical_key_cells(cells);
            if (map.find(canon) == map.end()){
                map[canon] = next_idx++;
            }
        }
        if (next_idx != 21) {
            cerr << "Warning: canonical window map produced " << next_idx << " indices (expected 21)\n";
        }
        return map;
    }
    static const unordered_map<string, int>& get_canon_map()
    {
        static const unordered_map<string, int> m = build_window_index_map();
        return m;
    }
    float line_feature_score_3122_in_window(const GameState& state, bool root_is_circle, const individual& ind,
                                           int min_x, int max_x, int min_y, int max_y)
    {
        const char my_piece = root_is_circle ? 'o' : 'x';
        const char enemy_piece = root_is_circle ? 'x' : 'o';
        const pair<int, int> dirs[4] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
        float score = 0.0f;

        const unordered_map<string,int>& canon_map = get_canon_map();


        for (int x = min_x; x <= max_x; ++x)
        {
            for (int y = min_y; y <= max_y; ++y)
            {
                for (const auto& d : dirs)
                {
                    const int dx = d.first;
                    const int dy = d.second;

                    vector<pair<int,int>> cells;
                    bool out=false;
                    for (int step=0; step<4; ++step){
                        int nx = x + dx*step;
                        int ny = y + dy*step;
                        if (nx<0 || nx>=5 || ny<0 || ny>=state.freed){ out=true; break; }
                        cells.emplace_back(nx,ny);
                    }
                    if (out) continue;

                    // compute canonical key based on full-board canon_map
                    string canon = canonical_key_cells(cells);
                    auto it = canon_map.find(canon);
                    if (it == canon_map.end()){
                        cerr << "Error: canonical key not found for window starting at ("<<x<<","<<y<<") dir=("<<dx<<","<<dy<<") canon="<<canon<<"\n";
                        assert(false && "canonical key lookup failed");
                    }
                    const int idx = it->second; // expected 0..20
                    const int MAX_IDX = 21;
                    if (idx < 0 || idx >= MAX_IDX) {
                        cerr << "Error: window index out of range: " << idx << "\n";
                        assert(false && "window index out of range");
                    }

                    int my_count = 0, enemy_count = 0;
                    bool has_triangle = false;
                    for (const auto &c : cells){
                        char cc = state.board[c.first][c.second];
                        if (cc == my_piece) ++my_count;
                        else if (cc == enemy_piece) ++enemy_count;
                        else if (cc == '^') { has_triangle = true; break; }
                    }
                    if (has_triangle) continue; // contribution 0
                    if (my_count > 0 && enemy_count > 0) continue; // blocked
                    if (my_count == 0 && enemy_count == 0) continue; // both empty -> contribute 0
                    if (my_count > 0)
                    {
                        if (my_count >= 4) continue; // handled elsewhere
                        score += ind.line_weights[idx][my_count];
                    }
                    else if (enemy_count > 0)
                    {
                        if (enemy_count >= 4) continue;
                        score -= ind.line_weights[idx][enemy_count];
                    }
                }
            }
        }
        return score;
    }

    float line_feature_score_3122(const GameState& state, bool root_is_circle, const individual& ind)
    {
        return line_feature_score_3122_in_window(state, root_is_circle, ind, 0, 4, 0, state.freed - 1);
    }

    float evaluate_state(const GameState& state, bool root_is_circle, const individual& ind)
    {
        const bool will_be_deleted = state.will_be_deleted;
        const float reach_diff_weight = will_be_deleted ? ind.reach_diff_weight_deleted : ind.reach_diff_weight_not_deleted;
        float score = ind.score_diff_weight * static_cast<float>(score_difference(state, root_is_circle)) +
            ind.can_place_last_weight * static_cast<float>(last_player_advantage_at_30(state, root_is_circle));

        // 3.1.2.1 と 3.1.2.2 の切り替えは、先頭の USE_LINE_FEATURE_3122 マクロを変えるだけで可能。
        // 3.1.2.1: リーチ数差を評価に使う。
#if USE_LINE_FEATURE_3122
        // 3.1.2.2: 全ラインの情報を評価に使う。
        score += line_feature_score_3122(state, root_is_circle, ind);
#else
        const int reach_diff = count_potential_reaches(state, root_is_circle) -
            count_potential_reaches(state, !root_is_circle);
        score += static_cast<float>(reach_diff) * reach_diff_weight;
#endif

        const char my_piece = root_is_circle ? 'o' : 'x';
        const char enemy_piece = root_is_circle ? 'x' : 'o';
        for (int i = 0; i < 5; i++)
        {
            int mapped_i = i > 2 ? 4 - i : i; // 0->0, 1->1, 2->2, 3->1, 4->0
            for (int j = 0; j < state.freed; j++)
            {
                if (state.board[i][j] == my_piece) {
                    score += ind.places_weight[mapped_i][j];
                } else if (state.board[i][j] == enemy_piece) {
                    score -= ind.places_weight[mapped_i][j];
                }
            }
        }
        return score;
    }

    float evaluate_move_delta(const GameState& before, const GameState& after, bool root_is_circle, const individual& ind,
                             int moved_x, int moved_y)
    {
        // If board height changed (freed differs), fall back to full evaluation difference to be safe.
        if (after.freed != before.freed)
        {
            const float before_val = evaluate_state(before, root_is_circle, ind);
            const float after_val = evaluate_state(after, root_is_circle, ind);
            return after_val - before_val;
        }

        const int min_x = max(0, moved_x - 3);
        const int max_x = min(4, moved_x + 3);
        const int min_y = max(0, moved_y - 3);
        const int max_y = min(before.freed - 1, moved_y + 3);

        float delta = 0.0f;
        delta += ind.score_diff_weight * (static_cast<float>(score_difference(after, root_is_circle)) -
            static_cast<float>(score_difference(before, root_is_circle)));
        delta += ind.can_place_last_weight * (static_cast<float>(last_player_advantage_at_30(after, root_is_circle)) -
            static_cast<float>(last_player_advantage_at_30(before, root_is_circle)));

        const char my_piece = root_is_circle ? 'o' : 'x';
        const char enemy_piece = root_is_circle ? 'x' : 'o';
        for (int x = min_x; x <= max_x; ++x)
        {
            const int mapped_x = x > 2 ? 4 - x : x;
            for (int y = min_y; y <= max_y; ++y)
            {
                const char before_cell = before.board[x][y];
                const char after_cell = after.board[x][y];
                if (before_cell == my_piece && after_cell != my_piece)
                    delta -= ind.places_weight[mapped_x][y];
                else if (before_cell == enemy_piece && after_cell != enemy_piece)
                    delta += ind.places_weight[mapped_x][y];
                else if (after_cell == my_piece && before_cell != my_piece)
                    delta += ind.places_weight[mapped_x][y];
                else if (after_cell == enemy_piece && before_cell != enemy_piece)
                    delta -= ind.places_weight[mapped_x][y];
            }
        }

        // 3.1.2.1 と 3.1.2.2 はここを切り替えればよい。
#if USE_LINE_FEATURE_3122
        delta += line_feature_score_3122_in_window(after, root_is_circle, ind, min_x, max_x, min_y, max_y) -
            line_feature_score_3122_in_window(before, root_is_circle, ind, min_x, max_x, min_y, max_y);
#else
        const int before_reach = count_potential_reaches(before, root_is_circle) -
            count_potential_reaches(before, !root_is_circle);
        const int after_reach = count_potential_reaches(after, root_is_circle) -
            count_potential_reaches(after, !root_is_circle);
        delta += static_cast<float>(after_reach - before_reach) *
            (before.will_be_deleted ? ind.reach_diff_weight_deleted : ind.reach_diff_weight_not_deleted);
#endif

        return delta;
    }

} // namespace

float seeker(GameState new_state, int depth, bool root_is_circle, const individual& ind,
             float alpha, float beta, float cached_eval = numeric_limits<float>::quiet_NaN())
{
    // Terminal/leaf handling
    if (depth <= 0 || new_state.empty_cells_count == 0)
    {
        // If the board is full, check whether the game truly ends (no expansion).
        if (new_state.empty_cells_count == 0)
        {
            GameState tmp = new_state;
            GameResult r = check_end_and_expand(tmp);
            if (r != GameResult::IN_PROGRESS)
            {
                // Terminal: return large signed value relative to root player
                const float WIN_SCORE = 1e5f;
                if (r == GameResult::CIRCLE_WINS)
                    return root_is_circle ? WIN_SCORE : -WIN_SCORE;
                if (r == GameResult::X_WINS)
                    return root_is_circle ? -WIN_SCORE : WIN_SCORE;
                // draw
                return 0.0f;
            }
            // else expansion occurred: not terminal, fall through to evaluation or cached eval
        }

        if (isnan(cached_eval))
        {
            return evaluate_state(new_state, root_is_circle, ind);
        }
        return cached_eval;
    }

    search_canblocked_place(new_state);

    const bool maximizing = (new_state.is_circle_turn == root_is_circle);

    // 手を事前に展開してソート — 差分評価を廃止し、各着手を evaluate_state で評価する
    pair<float, pair<int,int>> moves[5];
    int moves_count = 0;
    
    for (int i = 0; i < new_state.canblocked_size; ++i)
    {
        const auto& p = new_state.canblocked[i];
        const GameState next = process_move_returned(new_state, p.first, p.second).first;
        const float score = evaluate_state(next, root_is_circle, ind);
        moves[moves_count++] = {score, p};
    }

    // 挿入ソート（要素数が最大5のためstd::sortより高速）
    if (maximizing)
    {
        for (int i = 1; i < moves_count; ++i) {
            auto key = moves[i];
            int j = i - 1;
            while (j >= 0 && moves[j].first < key.first) {
                moves[j + 1] = moves[j];
                j = j - 1;
            }
            moves[j + 1] = key;
        }
    }
    else
    {
        for (int i = 1; i < moves_count; ++i) {
            auto key = moves[i];
            int j = i - 1;
            while (j >= 0 && moves[j].first > key.first) {
                moves[j + 1] = moves[j];
                j = j - 1;
            }
            moves[j + 1] = key;
        }
    }

    float best_score = maximizing ? numeric_limits<float>::lowest() : numeric_limits<float>::max();

    for (int i = 0; i < moves_count; ++i)
    {
        const auto& [score, p] = moves[i];
        const GameState next_state = process_move_returned(new_state, p.first, p.second).first;
        const float s = seeker(next_state, depth - 1, root_is_circle, ind, alpha, beta, numeric_limits<float>::quiet_NaN());
        if (maximizing)
        {
            best_score = max(best_score, s);
            alpha = max(alpha, s);
        }
        else
        {
            best_score = min(best_score, s);
            beta = min(beta, s);
        }
        if (alpha >= beta) break;
    }

    return best_score;
}

// helpers for new root policies
static inline int column_priority(int col)
{
    // priority order (smaller = better): B(1), C(2), D(3), A(0), E(4)
    const int order[5] = {1,2,3,0,4};
    for (int i = 0; i < 5; ++i) if (order[i] == col) return i;
    return 4;
}

static inline pair<int,int> choose_by_column_priority(const vector<pair<int,int>>& moves)
{
    pair<int,int> best = {-1,-1};
    int best_pr = 100;
    for (auto &m : moves)
    {
        int pr = column_priority(m.first);
        if (pr < best_pr)
        {
            best_pr = pr;
            best = m;
        }
    }
    return best;
}

pair<int, int> minimax(int depth, const GameState& state, bool is_circle_turn, const individual& ind,const float max_time)
{
    (void)max_time;
    assert(state.is_circle_turn == is_circle_turn);
    depth = min(depth, state.empty_cells_count);

    // collect legal root moves
    GameState tmp = state;
    search_canblocked_place(tmp);
    vector<pair<int,int>> legal_moves;
    legal_moves.reserve(tmp.canblocked_size);
    for (int i = 0; i < tmp.canblocked_size; ++i) legal_moves.push_back(tmp.canblocked[i]);

    // 1) immediate wins (root move that makes 4-in-a-row)
    vector<pair<int,int>> immediate_wins;
    for (auto &m : legal_moves)
    {
        GameState s2 = state;
        s2.board[m.first][m.second] = is_circle_turn ? 'o' : 'x';
        pair<int,int> match[30]; int match_size = 0;
        if (check_four_in_a_row(s2, match, match_size) > 0)
            immediate_wins.push_back(m);
    }
    if (!immediate_wins.empty())
    {
        return choose_by_column_priority(immediate_wins);
    }

    // 2) opponent immediate wins -> try to block
    const char opp_char = is_circle_turn ? 'x' : 'o';
    vector<pair<int,int>> opp_immediate_wins;
    {
        GameState s3 = state;
        search_canblocked_place(s3);
        for (int i = 0; i < s3.canblocked_size; ++i)
        {
            auto m = s3.canblocked[i];
            GameState s4 = state;
            s4.board[m.first][m.second] = opp_char;
            pair<int,int> match[30]; int match_size = 0;
            if (check_four_in_a_row(s4, match, match_size) > 0)
                opp_immediate_wins.push_back(m);
        }
    }
    if (!opp_immediate_wins.empty())
    {
        vector<pair<int,int>> blockers;
        for (auto &m : legal_moves)
        {
            for (auto &om : opp_immediate_wins)
            {
                if (m == om) { blockers.push_back(m); break; }
            }
        }
        if (!blockers.empty()) return choose_by_column_priority(blockers);
        // fallthrough if no direct blocker (rare)
    }

    // 3) moves that create >=2 reaches: choose move with largest reach count, tie-break by column priority
    int best_reach = -1;
    vector<pair<int,int>> best_reach_moves;
    for (auto &m : legal_moves)
    {
        auto next_pair = process_move_returned(state, m.first, m.second);
        GameState next_state = next_pair.first;
        int reach_count = count_potential_reaches(next_state, is_circle_turn);
        if (reach_count > best_reach)
        {
            best_reach = reach_count;
            best_reach_moves.clear();
            best_reach_moves.push_back(m);
        }
        else if (reach_count == best_reach)
        {
            best_reach_moves.push_back(m);
        }
    }
    if (best_reach >= 2 && !best_reach_moves.empty())
    {
        return choose_by_column_priority(best_reach_moves);
    }

    // fallback: existing root ordering + seeker search
    GameState new_state = state;
    search_canblocked_place(new_state);

    pair<int,int> best = {-1, -1};
    float best_score = numeric_limits<float>::lowest();
    float alpha = numeric_limits<float>::lowest();

    pair<float, pair<int,int>> moves[5];
    int moves_count = 0;
    
    for (int i = 0; i < new_state.canblocked_size; ++i)
    {
        const auto& p = new_state.canblocked[i];
        GameState next = process_move_returned(new_state, p.first, p.second).first;
        float score = evaluate_state(next, is_circle_turn, ind);
        moves[moves_count++] = {score, p};
    }

    // root: descending by evaluate_state
    for (int i = 1; i < moves_count; ++i)
    {
        auto key = moves[i];
        int j = i - 1;
        while (j >= 0 && moves[j].first < key.first)
        {
            moves[j + 1] = moves[j];
            j = j - 1;
        }
        moves[j + 1] = key;
    }

    for (int i = 0; i < moves_count; ++i)
    {
        const auto& p = moves[i].second;
        GameState next_state = process_move_returned(new_state, p.first, p.second).first;
        const float s = seeker(next_state, depth - 1, is_circle_turn, ind, alpha, numeric_limits<float>::max());
        if (s > best_score)
        {
            best_score = s;
            best = p;
        }
        alpha = max(alpha, s);
    }

    return best;
}

// Test wrapper exported for verification
extern "C" int last_player_advantage_for_test(const GameState& state, bool circle_player)
{
    return last_player_advantage_at_30(state, circle_player);
}

// Expose evaluate_state for tests
extern "C" float eval_state_for_test(const GameState& state, bool circle_player, const individual& ind)
{
    return evaluate_state(state, circle_player, ind);
}


// Expose a function to print windows and mapping for verification
extern "C" void print_line_feature_windows_for_test()
{
    const unordered_map<string,int>& canon_map = get_canon_map();
    // determine number of canonical indices
    int max_idx = -1;
    for (const auto &kv : canon_map) max_idx = max(max_idx, kv.second);
    if (max_idx < 0)
    {
        printf("No windows found in canonical map\n");
        return;
    }

    // collect mapping from ALL windows (H=6) to canonical indices
    const auto windows = enumerate_windows_for_freed(6);
    vector<vector<tuple<int,int,int,int>>> reps(max_idx + 1); // x,y,dx,dy
    int total_windows = 0;
    for (const auto &cells : windows)
    {
        string canon = canonical_key_cells(cells);
        auto it = canon_map.find(canon);
        if (it == canon_map.end()){
            cerr << "Error: canonical key not found during print for cells: " << serialize_cells(cells) << "\n";
            assert(false && "canonical key missing in print");
        }
        int idx = it->second;
        int x = cells[0].first; int y = cells[0].second; int dx = cells[1].first - cells[0].first; int dy = cells[1].second - cells[0].second;
        reps[idx].push_back({x,y,dx,dy});
        ++total_windows;
    }

    auto count_windows = [&](int freed){
        return static_cast<int>(enumerate_windows_for_freed(freed).size());
    };
    printf("windows for H=4:%d H=5:%d H=6:%d (total mapped=%d)\n", count_windows(4), count_windows(5), count_windows(6), total_windows);

    // print mapping for H=6: index -> windows mapping
    for (int i=0;i<=max_idx;++i)
    {
        printf("idx %d: count=%zu\n", i, reps[i].size());
        for (auto &t : reps[i])
        {
            int x,y,dx,dy; tie(x,y,dx,dy) = t;
            printf("  (%d,%d) dir=(%d,%d)\n", x, y, dx, dy);
        }
    }
}
