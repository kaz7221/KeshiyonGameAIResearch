#ifndef CLION_CPP_INDIVIDUAL_H
#define CLION_CPP_INDIVIDUAL_H
#pragma once

#include <vector>
#include <random>
#include <iostream>
#include <cmath>
using namespace std;

extern thread_local std::mt19937 engine;

struct individual
{
    float score_diff_weight;
    float reach_diff_weight_not_deleted;
    float reach_diff_weight_deleted;
    float can_place_last_weight;
    float places_weight[3][6];
    // 3.1.2.2: 全4マスライン特徴量
    // [line_type][my_count]
    // line_type: 0=open(両者混在なし), 1=placeable(空きに次手で置ける), 2=threat(即4になりうる)
    // my_count: 0..4
    float line_weights[21][4];

    void normalize_weights()
    {
        constexpr float target_norm = 100.0f;
        float sum_sq = 0.0f;
        sum_sq += score_diff_weight * score_diff_weight;
        sum_sq += reach_diff_weight_not_deleted * reach_diff_weight_not_deleted;
        sum_sq += reach_diff_weight_deleted * reach_diff_weight_deleted;
        sum_sq += can_place_last_weight * can_place_last_weight;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                sum_sq += places_weight[i][j] * places_weight[i][j];
            }
        }
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                sum_sq += line_weights[i][j] * line_weights[i][j];
            }
        }

        if (sum_sq <= 0.0f)
        {
            return;
        }
        const float scale = target_norm / std::sqrt(sum_sq);
        score_diff_weight *= scale;
        reach_diff_weight_not_deleted *= scale;
        reach_diff_weight_deleted *= scale;
        can_place_last_weight *= scale;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                places_weight[i][j] *= scale;
            }
        }
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                line_weights[i][j] *= scale;
            }
        }
    }

    individual()
    {
        uniform_real_distribution dist(-100.0f, 100.0f);
        score_diff_weight = dist(engine);
        reach_diff_weight_not_deleted = dist(engine);
        reach_diff_weight_deleted = dist(engine);
        can_place_last_weight = dist(engine);
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 6; j++)
            {
                places_weight[i][j] = dist(engine);
            }
        }
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                line_weights[i][j] = dist(engine);
            }
        }
        normalize_weights();
    }

    individual(float score_diff_weight,
               float reach_diff_weight_not_deleted,
               float reach_diff_weight_deleted,
               float can_place_last_weight,
               const vector<vector<float>>& places_weight)
        : score_diff_weight(score_diff_weight),
          reach_diff_weight_not_deleted(reach_diff_weight_not_deleted),
          reach_diff_weight_deleted(reach_diff_weight_deleted),
          can_place_last_weight(can_place_last_weight)
    {
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                this->places_weight[i][j] = places_weight[i][j];
            }
        }
        normalize_weights();
    }

    individual(const individual& p1, const individual& p2)
    {
        score_diff_weight = (p1.score_diff_weight + p2.score_diff_weight) / 2.0f;
        reach_diff_weight_not_deleted = (p1.reach_diff_weight_not_deleted + p2.reach_diff_weight_not_deleted) / 2.0f;
        reach_diff_weight_deleted = (p1.reach_diff_weight_deleted + p2.reach_diff_weight_deleted) / 2.0f;
        can_place_last_weight = (p1.can_place_last_weight + p2.can_place_last_weight) / 2.0f;

        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 6; j++)
            {
                places_weight[i][j] = (p1.places_weight[i][j] + p2.places_weight[i][j]) / 2.0f;
            }
        }
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                line_weights[i][j] = (p1.line_weights[i][j] + p2.line_weights[i][j]) / 2.0f;
            }
        }
        normalize_weights();
    }

    void mutate(float mutation_rate, float mutation_scale)
    {
        normal_distribution gauss(0.0f, mutation_scale);
        uniform_real_distribution rand_prob(0.0f, 1.0f);

        // ラムダ式（内部関数）を作って、変異判定のコードを使い回せるようにする
        auto try_mutate = [&](float& w)
        {
            if (rand_prob(engine) < mutation_rate)
            {
                w += gauss(engine);
            }
        };

        // 独立した変数にそれぞれ変異のチャンスを与える
        try_mutate(score_diff_weight);
        try_mutate(reach_diff_weight_not_deleted);
        try_mutate(reach_diff_weight_deleted);
        try_mutate(can_place_last_weight);

        // 2次元配列の要素それぞれにも変異のチャンスを与える
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 6; j++)
            {
                try_mutate(places_weight[i][j]);
            }
        }
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                try_mutate(line_weights[i][j]);
            }
        }
        normalize_weights();
    }

    void print() const
    {
        cout << "score_diff_weight: " << score_diff_weight << endl;
        cout << "reach_diff_weight_not_deleted: " << reach_diff_weight_not_deleted << endl;
        cout << "reach_diff_weight_deleted: " << reach_diff_weight_deleted << endl;
        cout << "can_place_last_weight: " << can_place_last_weight << endl;
        cout << "places_weight:" << endl;
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 6; j++)
            {
                cout << places_weight[i][j] << " ";
            }
            cout << endl;
        }
        cout << "line_weights:" << endl;
        for (int i = 0; i < 21; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                cout << line_weights[i][j] << " ";
            }
            cout << endl;
        }
    }
};

#endif
