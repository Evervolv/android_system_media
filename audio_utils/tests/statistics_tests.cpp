/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_statistics_tests"

#include <stdio.h>

#include <audio_utils/Statistics.h>
#include <gtest/gtest.h>


// Used to create compile-time reference constants for variance testing.
template <typename T>
class ConstexprStatistics {
public:
    template <size_t N>
    explicit constexpr ConstexprStatistics(const T (&a)[N])
        : mN{N}
        , mMax{android::audio_utils_max(a)}
        , mMin{android::audio_utils_min(a)}
        , mMean{android::audio_utils_sum(a) / mN}
        , mM2{android::audio_utils_sumSqDiff(a, mMean)}
        , mPopVariance{mM2 / mN}
        , mPopStdDev{android::audio_utils_sqrt(mPopVariance)}
        , mVariance{mM2 / (mN - 1)}
        , mStdDev{android::audio_utils_sqrt(mVariance)}
    { }

    constexpr int64_t getN() const { return mN; }
    constexpr T getMin() const { return mMin; }
    constexpr T getMax() const { return mMax; }
    constexpr double getWeight() const { return (double)mN; }
    constexpr double getMean() const { return mMean; }
    constexpr double getVariance() const { return mVariance; }
    constexpr double getStdDev() const { return mStdDev; }
    constexpr double getPopVariance() const { return mPopVariance; }
    constexpr double getPopStdDev() const { return mPopStdDev; }

private:
    const size_t mN;
    const T mMax;
    const T mMin;
    const double mMean;
    const double mM2;
    const double mPopVariance;
    const double mPopStdDev;
    const double mVariance;
    const double mStdDev;
};

class StatisticsTest : public testing::TestWithParam<const char *>
{
};

// find power of 2 that is small enough that it doesn't add to 1. due to finite mantissa.
template <typename T>
constexpr T smallp2() {
    T smallOne{};
    for (smallOne = T{1.}; smallOne + T{1.} > T{1.}; smallOne *= T(0.5));
    return smallOne;
}

// Our near expectation is 16x the bit that doesn't fit the mantissa.
// this works so long as we add values close in exponent with each other
// realizing that errors accumulate as the sqrt of N (random walk, lln, etc).
#define TEST_EXPECT_NEAR(e, v) \
        EXPECT_NEAR(e, v, abs((e) * std::numeric_limits<decltype(e)>::epsilon() * 8))

#define PRINT_AND_EXPECT_EQ(expected, expr) { \
    auto value = (expr); \
    printf("(%s): %s\n", #expr, std::to_string(value).c_str()); \
    if ((expected) == (expected)) { EXPECT_EQ((expected), (value)); } \
    EXPECT_EQ((expected) != (expected), (value) != (value)); /* nan check */\
}

#define PRINT_AND_EXPECT_NEAR(expected, expr) { \
    auto value = (expr); \
    printf("(%s): %s\n", #expr, std::to_string(value).c_str()); \
    TEST_EXPECT_NEAR((expected), (value)); \
}

template <typename T, typename S>
static void verify(const T &stat, const S &refstat) {
    EXPECT_EQ(refstat.getN(), stat.getN());
    EXPECT_EQ(refstat.getMin(), stat.getMin());
    EXPECT_EQ(refstat.getMax(), stat.getMax());
    TEST_EXPECT_NEAR(refstat.getWeight(), stat.getWeight());
    TEST_EXPECT_NEAR(refstat.getMean(), stat.getMean());
    TEST_EXPECT_NEAR(refstat.getVariance(), stat.getVariance());
    TEST_EXPECT_NEAR(refstat.getStdDev(), stat.getStdDev());
    TEST_EXPECT_NEAR(refstat.getPopVariance(), stat.getPopVariance());
    TEST_EXPECT_NEAR(refstat.getPopStdDev(), stat.getPopStdDev());
}

// Test against fixed reference

TEST(StatisticsTest, high_precision_sums)
{
    static const double simple[] = { 1., 2., 3. };

    double rssum = android::audio_utils_sum<double, double>(simple);
    PRINT_AND_EXPECT_EQ(6., rssum);
    double kssum = android::audio_utils_sum<double, android::KahanSum<double>>(simple);
    PRINT_AND_EXPECT_EQ(6., kssum);
    double nmsum = android::audio_utils_sum<double, android::NeumaierSum<double>>(simple);
    PRINT_AND_EXPECT_EQ(6., nmsum);

    double rs = 0.;
    android::KahanSum<double> ks;
    android::NeumaierSum<double> ns;

    // set counters to 1.
    rs += 1.;
    ks += 1.;
    ns += 1.;

    static constexpr double smallOne = std::numeric_limits<double>::epsilon() * 0.5;
    // add lots of small values
    static const int loop = 1000;
    for (int i = 0; i < loop; ++i) {
        rs += smallOne;
        ks += smallOne;
        ns += smallOne;
    }

    // remove 1.
    rs += -1.;
    ks += -1.;
    ns += -1.;

    const double totalAdded = smallOne * loop;
    printf("totalAdded: %lg\n", totalAdded);
    PRINT_AND_EXPECT_EQ(0., rs);            // normal count fails
    PRINT_AND_EXPECT_EQ(totalAdded, ks);    // kahan succeeds
    PRINT_AND_EXPECT_EQ(totalAdded, ns);    // neumaier succeeds

    // test case where kahan fails and neumaier method succeeds.
    static const double tricky[] = { 1e100, 1., -1e100 };

    rssum = android::audio_utils_sum<double, double>(tricky);
    PRINT_AND_EXPECT_EQ(0., rssum);
    kssum = android::audio_utils_sum<double, android::KahanSum<double>>(tricky);
    PRINT_AND_EXPECT_EQ(0., kssum);
    nmsum = android::audio_utils_sum<double, android::NeumaierSum<double>>(tricky);
    PRINT_AND_EXPECT_EQ(1., nmsum);
}

