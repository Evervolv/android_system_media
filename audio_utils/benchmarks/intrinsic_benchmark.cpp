/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <array>
#include <climits>
#include <cstdlib>
#include <functional>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#include <audio_utils/intrinsic_utils.h>
#include <audio_utils/format.h>

/**
Pixel 6 Pro (using Android 14 clang)

---------------------------------------------------------------------------------
Benchmark                                       Time             CPU   Iterations
---------------------------------------------------------------------------------
BM_VectorTestMulLoopFloat/1                  1199 ns         1195 ns       583505
BM_VectorTestMulLoopFloat/2                  2255 ns         2248 ns       317302
BM_VectorTestMulLoopFloat/4                  4454 ns         4438 ns       158692
BM_VectorTestMulLoopFloat/7                  7786 ns         7757 ns        90247
BM_VectorTestMulLoopFloat/8                  8995 ns         8962 ns        76373
BM_VectorTestMulLoopFloat/15                17131 ns        17066 ns        41214
BM_VectorTestMulLoopFloat/16                18439 ns        18341 ns        38319
BM_VectorTestMulConstArraySizeFloat/1         183 ns          182 ns      3938572
BM_VectorTestMulConstArraySizeFloat/2         640 ns          638 ns      1113513
BM_VectorTestMulConstArraySizeFloat/3        2102 ns         2093 ns       331829
BM_VectorTestMulConstArraySizeFloat/4        3771 ns         3758 ns       185266
BM_VectorTestMulConstArraySizeFloat/5        1825 ns         1818 ns       382081
BM_VectorTestMulConstArraySizeFloat/6        1905 ns         1898 ns       370506
BM_VectorTestMulConstArraySizeFloat/7        2745 ns         2734 ns       256104
BM_VectorTestMulConstArraySizeFloat/8        2010 ns         2002 ns       351298
BM_VectorTestMulConstArraySizeFloat/9        3158 ns         3146 ns       222887
BM_VectorTestMulConstArraySizeFloat/10       3018 ns         3007 ns       233799
BM_VectorTestMulConstArraySizeFloat/11       4005 ns         3991 ns       176145
BM_VectorTestMulConstArraySizeFloat/12       3081 ns         3068 ns       228512
BM_VectorTestMulConstArraySizeFloat/13       4409 ns         4393 ns       159303
BM_VectorTestMulConstArraySizeFloat/14       4242 ns         4219 ns       165899
BM_VectorTestMulConstArraySizeFloat/15       5301 ns         5279 ns       134157
BM_VectorTestMulConstArraySizeFloat/16       4078 ns         4063 ns       174066
BM_VectorTestMulConstArraySizeFloat/17       5693 ns         5669 ns       125403
BM_VectorTestMulConstArraySizeFloat/18       5339 ns         5318 ns       131839
BM_VectorTestMulConstArraySizeFloat/19       6508 ns         6483 ns       108158
BM_VectorTestMulConstArraySizeFloat/20       5108 ns         5089 ns       139637
BM_VectorTestMulConstArraySizeFloat/21       6896 ns         6868 ns       102084
BM_VectorTestMulConstArraySizeFloat/22       6523 ns         6490 ns       109281
BM_VectorTestMulConstArraySizeFloat/23       7734 ns         7686 ns        92986
BM_VectorTestMulConstArraySizeFloat/24       6138 ns         6071 ns       116883
BM_VectorTestMulConstArraySizeFloat/25       8122 ns         8085 ns        86703
BM_VectorTestMulConstArraySizeFloat/26       7670 ns         7637 ns        91665
BM_VectorTestMulConstArraySizeFloat/27       9026 ns         8988 ns        78633
BM_VectorTestMulConstArraySizeFloat/28       7161 ns         7129 ns        99711
BM_VectorTestMulConstArraySizeFloat/29       9380 ns         9341 ns        75947
BM_VectorTestMulConstArraySizeFloat/30       8878 ns         8838 ns        79578
BM_VectorTestMulConstArraySizeFloat/31      10277 ns        10230 ns        67954
BM_VectorTestMulConstArraySizeFloat/32       8122 ns         8083 ns        87244
BM_VectorTestMulForcedIntrinsics/1            188 ns          187 ns      3628943
BM_VectorTestMulForcedIntrinsics/2           1184 ns         1180 ns       565704
BM_VectorTestMulForcedIntrinsics/3           1692 ns         1684 ns       414409
BM_VectorTestMulForcedIntrinsics/4           1227 ns         1222 ns       578638
BM_VectorTestMulForcedIntrinsics/5           1885 ns         1878 ns       366852
BM_VectorTestMulForcedIntrinsics/6           1984 ns         1976 ns       352979
BM_VectorTestMulForcedIntrinsics/7           2815 ns         2803 ns       249306
BM_VectorTestMulForcedIntrinsics/8           2081 ns         2073 ns       339434
BM_VectorTestMulForcedIntrinsics/9           3051 ns         3040 ns       229261
BM_VectorTestMulForcedIntrinsics/10          3198 ns         3187 ns       220889
BM_VectorTestMulForcedIntrinsics/11          4083 ns         4067 ns       171785
BM_VectorTestMulForcedIntrinsics/12          3167 ns         3156 ns       221858
BM_VectorTestMulForcedIntrinsics/13          4497 ns         4479 ns       156926
BM_VectorTestMulForcedIntrinsics/14          4339 ns         4323 ns       162496
BM_VectorTestMulForcedIntrinsics/15          5294 ns         5274 ns       135733
BM_VectorTestMulForcedIntrinsics/16          4167 ns         4150 ns       168642
BM_VectorTestMulForcedIntrinsics/17          5732 ns         5710 ns       122927
BM_VectorTestMulForcedIntrinsics/18          5449 ns         5424 ns       131800
BM_VectorTestMulForcedIntrinsics/19          6539 ns         6504 ns       107850
BM_VectorTestMulForcedIntrinsics/20          5219 ns         5198 ns       135148
BM_VectorTestMulForcedIntrinsics/21          6676 ns         6639 ns       105846
BM_VectorTestMulForcedIntrinsics/22          6618 ns         6589 ns       107258
BM_VectorTestMulForcedIntrinsics/23          7774 ns         7741 ns        90216
BM_VectorTestMulForcedIntrinsics/24          6231 ns         6201 ns       116996
BM_VectorTestMulForcedIntrinsics/25          8156 ns         8121 ns        86237
BM_VectorTestMulForcedIntrinsics/26          7615 ns         7578 ns        91086
BM_VectorTestMulForcedIntrinsics/27          9067 ns         8995 ns        76733
BM_VectorTestMulForcedIntrinsics/28          7090 ns         7031 ns       101117
BM_VectorTestMulForcedIntrinsics/29          9220 ns         9160 ns        76350
BM_VectorTestMulForcedIntrinsics/30          8895 ns         8832 ns        80551
BM_VectorTestMulForcedIntrinsics/31         10060 ns        10001 ns        71265
BM_VectorTestMulForcedIntrinsics/32          8056 ns         7996 ns        88176
BM_VectorTestAddConstArraySizeFloat/1         188 ns          187 ns      3742628
BM_VectorTestAddConstArraySizeFloat/2         634 ns          631 ns      1095480
BM_VectorTestAddConstArraySizeFloat/4        3723 ns         3710 ns       188332
BM_VectorTestAddConstArraySizeFloat/7        2791 ns         2777 ns       252911
BM_VectorTestAddConstArraySizeFloat/8        2060 ns         2051 ns       345573
BM_VectorTestAddConstArraySizeFloat/15       5322 ns         5302 ns       132415
BM_VectorTestAddConstArraySizeFloat/16       4101 ns         4083 ns       170300
BM_VectorTestAddForcedIntrinsics/1            187 ns          186 ns      3656441
BM_VectorTestAddForcedIntrinsics/2           1184 ns         1178 ns       564643
BM_VectorTestAddForcedIntrinsics/4           1218 ns         1213 ns       584709
BM_VectorTestAddForcedIntrinsics/7           2775 ns         2764 ns       252256
BM_VectorTestAddForcedIntrinsics/8           2070 ns         2062 ns       342709
BM_VectorTestAddForcedIntrinsics/15          5213 ns         5192 ns       132663
BM_VectorTestAddForcedIntrinsics/16          4116 ns         4100 ns       171005


Pixel 9 XL Pro (using Android 14 clang)
---------------------------------------------------------------------------------
Benchmark                                       Time             CPU   Iterations
---------------------------------------------------------------------------------
BM_VectorTestMulLoopFloat/1                  1171 ns         1166 ns       450848
BM_VectorTestMulLoopFloat/2                  1847 ns         1840 ns       381613
BM_VectorTestMulLoopFloat/4                  3432 ns         3423 ns       205730
BM_VectorTestMulLoopFloat/7                  5615 ns         5598 ns       124818
BM_VectorTestMulLoopFloat/8                  6411 ns         6383 ns       109013
BM_VectorTestMulLoopFloat/15                12371 ns        12332 ns        55439
BM_VectorTestMulLoopFloat/16                13594 ns        13555 ns        51753
BM_VectorTestMulConstArraySizeFloat/1         153 ns          152 ns      4534625
BM_VectorTestMulConstArraySizeFloat/2         683 ns          680 ns      1005789
BM_VectorTestMulConstArraySizeFloat/3         886 ns          883 ns       803793
BM_VectorTestMulConstArraySizeFloat/4        1491 ns         1487 ns       471683
BM_VectorTestMulConstArraySizeFloat/5        1448 ns         1443 ns       486353
BM_VectorTestMulConstArraySizeFloat/6        1482 ns         1478 ns       474901
BM_VectorTestMulConstArraySizeFloat/7        2279 ns         2272 ns       308978
BM_VectorTestMulConstArraySizeFloat/8        1620 ns         1600 ns       438957
BM_VectorTestMulConstArraySizeFloat/9        2505 ns         2487 ns       283335
BM_VectorTestMulConstArraySizeFloat/10       2389 ns         2386 ns       293332
BM_VectorTestMulConstArraySizeFloat/11       3185 ns         3180 ns       219746
BM_VectorTestMulConstArraySizeFloat/12       2285 ns         2280 ns       307091
BM_VectorTestMulConstArraySizeFloat/13       3464 ns         3459 ns       201902
BM_VectorTestMulConstArraySizeFloat/14       3254 ns         3249 ns       215345
BM_VectorTestMulConstArraySizeFloat/15       4156 ns         4149 ns       169102
BM_VectorTestMulConstArraySizeFloat/16       3075 ns         3068 ns       228544
BM_VectorTestMulConstArraySizeFloat/17       4469 ns         4442 ns       157317
BM_VectorTestMulConstArraySizeFloat/18       4141 ns         4133 ns       170148
BM_VectorTestMulConstArraySizeFloat/19       5193 ns         5179 ns       135294
BM_VectorTestMulConstArraySizeFloat/20       3876 ns         3866 ns       181134
BM_VectorTestMulConstArraySizeFloat/21       5450 ns         5429 ns       129921
BM_VectorTestMulConstArraySizeFloat/22       5075 ns         5056 ns       139238
BM_VectorTestMulConstArraySizeFloat/23       6145 ns         6125 ns       114880
BM_VectorTestMulConstArraySizeFloat/24       4659 ns         4646 ns       150923
BM_VectorTestMulConstArraySizeFloat/25       6423 ns         6400 ns       109467
BM_VectorTestMulConstArraySizeFloat/26       5962 ns         5947 ns       117755
BM_VectorTestMulConstArraySizeFloat/27       7139 ns         7115 ns        98581
BM_VectorTestMulConstArraySizeFloat/28       5462 ns         5446 ns       128477
BM_VectorTestMulConstArraySizeFloat/29       7431 ns         7399 ns        94492
BM_VectorTestMulConstArraySizeFloat/30       6877 ns         6854 ns       101706
BM_VectorTestMulConstArraySizeFloat/31       8322 ns         8304 ns        83352
BM_VectorTestMulConstArraySizeFloat/32       6223 ns         6208 ns       114265
BM_VectorTestMulForcedIntrinsics/1            160 ns          160 ns      4365646
BM_VectorTestMulForcedIntrinsics/2            848 ns          845 ns       807945
BM_VectorTestMulForcedIntrinsics/3           1435 ns         1430 ns       489448
BM_VectorTestMulForcedIntrinsics/4            937 ns          934 ns       757416
BM_VectorTestMulForcedIntrinsics/5           1477 ns         1473 ns       474891
BM_VectorTestMulForcedIntrinsics/6           1825 ns         1820 ns       385118
BM_VectorTestMulForcedIntrinsics/7           2303 ns         2298 ns       303823
BM_VectorTestMulForcedIntrinsics/8           1643 ns         1638 ns       430851
BM_VectorTestMulForcedIntrinsics/9           2490 ns         2482 ns       281294
BM_VectorTestMulForcedIntrinsics/10          2429 ns         2423 ns       291028
BM_VectorTestMulForcedIntrinsics/11          3201 ns         3193 ns       219256
BM_VectorTestMulForcedIntrinsics/12          2341 ns         2335 ns       302086
BM_VectorTestMulForcedIntrinsics/13          3475 ns         3466 ns       201570
BM_VectorTestMulForcedIntrinsics/14          3294 ns         3286 ns       212762
BM_VectorTestMulForcedIntrinsics/15          4141 ns         4129 ns       169275
BM_VectorTestMulForcedIntrinsics/16          3123 ns         3116 ns       225516
BM_VectorTestMulForcedIntrinsics/17          4447 ns         4436 ns       157620
BM_VectorTestMulForcedIntrinsics/18          4175 ns         4163 ns       168170
BM_VectorTestMulForcedIntrinsics/19          5164 ns         5147 ns       134830
BM_VectorTestMulForcedIntrinsics/20          3927 ns         3917 ns       179070
BM_VectorTestMulForcedIntrinsics/21          5481 ns         5449 ns       126196
BM_VectorTestMulForcedIntrinsics/22          5124 ns         5109 ns       138492
BM_VectorTestMulForcedIntrinsics/23          6142 ns         6125 ns       113071
BM_VectorTestMulForcedIntrinsics/24          4690 ns         4675 ns       150096
BM_VectorTestMulForcedIntrinsics/25          6423 ns         6398 ns       108462
BM_VectorTestMulForcedIntrinsics/26          6047 ns         6029 ns       117408
BM_VectorTestMulForcedIntrinsics/27          7150 ns         7128 ns        97901
BM_VectorTestMulForcedIntrinsics/28          5483 ns         5467 ns       129504
BM_VectorTestMulForcedIntrinsics/29          7416 ns         7390 ns        94167
BM_VectorTestMulForcedIntrinsics/30          6960 ns         6934 ns       102061
BM_VectorTestMulForcedIntrinsics/31          8073 ns         8043 ns        87555
BM_VectorTestMulForcedIntrinsics/32          6255 ns         6235 ns       113705
BM_VectorTestAddConstArraySizeFloat/1         161 ns          161 ns      4339090
BM_VectorTestAddConstArraySizeFloat/2         718 ns          716 ns       958914
BM_VectorTestAddConstArraySizeFloat/4        1500 ns         1496 ns       468059
BM_VectorTestAddConstArraySizeFloat/7        2334 ns         2326 ns       301694
BM_VectorTestAddConstArraySizeFloat/8        1655 ns         1651 ns       428569
BM_VectorTestAddConstArraySizeFloat/15       4224 ns         4214 ns       166108
BM_VectorTestAddConstArraySizeFloat/16       3229 ns         3219 ns       217681
BM_VectorTestAddForcedIntrinsics/1            164 ns          163 ns      4286279
BM_VectorTestAddForcedIntrinsics/2            858 ns          854 ns       795537
BM_VectorTestAddForcedIntrinsics/4            927 ns          924 ns       761731
BM_VectorTestAddForcedIntrinsics/7           2333 ns         2325 ns       301963
BM_VectorTestAddForcedIntrinsics/8           1658 ns         1654 ns       425574
BM_VectorTestAddForcedIntrinsics/15          4096 ns         4087 ns       171278
BM_VectorTestAddForcedIntrinsics/16          3245 ns         3236 ns       217538

*/

