#include "rule.h"
#include "random_eval.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace
{
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

    bool load_individual_from_stream(istream& in, individual& out)
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
            for (int j = 0; j < 6; ++j)
                out.places_weight[i][j] = values[idx++];
        for (int i = 0; i < 21; ++i)
            for (int j = 0; j < 4; ++j)
                out.line_weights[i][j] = values[idx++];
        out.normalize_weights();
        return true;
    }

    bool load_individual_from_file(const string& path, individual& out)
    {
        ifstream in(path);
        if (!in) { cerr << "Failed to open weight file: " << path << "\n"; return false; }
        return load_individual_from_stream(in, out);
    }

    int run_weight_match_mode_inner(const individual& ai_ind, const string& opponent, int ai_depth, int games_per_side)
    {
        cout << "Opponent=" << opponent << ", ai_depth=" << ai_depth << ", games_per_side=" << games_per_side << "\n";

        pair<random_eval::RandomEvalResult, random_eval::RandomEvalResult> result;
        if (opponent == "random")
            result = random_eval::evaluate_against_random(ai_ind, ai_depth, games_per_side);
        else if (opponent == "walk231")
            result = random_eval::evaluate_against_random_walk_231_all(ai_ind, ai_depth, 3);
        else if (opponent == "walk232")
            result = random_eval::evaluate_against_random_walk_232_all(ai_ind, ai_depth);
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
}

int main(int argc, char** argv)
{
    if (argc < 2 || string(argv[1]) == "--help" || string(argv[1]) == "-h")
    {
        cout << "Usage:\n";
        cout << "  del_kesi4_weight_match.exe < weights.txt\n";
        cout << "  del_kesi4_weight_match.exe weights.txt\n";
        cout << "  del_kesi4_weight_match.exe weights.txt --opponent random --ai-depth 1 --games 20\n";
        cout << "  del_kesi4_weight_match.exe --weights weights.txt --opponent walk231 --ai-depth 2 --games 20\n";
        cout << "  echo \"...106 weights...\" | del_kesi4_weight_match.exe --stdin --opponent random --ai-depth 1 --games 20\n";
        cout << "Supported opponents: random, walk231, walk232\n";
        return 0;
    }

    string weight_path;
    string opponent = "random";
    int ai_depth = 1;
    int games_per_side = 20;

    if (string(argv[1]) == "--stdin" || string(argv[1]) == "-")
    {
        for (int i = 2; i < argc; ++i)
        {
            string opt = argv[i];
            if (opt == "--opponent" && i + 1 < argc) opponent = argv[++i];
            else if (opt == "--ai-depth" && i + 1 < argc) ai_depth = stoi(argv[++i]);
            else if (opt == "--games" && i + 1 < argc) games_per_side = stoi(argv[++i]);
            else if (opt == "--help" || opt == "-h")
            {
                cout << "Usage:\n";
                cout << "  del_kesi4_weight_match.exe < weights.txt\n";
                cout << "Supported opponents: random, walk231, walk232\n";
                return 0;
            }
            else { cerr << "Unknown option: " << opt << "\n"; return 1; }
        }

        individual ai_ind;
        if (!load_individual_from_stream(cin, ai_ind)) return 1;
        cout << "Loaded weights from stdin\n";
        return run_weight_match_mode_inner(ai_ind, opponent, ai_depth, games_per_side);
    }

    if (string(argv[1]) == "--weights")
    {
        if (argc < 3) { cerr << "Missing weight file path after --weights\n"; return 1; }
        weight_path = argv[2];
        for (int i = 3; i < argc; ++i)
        {
            string opt = argv[i];
            if (opt == "--opponent" && i + 1 < argc) opponent = argv[++i];
            else if (opt == "--ai-depth" && i + 1 < argc) ai_depth = stoi(argv[++i]);
            else if (opt == "--games" && i + 1 < argc) games_per_side = stoi(argv[++i]);
            else if (opt == "--help" || opt == "-h")
            {
                cout << "Usage:\n";
                cout << "  del_kesi4_weight_match.exe weights.txt\n";
                cout << "Supported opponents: random, walk231, walk232\n";
                return 0;
            }
            else { cerr << "Unknown option: " << opt << "\n"; return 1; }
        }
    }
    else
    {
        weight_path = argv[1];
        for (int i = 2; i < argc; ++i)
        {
            string opt = argv[i];
            if (opt == "--opponent" && i + 1 < argc) opponent = argv[++i];
            else if (opt == "--ai-depth" && i + 1 < argc) ai_depth = stoi(argv[++i]);
            else if (opt == "--games" && i + 1 < argc) games_per_side = stoi(argv[++i]);
            else if (opt == "--help" || opt == "-h")
            {
                cout << "Usage:\n";
                cout << "  del_kesi4_weight_match.exe weights.txt\n";
                cout << "Supported opponents: random, walk231, walk232\n";
                return 0;
            }
            else { cerr << "Unknown option: " << opt << "\n"; return 1; }
        }
    }

    individual ai_ind;
    if (!load_individual_from_file(weight_path, ai_ind)) return 1;
    cout << "Loaded weights from " << weight_path << "\n";
    return run_weight_match_mode_inner(ai_ind, opponent, ai_depth, games_per_side);
}
