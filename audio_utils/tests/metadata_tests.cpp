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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_metadata_tests"

#define METADATA_TESTING

#include <audio_utils/Metadata.h>
#include <gtest/gtest.h>
#include <iostream>
#include <log/log.h>

using namespace android::audio_utils::metadata;

// Preferred: Key in header - a constexpr which is created by the compiler.
inline constexpr CKey<std::string> ITS_NAME_IS("its_name_is");

// Not preferred: Key which is created at run-time.
inline const Key<std::string> MY_NAME_IS("my_name_is");

// The Metadata table
inline constexpr CKey<Data> TABLE("table");

#ifdef METADATA_TESTING
inline constexpr CKey<std::vector<Datum>> VECTOR("vector");
inline constexpr CKey<std::pair<Datum, Datum>> PAIR("pair");
inline constexpr CKey<MoveCount> MOVE_COUNT("MoveCount");
#endif

TEST(metadata_tests, basic_datum) {
    Datum d;
    d = "abc";
    //ASSERT_EQ("abc", std::any_cast<const char *>(d));
    ASSERT_EQ("abc", std::any_cast<std::string>(d));
    //d = std::vector<int>();

    Datum lore((int32_t) 10);
    d = lore;
    ASSERT_EQ(10, std::any_cast<int32_t>(d));

    // TODO: should we enable Datum to copy from std::any if the types
    // are correct?  The problem is how to signal failure.
    std::any arg = (int)1;
    // Datum invalid = arg; // this doesn't work.

    struct dummy {
        int value = 0;
    };

    // check apply with a void function
    {
        // try to apply with an invalid argument
        int value = 0;

        arg = dummy{}; // not an expected type, apply will fail with false.
        std::any result;

        ASSERT_FALSE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
            }, &arg, &result));

        ASSERT_EQ(0, value);              // never invoked.
        ASSERT_FALSE(result.has_value()); // no value returned.

        // try to apply with a valid argument.
        arg = (int)1;

        ASSERT_TRUE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
            }, &arg, &result));

        ASSERT_EQ(1, value);              // invoked once.
        ASSERT_FALSE(result.has_value()); // no value returned (function returns void).
    }

    // check apply with a function that returns 2.
    {
        int value = 0;
        arg = (int)1;
        std::any result;

        ASSERT_TRUE(primitive_metadata_types::apply([&](auto *t __unused) {
                value++;
                return (int32_t)2;
            }, &arg, &result));

        ASSERT_EQ(1, value);                          // invoked once.
        ASSERT_EQ(2, std::any_cast<int32_t>(result)); // 2 returned
    }

#ifdef METADATA_TESTING
    // Checks the number of moves versus copies as the datum flows through Data.
    // the counters should increment each time a MoveCount gets copied or
    // moved.

    //  Datum mc = MoveCount();

    Datum mc{MoveCount()};
    ASSERT_TRUE(1 >= std::any_cast<MoveCount>(mc).mMoveCount); // no more than 1 move.
    ASSERT_EQ(0, std::any_cast<MoveCount>(&mc)->mCopyCount);   // no copies
    ASSERT_EQ(1, std::any_cast<MoveCount>(mc).mCopyCount);     // Note: any_cast on value copies.


    // serialize
    ByteString bs;
    ASSERT_TRUE(copyToByteString(mc, bs));
    // deserialize
    size_t idx = 0;
    Datum parceled = datumFromByteString(bs, idx);

    // everything OK with the received data?
    ASSERT_EQ(bs.size(), idx);          // no data left over.
    ASSERT_TRUE(parceled.has_value());  // we have a value.

    // confirm no copies.
    ASSERT_TRUE(2 >= std::any_cast<MoveCount>(&parceled)->mMoveCount); // no more than 2 moves.
    ASSERT_EQ(0, std::any_cast<MoveCount>(&parceled)->mCopyCount);
#endif
}