using namespace android::audio_utils::intrinsics;

static constexpr size_t kDataSize = 2048;

// exhaustively go from 1-32 channels.
static void TestFullArgs(benchmark::internal::Benchmark* b) {
    constexpr int kChannelCountMin = 1;
    constexpr int kChannelCountMax = 32;
    for (int i = kChannelCountMin; i <= kChannelCountMax; ++i) {
        b->Args({i});
    }
}

// selective channels to test.
static void TestArgs(benchmark::internal::Benchmark* b) {
    for (int i : { 1, 2, 4, 7, 8, 15, 16 }) {
        b->Args({i});
    }
}

// Macro test operator

#define OPERATOR(N) \
    *reinterpret_cast<V<F, N>*>(out) = Traits::func_( \
    *reinterpret_cast<const V<F, N>*>(in1), \
    *reinterpret_cast<const V<F, N>*>(in2)); \
    out += N; \
    in1 += N; \
    in2 += N;

// Macro to instantiate switch case statements.

#define INSTANTIATE(N) case N: mFunc = TestFunc<N>;  break;

template <typename Traits>
class Processor {
public:
    // shorthand aliases
    using F = typename Traits::data_t;
    template <typename T, int N>
    using V = typename Traits::template container_t<T, N>;
    template <size_t N>
    static void TestFunc(F* out, const F* in1, const F* in2, size_t count) {
        static_assert(sizeof(V<F, N>) == N * sizeof(F));
        for (size_t i = 0; i < count; ++i) {
            OPERATOR(N);
        }
    }

