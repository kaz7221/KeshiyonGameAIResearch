//
// Created by kaz72 on 2026/04/28.
// Main game runner: GA training
//
#include "rule.h"
#include "individual.h"
#include "game_state.h"
#include "state_utils.h"
#include "game_modes.h"
#include "random_eval.h"
#include <thread>
#include <fstream>
#include <chrono>
#include <vector>
#include <limits>
#include <sstream>


namespace
{

void log_individual_weights(std::ostream& os, const individual& ind, const string& label)
{
    os << "# " << label << " individual weights\n";
    os << "score_diff_weight=" << ind.score_diff_weight << '\n';
    os << "reach_diff_weight_not_deleted=" << ind.reach_diff_weight_not_deleted << '\n';
    os << "reach_diff_weight_deleted=" << ind.reach_diff_weight_deleted << '\n';
    os << "can_place_last_weight=" << ind.can_place_last_weight << '\n';
    os << "places_weight:\n";
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            os << ind.places_weight[i][j] << '\n';
        }
    }
    os << "line_weights:\n";
    for (int i = 0; i < 21; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            os << ind.line_weights[i][j] << '\n';
        }
    }
}

bool try_parse_float(const string& text, float& value)
{
    try
    {
        size_t idx = 0;
        const float parsed = stof(text, &idx);
        if (idx == 0 || (idx != text.size() && text.find_first_not_of(" \t\r\n", idx) != string::npos))
        {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (...) { return false; }
}

void evaluate_population_parallel(vector<pair<int, individual>> &population, int ai_depth)
{
    const int population_size = static_cast<int>(population.size());
    const unsigned int hardware_threads = thread::hardware_concurrency();
    const int available_threads = hardware_threads == 0 ? 1 : static_cast<int>(hardware_threads);
    const int thread_count = max(1, min(population_size, available_threads));

    // We'll compute unordered pairs (i<j) once per pair and play two games (i as circle, j as circle).
    // Each thread keeps a local score vector and a local games counter, then we aggregate.

    // prepare per-thread local score buffers
    vector<vector<long long>> local_scores(thread_count, vector<long long>(population_size, 0));
    vector<long long> local_game_counts(thread_count, 0);

    const int chunk_size = (population_size + thread_count - 1) / thread_count;
    vector<thread> workers;
    workers.reserve(thread_count);

    for (int tid = 0; tid < thread_count; ++tid)
    {
        const int begin = tid * chunk_size;
        const int end = min(population_size, begin + chunk_size);
        if (begin >= end)
        {
            // keep empty slot
            continue;
        }
        workers.emplace_back([&, tid, begin, end]()
        {
            auto &scores_local = local_scores[tid];
            long long games_local = 0;
            for (int i = begin; i < end; ++i)
            {
                for (int j = i + 1; j < population_size; ++j)
                {
                    // play i as circle, j as X
                    const int res1 = game_modes::play_game(population[i].second, population[j].second, ai_depth);
                    // res1: 2 -> i wins, 1 -> draw, 0 -> j wins
                    if (res1 == 2) scores_local[i] += 2 * 9;
                    else if (res1 == 1) { scores_local[i] += 1 * 9; scores_local[j] += 1; }
                    else scores_local[j] += 2;
                    ++games_local;

                    // play j as circle, i as X
                    const int res2 = game_modes::play_game(population[j].second, population[i].second, ai_depth);
                    if (res2 == 2) scores_local[j] += 2 * 9;
                    else if (res2 == 1) { scores_local[i] += 1; scores_local[j] += 1 * 9; }
                    else scores_local[i] += 2;
                    ++games_local;
                }
            }
            local_game_counts[tid] = games_local;
        });
    }

    for (auto &worker : workers) worker.join();

    // aggregate
    vector<long long> scores(population_size, 0);
    long long total_games = 0;
    for (int tid = 0; tid < thread_count; ++tid)
    {
        for (int i = 0; i < population_size; ++i) scores[i] += local_scores[tid][i];
        total_games += local_game_counts[tid];
    }

    // apply baseline +1 to avoid zero fitness as previous behavior
    for (int i = 0; i < population_size; ++i)
    {
        population[i].first = static_cast<int>(scores[i] + 1);
    }

    // debug: you may want to log total_games somewhere; for now we could print to stdout
    cout << "evaluate_population_parallel: total_games=" << total_games << " (expected=" << (long long)population_size*(population_size-1) << ")" << endl;
}

} // namespace

bool load_individual_from_stream(std::istream& in, individual& out)
{
    vector<float> values;
    string line;
    while (getline(in, line))
    {
        if (line.empty()) continue;
        string trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r')) trimmed.pop_back();
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eq = trimmed.find('=');
        if (eq != string::npos)
        {
            string value_text = trimmed.substr(eq + 1);
            float v = 0.0f;
            if (try_parse_float(value_text, v))
            {
                values.push_back(v);
                continue;
            }
        }

        istringstream ss(trimmed);
        string token;
        while (ss >> token)
        {
            float v = 0.0f;
            if (try_parse_float(token, v))
            {
                values.push_back(v);
            }
        }
    }

    if (values.size() != 106)
    {
        cerr << "Expected 106 numeric weights, got " << values.size() << "\n";
        return false;
    }

    int idx = 0;
    out.score_diff_weight = values[idx++];
    out.reach_diff_weight_not_deleted = values[idx++];
    out.reach_diff_weight_deleted = values[idx++];
    out.can_place_last_weight = values[idx++];
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 6; ++j)
        {
            out.places_weight[i][j] = values[idx++];
        }
    }
    for (int i = 0; i < 21; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            out.line_weights[i][j] = values[idx++];
        }
    }
    out.normalize_weights();
    return true;
}