TEST(metadata_tests, basic_data) {
    Data d;
    d.emplace("int32", (int32_t)1);
    d.emplace("int64", (int64_t)2);
    d.emplace("float", (float)3.1f);
    d.emplace("double", (double)4.11);
    d.emplace("string", "hello");
    d["string2"] = "world";

    // Put with typed keys
    d.put(MY_NAME_IS, "neo");
    d[ITS_NAME_IS] = "spot";

    ASSERT_EQ(1, std::any_cast<int32_t>(d["int32"]));
    ASSERT_EQ(2, std::any_cast<int64_t>(d["int64"]));
    ASSERT_EQ(3.1f, std::any_cast<float>(d["float"]));
    ASSERT_EQ(4.11, std::any_cast<double>(d["double"]));
    ASSERT_EQ("hello", std::any_cast<std::string>(d["string"]));
    ASSERT_EQ("world", std::any_cast<std::string>(d["string2"]));

    // Get with typed keys
    ASSERT_EQ("neo", *d.get_ptr(MY_NAME_IS));
    ASSERT_EQ("spot", *d.get_ptr(ITS_NAME_IS));

    ASSERT_EQ("neo", d[MY_NAME_IS]);
    ASSERT_EQ("spot", d[ITS_NAME_IS]);

    // Try round-trip conversion to a ByteString.
    ByteString bs;
    copyToByteString(d, bs);
    size_t index = 0;
    Datum datum = datumFromByteString(bs, index);

    ASSERT_TRUE(datum.has_value());

    // Check that we got a valid Data table back.
    Data data = *std::any_cast<Data>(&datum);
    ASSERT_EQ((size_t)8, data.size());

    ASSERT_EQ(1, std::any_cast<int32_t>(data["int32"]));
    ASSERT_EQ(2, std::any_cast<int64_t>(data["int64"]));
    ASSERT_EQ(3.1f, std::any_cast<float>(data["float"]));
    ASSERT_EQ(4.11, std::any_cast<double>(data["double"]));
    ASSERT_EQ("hello", std::any_cast<std::string>(data["string"]));
    ASSERT_EQ("neo", *data.get_ptr(MY_NAME_IS));
    ASSERT_EQ("spot", *data.get_ptr(ITS_NAME_IS));

    data[MY_NAME_IS] = "one";
    ASSERT_EQ("one", data[MY_NAME_IS]);

    // Keys are typed, so this fails to compile.
    // data->put(MY_NAME_IS, 10);

#ifdef METADATA_TESTING
    // Checks the number of moves versus copies as the Datum goes to
    // Data and then parceled and unparceled.
    // The counters should increment each time a MoveCount gets copied or
    // moved.
    {
        Data d2;
        d2[MOVE_COUNT] = MoveCount(); // should be moved.

        ASSERT_TRUE(1 >= d2[MOVE_COUNT].mMoveCount); // no more than one move.
        ASSERT_EQ(0, d2[MOVE_COUNT].mCopyCount);     // no copies

        ByteString bs = byteStringFromData(d2);
        Data d3 = dataFromByteString(bs);

        ASSERT_EQ(0, d3[MOVE_COUNT].mCopyCount);     // no copies
        ASSERT_TRUE(2 >= d3[MOVE_COUNT].mMoveCount); // no more than 2 moves after parceling
    }
#endif
}

TEST(metadata_tests, complex_data) {
    Data small;
    Data big;

    small[MY_NAME_IS] = "abc";
#ifdef METADATA_TESTING
    small[MOVE_COUNT] = MoveCount{};
#endif
    big[TABLE] = small;  // ONE COPY HERE of the MoveCount (embedded in small).

#ifdef METADATA_TESTING
    big[VECTOR] = std::vector<Datum>{small, small};
    big[PAIR] = std::make_pair<Datum, Datum>(small, small);
    ASSERT_EQ(1, big[TABLE][MOVE_COUNT].mCopyCount); // one copy done.
#endif

    // Try round-trip conversion to a ByteString.
    ByteString bs = byteStringFromData(big);
    Data data = dataFromByteString(bs);
#ifdef METADATA_TESTING
    ASSERT_EQ((size_t)3, data.size());
#else
    ASSERT_EQ((size_t)1, data.size());
#endif

    // Nested tables make sense.
    ASSERT_EQ("abc", data[TABLE][MY_NAME_IS]);

#ifdef METADATA_TESTING
    // TODO: Maybe we don't need the vector or the pair.
    ASSERT_EQ("abc", std::any_cast<Data>(data[VECTOR][1])[MY_NAME_IS]);
    ASSERT_EQ("abc", std::any_cast<Data>(data[PAIR].first)[MY_NAME_IS]);
    ASSERT_EQ(1, data[TABLE][MOVE_COUNT].mCopyCount); // no additional copies.
#endif
}