TEST(StatisticsTest, minmax_bounds)
{
    static constexpr double one[] = { 1. };

    PRINT_AND_EXPECT_EQ(std::numeric_limits<double>::infinity(),
            android::audio_utils_min(&one[0], &one[0]));

    PRINT_AND_EXPECT_EQ(-std::numeric_limits<double>::infinity(),
            android::audio_utils_max(&one[0], &one[0]));

    static constexpr int un[] = { 1 };

    PRINT_AND_EXPECT_EQ(std::numeric_limits<int>::max(),
            android::audio_utils_min(&un[0], &un[0]));

    PRINT_AND_EXPECT_EQ(std::numeric_limits<int>::min(),
            android::audio_utils_max(&un[0], &un[0]));

    double nanarray[] = { nan(""), nan(""), nan("") };

    PRINT_AND_EXPECT_EQ(std::numeric_limits<double>::infinity(),
            android::audio_utils_min(nanarray));

    PRINT_AND_EXPECT_EQ(-std::numeric_limits<double>::infinity(),
            android::audio_utils_max(nanarray));

    android::Statistics<double> s(nanarray);

    PRINT_AND_EXPECT_EQ(std::numeric_limits<double>::infinity(),
           s.getMin());

    PRINT_AND_EXPECT_EQ(-std::numeric_limits<double>::infinity(),
            s.getMax());
}

/*
TEST(StatisticsTest, sqrt_convergence)
{
    union {
        int i;
        float f;
    } u;

    for (int i = 0; i < INT_MAX; ++i) {
        u.i = i;
        const float f = u.f;
        if (!android::audio_utils_isnan(f)) {
            const float sf = android::audio_utils_sqrt(f);
            if ((i & (1 << 16) - 1) == 0) {
                printf("i: %d  f:%f  sf:%f\n", i, f, sf);
            }
        }
    }
}
*/

TEST(StatisticsTest, minmax_simple_array)
{
    static constexpr double ary[] = { -1.5, 1.5, -2.5, 2.5 };

    PRINT_AND_EXPECT_EQ(-2.5, android::audio_utils_min(ary));

    PRINT_AND_EXPECT_EQ(2.5, android::audio_utils_max(ary));

    static constexpr int ray[] = { -1, 1, -2, 2 };

    PRINT_AND_EXPECT_EQ(-2, android::audio_utils_min(ray));

    PRINT_AND_EXPECT_EQ(2, android::audio_utils_max(ray));
}

TEST(StatisticsTest, sqrt)
{
    // check doubles
    PRINT_AND_EXPECT_EQ(std::numeric_limits<double>::infinity(),
            android::audio_utils_sqrt(std::numeric_limits<double>::infinity()));

    PRINT_AND_EXPECT_EQ(std::nan(""),
            android::audio_utils_sqrt(-std::numeric_limits<double>::infinity()));

    PRINT_AND_EXPECT_NEAR(sqrt(std::numeric_limits<double>::epsilon()),
            android::audio_utils_sqrt(std::numeric_limits<double>::epsilon()));

    PRINT_AND_EXPECT_EQ(3.,
            android::audio_utils_sqrt(9.));

    PRINT_AND_EXPECT_EQ(0.,
            android::audio_utils_sqrt(0.));

    PRINT_AND_EXPECT_EQ(std::nan(""),
            android::audio_utils_sqrt(-1.));

    PRINT_AND_EXPECT_EQ(std::nan(""),
            android::audio_utils_sqrt(std::nan("")));

    // check floats
    PRINT_AND_EXPECT_EQ(std::numeric_limits<float>::infinity(),
            android::audio_utils_sqrt(std::numeric_limits<float>::infinity()));

    PRINT_AND_EXPECT_EQ(std::nanf(""),
            android::audio_utils_sqrt(-std::numeric_limits<float>::infinity()));

    PRINT_AND_EXPECT_NEAR(sqrtf(std::numeric_limits<float>::epsilon()),
            android::audio_utils_sqrt(std::numeric_limits<float>::epsilon()));

    PRINT_AND_EXPECT_EQ(2.f,
            android::audio_utils_sqrt(4.f));

    PRINT_AND_EXPECT_EQ(0.f,
            android::audio_utils_sqrt(0.f));

    PRINT_AND_EXPECT_EQ(std::nanf(""),
            android::audio_utils_sqrt(-1.f));

    PRINT_AND_EXPECT_EQ(std::nanf(""),
            android::audio_utils_sqrt(std::nanf("")));
}

TEST(StatisticsTest, stat_reference)
{
    // fixed reference compile time constants.
    static constexpr double data[] = {0.1, -0.1, 0.2, -0.3};
    static constexpr ConstexprStatistics<double> rstat(data); // use alpha = 1.
    static constexpr android::Statistics<double> stat{data};

    verify(stat, rstat);
}

TEST_P(StatisticsTest, stat_simple_char)
{
    const char *param = GetParam();

    android::Statistics<char> stat(0.9);
    android::ReferenceStatistics<char> rstat(0.9);

    // feed the string character by character to the statistics collectors.
    for (size_t i = 0; param[i] != '\0'; ++i) {
        stat.add(param[i]);
        rstat.add(param[i]);
    }

    printf("statistics for %s: %s\n", param, stat.toString().c_str());
    // verify that the statistics are the same
    verify(stat, rstat);
}

// find the variance of pet names as signed characters.
const char *pets[] = {"cat", "dog", "elephant", "mountain lion"};
INSTANTIATE_TEST_CASE_P(PetNameStatistics, StatisticsTest,
                        ::testing::ValuesIn(pets));