    Processor(int channelCount)
        : mChannelCount(channelCount) {

        if constexpr (Traits::loop_) {
            mFunc = [channelCount](F* out, const F* in1, const F* in2, size_t count) {
                for (size_t i = 0; i < count; ++i) {
                    for (size_t j = 0; j < channelCount; ++j) {
                        OPERATOR(1);
                    }
                }
            };
            return;
        }
        switch (channelCount) {
        INSTANTIATE(1);
        INSTANTIATE(2);
        INSTANTIATE(3);
        INSTANTIATE(4);
        INSTANTIATE(5);
        INSTANTIATE(6);
        INSTANTIATE(7);
        INSTANTIATE(8);
        INSTANTIATE(9);
        INSTANTIATE(10);
        INSTANTIATE(11);
        INSTANTIATE(12);
        INSTANTIATE(13);
        INSTANTIATE(14);
        INSTANTIATE(15);
        INSTANTIATE(16);
        INSTANTIATE(17);
        INSTANTIATE(18);
        INSTANTIATE(19);
        INSTANTIATE(20);
        INSTANTIATE(21);
        INSTANTIATE(22);
        INSTANTIATE(23);
        INSTANTIATE(24);
        INSTANTIATE(25);
        INSTANTIATE(26);
        INSTANTIATE(27);
        INSTANTIATE(28);
        INSTANTIATE(29);
        INSTANTIATE(30);
        INSTANTIATE(31);
        INSTANTIATE(32);
        }
    }