bool load_individual_from_file(const string& path, individual& out)
{
    ifstream in(path);
    if (!in)
    {
        cerr << "Failed to open weight file: " << path << '\n';
        return false;
    }
    return load_individual_from_stream(in, out);
}

int run_weight_match_mode_from_stream(istream& in, const string& opponent, int ai_depth, int games_per_side)
{
    individual ai_ind;
    if (!load_individual_from_stream(in, ai_ind))
    {
        return 1;
    }

    cout << "Loaded weights from stdin\n";
    cout << "Opponent=" << opponent << ", ai_depth=" << ai_depth << ", games_per_side=" << games_per_side << "\n";

    pair<random_eval::RandomEvalResult, random_eval::RandomEvalResult> result;
    if (opponent == "random")
    {
        result = random_eval::evaluate_against_random(ai_ind, ai_depth, games_per_side);
    }
    else if (opponent == "walk231")
    {
        result = random_eval::evaluate_against_random_walk_231_all(ai_ind, ai_depth, 3);
    }
    else if (opponent == "walk232")
    {
        result = random_eval::evaluate_against_random_walk_232_all(ai_ind, ai_depth);
    }
    else
    {
        cerr << "Unknown opponent: " << opponent << "\n";
        cerr << "Supported: random, walk231, walk232\n";
        return 1;
    }

    const auto& first = result.first;
    const auto& second = result.second;
    const int total_first = first.wins + first.draws + first.losses;
    const int total_second = second.wins + second.draws + second.losses;
    cout << "AI as first: wins=" << first.wins << ", draws=" << first.draws << ", losses=" << first.losses
         << ", total=" << total_first << "\n";
    cout << "AI as second: wins=" << second.wins << ", draws=" << second.draws << ", losses=" << second.losses
         << ", total=" << total_second << "\n";
    return 0;
}

