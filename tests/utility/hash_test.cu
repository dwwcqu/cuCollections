/*
 * Copyright (c) 2023, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils.hpp>

#include <cuco/detail/__config>
#include <cuco/hash_functions.cuh>

#include <thrust/device_vector.h>

#include <catch2/catch_test_macros.hpp>

template <int32_t Words>
struct large_key {
  constexpr __host__ __device__ large_key(int32_t value) noexcept
  {
    for (int32_t i = 0; i < Words; ++i) {
      data_[i] = value;
    }
  }

 private:
  int32_t data_[Words];
};

template <typename Hash>
__host__ __device__ bool check_hash_result(typename Hash::argument_type const& key,
                                           typename Hash::result_type seed,
                                           typename Hash::result_type expected) noexcept
{
  Hash h(seed);
  return (h(key) == expected);
}

template <typename OutputIter>
__global__ void check_hash_result_kernel_64(OutputIter result)
{
  result[0] = check_hash_result<cuco::xxhash_64<int32_t>>(0, 0, 4246796580750024372);
  result[1] = check_hash_result<cuco::xxhash_64<int32_t>>(0, 42, 3614696996920510707);
  result[2] = check_hash_result<cuco::xxhash_64<int32_t>>(42, 0, 15516826743637085169);
  result[3] = check_hash_result<cuco::xxhash_64<int32_t>>(123456789, 0, 9462334144942111946);

  result[4] = check_hash_result<cuco::xxhash_64<int64_t>>(0, 0, 3803688792395291579);
  result[5] = check_hash_result<cuco::xxhash_64<int64_t>>(0, 42, 13194218611613725804);
  result[6] = check_hash_result<cuco::xxhash_64<int64_t>>(42, 0, 13066772586158965587);
  result[7] = check_hash_result<cuco::xxhash_64<int64_t>>(123456789, 0, 14662639848940634189);

#if defined(CUCO_HAS_INT128)
  result[8] = check_hash_result<cuco::xxhash_64<__int128>>(123456789, 0, 7986913354431084250);
#endif

  result[9] = check_hash_result<cuco::xxhash_64<large_key<32>>>(123456789, 0, 2031761887105658523);
}

TEST_CASE("Test cuco::xxhash_64", "")
{
  // Reference hash values were computed using https://github.com/Cyan4973/xxHash
  SECTION("Check if host-generated hash values match the reference implementation.")
  {
    CHECK(check_hash_result<cuco::xxhash_64<int32_t>>(0, 0, 4246796580750024372));
    CHECK(check_hash_result<cuco::xxhash_64<int32_t>>(0, 42, 3614696996920510707));
    CHECK(check_hash_result<cuco::xxhash_64<int32_t>>(42, 0, 15516826743637085169));
    CHECK(check_hash_result<cuco::xxhash_64<int32_t>>(123456789, 0, 9462334144942111946));

    CHECK(check_hash_result<cuco::xxhash_64<int64_t>>(0, 0, 3803688792395291579));
    CHECK(check_hash_result<cuco::xxhash_64<int64_t>>(0, 42, 13194218611613725804));
    CHECK(check_hash_result<cuco::xxhash_64<int64_t>>(42, 0, 13066772586158965587));
    CHECK(check_hash_result<cuco::xxhash_64<int64_t>>(123456789, 0, 14662639848940634189));

#if defined(CUCO_HAS_INT128)
    CHECK(check_hash_result<cuco::xxhash_64<__int128>>(123456789, 0, 7986913354431084250));
#endif

    // 32*4=128-byte key to test the pipelined outermost hashing loop
    CHECK(check_hash_result<cuco::xxhash_64<large_key<32>>>(123456789, 0, 2031761887105658523));
  }

  SECTION("Check if device-generated hash values match the reference implementation.")
  {
    thrust::device_vector<bool> result(10);

    check_hash_result_kernel_64<<<1, 1>>>(result.begin());

    CHECK(cuco::test::all_of(result.begin(), result.end(), [] __device__(bool v) { return v; }));
  }
}

template <typename OutputIter>
__global__ void check_hash_result_kernel_32(OutputIter result)
{
  result[0] = check_hash_result<cuco::xxhash_32<int32_t>>(0, 0, 148298089);
  result[1] = check_hash_result<cuco::xxhash_32<int32_t>>(0, 42, 2132181312);
  result[2] = check_hash_result<cuco::xxhash_32<int32_t>>(42, 0, 1161967057);
  result[3] = check_hash_result<cuco::xxhash_32<int32_t>>(123456789, 0, 2987034094);

  result[4] = check_hash_result<cuco::xxhash_32<int64_t>>(0, 0, 3736311059);
  result[5] = check_hash_result<cuco::xxhash_32<int64_t>>(0, 42, 1076387279);
  result[6] = check_hash_result<cuco::xxhash_32<int64_t>>(42, 0, 2332451213);
  result[7] = check_hash_result<cuco::xxhash_32<int64_t>>(123456789, 0, 1561711919);

#if defined(CUCO_HAS_INT128)
  result[8] = check_hash_result<cuco::xxhash_32<__int128>>(123456789, 0, 1846633701);
#endif

  result[9] = check_hash_result<cuco::xxhash_32<large_key<32>>>(123456789, 0, 3715432378);
}

TEST_CASE("Test cuco::xxhash_32", "")
{
  // Reference hash values were computed using https://github.com/Cyan4973/xxHash
  SECTION("Check if host-generated hash values match the reference implementation.")
  {
    CHECK(check_hash_result<cuco::xxhash_32<int32_t>>(0, 0, 148298089));
    CHECK(check_hash_result<cuco::xxhash_32<int32_t>>(0, 42, 2132181312));
    CHECK(check_hash_result<cuco::xxhash_32<int32_t>>(42, 0, 1161967057));
    CHECK(check_hash_result<cuco::xxhash_32<int32_t>>(123456789, 0, 2987034094));

    CHECK(check_hash_result<cuco::xxhash_32<int64_t>>(0, 0, 3736311059));
    CHECK(check_hash_result<cuco::xxhash_32<int64_t>>(0, 42, 1076387279));
    CHECK(check_hash_result<cuco::xxhash_32<int64_t>>(42, 0, 2332451213));
    CHECK(check_hash_result<cuco::xxhash_32<int64_t>>(123456789, 0, 1561711919));

#if defined(CUCO_HAS_INT128)
    CHECK(check_hash_result<cuco::xxhash_32<__int128>>(123456789, 0, 1846633701));
#endif

    // 32*4=128-byte key to test the pipelined outermost hashing loop
    CHECK(check_hash_result<cuco::xxhash_32<large_key<32>>>(123456789, 0, 3715432378));
  }

  SECTION("Check if device-generated hash values match the reference implementation.")
  {
    thrust::device_vector<bool> result(10);

    check_hash_result_kernel_32<<<1, 1>>>(result.begin());

    CHECK(cuco::test::all_of(result.begin(), result.end(), [] __device__(bool v) { return v; }));
  }
}