    void process(F* out, const F* in1, const F* in2, size_t frames) {
        mFunc(out, in1, in2, frames);
    }

    const size_t mChannelCount;
    /* const */ std::function<void(F*, const F*, const F*, size_t)> mFunc;
};

template <typename Traits>
static void BM_VectorTest(benchmark::State& state) {
    using F = typename Traits::data_t;
    const size_t channelCount = state.range(0);

    std::vector<F> input1(kDataSize * channelCount);
    std::vector<F> input2(kDataSize * channelCount);
    std::vector<F> output(kDataSize * channelCount);

    // Initialize input buffer and coefs with deterministic pseudo-random values
    std::minstd_rand gen(42);
    const F amplitude = 1.;
    std::uniform_real_distribution<> dis(-amplitude, amplitude);
    for (auto& in : input1) {
        in = dis(gen);
    }
    for (auto& in : input2) {
        in = dis(gen);
    }

    Processor<Traits> processor(channelCount);

    // Run the test
    while (state.KeepRunning()) {
        benchmark::DoNotOptimize(input1.data());
        benchmark::DoNotOptimize(input2.data());
        benchmark::DoNotOptimize(output.data());
        processor.process(output.data(), input1.data(), input2.data(), kDataSize);
        benchmark::ClobberMemory();
    }
    state.SetComplexityN(channelCount);
}