int run_weight_match_mode(const string& weight_path, const string& opponent, int ai_depth, int games_per_side)
{
    individual ai_ind;
    if (!load_individual_from_file(weight_path, ai_ind))
    {
        return 1;
    }

    cout << "Loaded weights from " << weight_path << "\n";
    cout << "Opponent=" << opponent << ", ai_depth=" << ai_depth << ", games_per_side=" << games_per_side << "\n";

    pair<random_eval::RandomEvalResult, random_eval::RandomEvalResult> result;
    if (opponent == "random")
    {
        result = random_eval::evaluate_against_random(ai_ind, ai_depth, games_per_side);
    }
    else if (opponent == "walk231")
    {
        result = random_eval::evaluate_against_random_walk_231_all(ai_ind, ai_depth, 3);
    }
    else if (opponent == "walk232")
    {
        result = random_eval::evaluate_against_random_walk_232_all(ai_ind, ai_depth);
    }
    else
    {
        cerr << "Unknown opponent: " << opponent << "\n";
        cerr << "Supported: random, walk231, walk232\n";
        return 1;
    }

    const auto& first = result.first;
    const auto& second = result.second;
    const int total_first = first.wins + first.draws + first.losses;
    const int total_second = second.wins + second.draws + second.losses;
    cout << "AI as first: wins=" << first.wins << ", draws=" << first.draws << ", losses=" << first.losses
         << ", total=" << total_first << "\n";
    cout << "AI as second: wins=" << second.wins << ", draws=" << second.draws << ", losses=" << second.losses
         << ", total=" << total_second << "\n";
    return 0;
}

// Test helper: run one generation timing and return elapsed milliseconds
extern "C" long run_one_generation_timing(int ai_depth, int population_size)
{
    // Build a dummy population
    vector<pair<int, individual>> population;
    population.reserve(population_size);
    for (int i=0;i<population_size;++i) population.emplace_back(0, individual());

    auto t0 = chrono::high_resolution_clock::now();
    evaluate_population_parallel(population, ai_depth);
    auto t1 = chrono::high_resolution_clock::now();
    long ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    return ms;
}


// Public wrapper for benchmarking (calls internal play_game)
int play_game_public(const individual& circle_ind, const individual& x_ind, int ai_depth)
{
    return game_modes::play_game(circle_ind, x_ind, ai_depth);
}

#ifndef BUILD_AS_LIBRARY
int main()
{
    cin.tie(&cout);
    ios_base::sync_with_stdio(false);

    ofstream ga_log("ga_output.txt", ios::out);
    if (!ga_log)
    {
        cerr << "Failed to open ga_output.txt\n";
        return 1;
    }
    const time_t now = time(nullptr);
    ga_log << "# GA run started at " << ctime(&now);
    constexpr int POPULATION_SIZE = 48; // 個体数
    constexpr int GENERATIONS = 100; // 世代数
    // constexpr int AI_DEPTH = 7; // 思考の深さ 3.1.2.1ルール用
    constexpr int AI_DEPTH = 1; // 思考の深さ 3.1.2.2ルール用
    constexpr int ELITE_COUNT = 2;
    constexpr int RANDOM_EVAL_INTERVAL = 10;
    constexpr int RANDOM_EVAL_GAMES_PER_SIDE = 500;
    constexpr float MUTATION_RATE = 0.1f;// 突然変異の確率
    constexpr float MUTATION_SCALE = 20.0f; //突然変異の大きさの範囲
    vector<pair<int, individual>> new_population(POPULATION_SIZE);
    vector<long long> sum(POPULATION_SIZE + 1, 0);
    auto start = chrono::high_resolution_clock::now();

    for (int roop = 0; roop < GENERATIONS; roop++)
    {
        vector<pair<int, individual>> population;
        population.reserve(POPULATION_SIZE);
        for (int i = 0; i < POPULATION_SIZE; i++)
        {
            population.push_back(std::move(new_population[i]));
            new_population[i].first = 0;
        }

        auto time_point = chrono::high_resolution_clock::now();
        const auto elapsed_ms = chrono::duration_cast<chrono::milliseconds>(time_point - start).count();
        cout << elapsed_ms << "ms" << endl;
        cout << "now running generation: " << roop << endl;
        if (roop > 0)
            cout << "Estimated time remaining: " << elapsed_ms * (GENERATIONS - roop) / (roop * 1000) << "s"
                << endl;

        evaluate_population_parallel(population, AI_DEPTH);
        ranges::sort(population,
                     [](const pair<int, individual>& a, const pair<int, individual>& b)
                     {
                         return a.first > b.first;
                     });
        game_modes::log_generation(ga_log, roop, population);

        // GA CSV logging: generation,best,mean,worst,weight_std,prob_ratio_best_worst,w1..wL
        {
            std::ofstream ga_csv("ga_log.txt", std::ios::app);
            if (ga_csv)
            {
                if (roop == 0) // write header if starting a run (may duplicate if file reused)
                {
                    const int L_header = 106;
                    ga_csv << "generation,best,mean,worst,weight_std,prob_ratio_best_worst";
                    // append weight headers
                    for (int idx = 1; idx <= L_header; ++idx) ga_csv << ",w" << idx;
                    ga_csv << '\n';
                }

                // compute stats
                const int best = population.front().first;
                long long sum_f = 0;
                int worst = population.back().first;
                for (const auto &p : population) sum_f += p.first;
                const double mean = static_cast<double>(sum_f) / static_cast<double>(population.size());

                // compute population weight std (std of vectors)
                const int L = 106;
                vector<double> mean_vec(L, 0.0);
                auto flatten = [&](const individual &ind, vector<double> &out)
                {
                    out.clear(); out.reserve(L);
                    out.push_back(ind.score_diff_weight);
                    out.push_back(ind.reach_diff_weight_not_deleted);
                    out.push_back(ind.reach_diff_weight_deleted);
                    out.push_back(ind.can_place_last_weight);
                    for (int i=0;i<3;++i) for (int j=0;j<6;++j) out.push_back(ind.places_weight[i][j]);
                    for (int i=0;i<21;++i) for (int j=0;j<4;++j) out.push_back(ind.line_weights[i][j]);
                };
                vector<double> tmpv;
                // compute mean vector
                for (const auto &p : population)
                {
                    flatten(p.second, tmpv);
                    for (int k=0;k<L;++k) mean_vec[k] += tmpv[k];
                }
                for (int k=0;k<L;++k) mean_vec[k] /= static_cast<double>(population.size());
                // compute variance = mean squared distance
                double var = 0.0;
                for (const auto &p : population)
                {
                    flatten(p.second, tmpv);
                    double dist2 = 0.0;
                    for (int k=0;k<L;++k) { double d = tmpv[k] - mean_vec[k]; dist2 += d*d; }
                    var += dist2;
                }
                var /= static_cast<double>(population.size());
                const double weight_std = std::sqrt(var);

                // selection probability ratio best/worst
                const double prob_best = static_cast<double>(best) / static_cast<double>(sum_f);
                const double prob_worst = static_cast<double>(worst) / static_cast<double>(sum_f);
                const double prob_ratio = (prob_worst <= 0.0) ? std::numeric_limits<double>::infinity() : (prob_best / prob_worst);

                // write CSV line
                ga_csv << roop << ',' << best << ',' << mean << ',' << worst << ',' << weight_std << ',' << prob_ratio;
                // write best individual's weights
                vector<double> bestw; flatten(population.front().second, bestw);
                for (int k=0;k<L;++k) ga_csv << ',' << bestw[k];
                ga_csv << '\n';
                ga_csv.flush();
            }
        }

        if ((roop % 10 == 0 || roop == GENERATIONS - 1) && !population.empty())
        {
            log_individual_weights(ga_log, population.front().second, "generation=" + to_string(roop));
            cout << "[generation " << roop << "] best individual weights logged" << endl;
        }

        if (roop % RANDOM_EVAL_INTERVAL == 0 || roop == GENERATIONS - 1)
        {
            // Baseline: pure random opponent
            const auto random_eval_pair = random_eval::evaluate_against_random(
                population.front().second, AI_DEPTH, RANDOM_EVAL_GAMES_PER_SIDE);
            const auto &random_first = random_eval_pair.first;
            const auto &random_second = random_eval_pair.second;
            const int total_first = random_first.wins + random_first.draws + random_first.losses;
            const int total_second = random_second.wins + random_second.draws + random_second.losses;
            const double win_rate_first = total_first > 0 ? static_cast<double>(random_first.wins) / static_cast<double>(total_first) : 0.0;
            const double draw_rate_first = total_first > 0 ? static_cast<double>(random_first.draws) / static_cast<double>(total_first) : 0.0;
            const double win_rate_second = total_second > 0 ? static_cast<double>(random_second.wins) / static_cast<double>(total_second) : 0.0;
            const double draw_rate_second = total_second > 0 ? static_cast<double>(random_second.draws) / static_cast<double>(total_second) : 0.0;
            const auto eval_time_point = chrono::high_resolution_clock::now();
            const auto elapsed_ms_eval = chrono::duration_cast<chrono::milliseconds>(eval_time_point - start).count();
            ga_log << "random_eval,generation=" << roop
                   << ",ai_first_wins=" << random_first.wins
                   << ",ai_first_draws=" << random_first.draws
                   << ",ai_first_losses=" << random_first.losses
                   << ",ai_first_win_rate=" << win_rate_first
                   << ",ai_first_draw_rate=" << draw_rate_first
                   << ",ai_second_wins=" << random_second.wins
                   << ",ai_second_draws=" << random_second.draws
                   << ",ai_second_losses=" << random_second.losses
                   << ",ai_second_win_rate=" << win_rate_second
                   << ",ai_second_draw_rate=" << draw_rate_second
                   << ",elapsed_ms=" << elapsed_ms_eval << '\n';
            cout << "random_eval generation " << roop
                 << " ai_first_win_rate=" << win_rate_first << " (" << random_first.wins << "/" << total_first << ")"
                 << " ai_first_draw_rate=" << draw_rate_first << " (" << random_first.draws << "/" << total_first << ")"
                 << " ai_second_win_rate=" << win_rate_second << " (" << random_second.wins << "/" << total_second << ")"
                 << " ai_second_draw_rate=" << draw_rate_second << " (" << random_second.draws << "/" << total_second << ")" << endl;
            // 2.3.1: start from 2-move-per-side random walks
            const auto walk231 = random_eval::evaluate_against_random_walk_231_all(population.front().second, AI_DEPTH, 4);
            const auto &walk231_first = walk231.first;
            const auto &walk231_second = walk231.second;
            const int total231_first = walk231_first.wins + walk231_first.draws + walk231_first.losses;
            const int total231_second = walk231_second.wins + walk231_second.draws + walk231_second.losses;
            const double win_rate231_first = total231_first > 0 ? static_cast<double>(walk231_first.wins) / static_cast<double>(total231_first) : 0.0;
            const double draw_rate231_first = total231_first > 0 ? static_cast<double>(walk231_first.draws) / static_cast<double>(total231_first) : 0.0;
            const double win_rate231_second = total231_second > 0 ? static_cast<double>(walk231_second.wins) / static_cast<double>(total231_second) : 0.0;
            const double draw_rate231_second = total231_second > 0 ? static_cast<double>(walk231_second.draws) / static_cast<double>(total231_second) : 0.0;
            ga_log << "walk231,total_moves=3,generation=" << roop
                   << ",ai_first_wins=" << walk231_first.wins
                   << ",ai_first_draws=" << walk231_first.draws
                   << ",ai_first_losses=" << walk231_first.losses
                   << ",ai_first_win_rate=" << win_rate231_first
                   << ",ai_first_draw_rate=" << draw_rate231_first
                   << ",ai_second_wins=" << walk231_second.wins
                   << ",ai_second_draws=" << walk231_second.draws
                   << ",ai_second_losses=" << walk231_second.losses
                   << ",ai_second_win_rate=" << win_rate231_second
                   << ",ai_second_draw_rate=" << draw_rate231_second
                   << ",elapsed_ms=" << elapsed_ms_eval << '\n';
            cout << "walk231(total_moves=3) generation " << roop
                 << " ai_first_win_rate=" << win_rate231_first
                 << " (" << walk231_first.wins << "/" << total231_first << ")"
                 << " ai_first_draw_rate=" << draw_rate231_first
                 << " (" << walk231_first.draws << "/" << total231_first << ")"
                 << " ai_second_win_rate=" << win_rate231_second
                 << " (" << walk231_second.wins << "/" << total231_second << ")"
                 << " ai_second_draw_rate=" << draw_rate231_second
                 << " (" << walk231_second.draws << "/" << total231_second << ")" << endl;

            // 2.3.2: start from 3-first-moves with symmetric responses
            const auto walk232 = random_eval::evaluate_against_random_walk_232_all(population.front().second, AI_DEPTH);
            const auto &walk232_first = walk232.first;
            const int total232_first = walk232_first.wins + walk232_first.draws + walk232_first.losses;
            const double win_rate232_first = total232_first > 0 ? static_cast<double>(walk232_first.wins) / static_cast<double>(total232_first) : 0.0;
            const double draw_rate232_first = total232_first > 0 ? static_cast<double>(walk232_first.draws) / static_cast<double>(total232_first) : 0.0;
            ga_log << "walk232,generation=" << roop
                   << ",ai_first_wins=" << walk232_first.wins
                   << ",ai_first_draws=" << walk232_first.draws
                   << ",ai_first_losses=" << walk232_first.losses
                   << ",ai_first_win_rate=" << win_rate232_first
                   << ",ai_first_draw_rate=" << draw_rate232_first
                   << ",elapsed_ms=" << elapsed_ms_eval << '\n';
            cout << "walk232 (AI as first only) generation " << roop
                 << " ai_first_win_rate=" << win_rate232_first
                 << " (" << walk232_first.wins << "/" << total232_first << ")"
                 << " ai_first_draw_rate=" << draw_rate232_first
                 << " (" << walk232_first.draws << "/" << total232_first << ")" << endl;
        }

        // Reuse sum vector - clear and refill
        sum.assign(POPULATION_SIZE + 1, 0);
        for (int i = 1; i <= POPULATION_SIZE; i++)
        {
            sum[i] = sum[i - 1] + static_cast<long long>(population[i - 1].first);
        }

        uniform_int_distribution<long long> dist(1, sum.back());
        new_population.clear();
        for (int i = 0; i < ELITE_COUNT; ++i)
        {
            new_population.emplace_back(0, population[i].second);
        }
        for (int i = ELITE_COUNT; i < POPULATION_SIZE; i++)
        {
            long long choice1 = dist(engine), choice2 = dist(engine);
            const auto chosen1_it = lower_bound(sum.begin(), sum.end(), choice1);
            const auto chosen2_it = lower_bound(sum.begin(), sum.end(), choice2);
            const int chosen1 = static_cast<int>(distance(sum.begin(), chosen1_it) - 1);
            const int chosen2 = static_cast<int>(distance(sum.begin(), chosen2_it) - 1);
            //1-indexedから0-indexedにここで直す
            individual child(population[chosen1].second, population[chosen2].second);
            child.mutate(MUTATION_RATE, MUTATION_SCALE);
            new_population.emplace_back(0, std::move(child));
        }
        if (roop == GENERATIONS - 1)
        {
            population.front().second.print();
            log_individual_weights(ga_log, population.front().second, "final best");
        }
    }

    const auto end_time = chrono::high_resolution_clock::now();
    const auto total_elapsed_ms = chrono::duration_cast<chrono::milliseconds>(end_time - start).count();
    ga_log << "# total_elapsed_ms=" << total_elapsed_ms << '\n';
    ga_log << "# total_elapsed_s=" << (static_cast<double>(total_elapsed_ms) / 1000.0) << '\n';
    cout << "total elapsed: " << total_elapsed_ms << "ms" << endl;

    return 0;
}
#endif