// Clang has an issue with -frelaxed-template-template-args where
// it may not follow the C++17 guidelines.  Use a traits struct to
// pass in parameters.

// Test using two loops.
struct LoopFloatTraits {
    template <typename F, int N>
    using container_t = internal_array_t<F, N>;
    using data_t = float;
    static constexpr bool loop_ = true;
};

// Test using two loops, the inner loop is constexpr size.
struct ConstArraySizeFloatTraits {
    template <typename F, int N>
    using container_t = internal_array_t<F, N>;
    using data_t = float;
    static constexpr bool loop_ = false;
};

// Test using intrinsics, if available.
struct ForcedIntrinsicsTraits {
    template <typename F, int N>
    using container_t = vector_hw_t<F, N>;
    using data_t = float;
    static constexpr bool loop_ = false;
};

// --- MULTIPLY

struct MulFunc {
    template <typename T>
    static T func_(T a, T b) { return vmul(a, b); }
};

struct MulLoopFloatTraits : public LoopFloatTraits, public MulFunc {};

static void BM_VectorTestMulLoopFloat(benchmark::State& state) {
    BM_VectorTest<MulLoopFloatTraits>(state);
}

struct MulConstArraySizeFloatTraits : public ConstArraySizeFloatTraits, public MulFunc {};

static void BM_VectorTestMulConstArraySizeFloat(benchmark::State& state) {
    BM_VectorTest<MulConstArraySizeFloatTraits>(state);
}

struct MulForcedIntrinsicsTraits : public ForcedIntrinsicsTraits, public MulFunc {};

static void BM_VectorTestMulForcedIntrinsics(benchmark::State& state) {
    BM_VectorTest<MulForcedIntrinsicsTraits>(state);
}

BENCHMARK(BM_VectorTestMulLoopFloat)->Apply(TestArgs);

BENCHMARK(BM_VectorTestMulConstArraySizeFloat)->Apply(TestFullArgs);

BENCHMARK(BM_VectorTestMulForcedIntrinsics)->Apply(TestFullArgs);

// --- ADD

struct AddFunc {
    template <typename T>
    static T func_(T a, T b) { return vadd(a, b); }
};

struct AddConstArraySizeFloatTraits : public ConstArraySizeFloatTraits, public AddFunc {};

static void BM_VectorTestAddConstArraySizeFloat(benchmark::State& state) {
    BM_VectorTest<AddConstArraySizeFloatTraits>(state);
}

struct AddForcedIntrinsicsTraits : public ForcedIntrinsicsTraits, public AddFunc {};

static void BM_VectorTestAddForcedIntrinsics(benchmark::State& state) {
    BM_VectorTest<AddForcedIntrinsicsTraits>(state);
}

BENCHMARK(BM_VectorTestAddConstArraySizeFloat)->Apply(TestArgs);

BENCHMARK(BM_VectorTestAddForcedIntrinsics)->Apply(TestArgs);

BENCHMARK_MAIN();
