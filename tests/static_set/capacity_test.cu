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

#include <cuco/static_set.cuh>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Static set capacity", "")
{
  using Key        = int32_t;
  using ProbeT     = cuco::experimental::double_hashing<1, cuco::default_hash_function<Key>>;
  using Equal      = thrust::equal_to<Key>;
  using AllocatorT = cuco::cuda_allocator<std::byte>;
  using StorageT   = cuco::experimental::aow_storage<2>;

  SECTION("zero capacity is allowed.")
  {
    auto constexpr gold_capacity = 4;

    using extent_type = cuco::experimental::extent<std::size_t, 0>;
    cuco::experimental::
      static_set<Key, extent_type, cuda::thread_scope_device, Equal, ProbeT, AllocatorT, StorageT>
        set{extent_type{}, cuco::empty_key<Key>{-1}};
    auto const capacity = set.capacity();
    REQUIRE(capacity == gold_capacity);

    auto ref                = set.ref(cuco::experimental::insert);
    auto const ref_capacity = ref.capacity();
    REQUIRE(ref_capacity == gold_capacity);
  }

  SECTION("negative capacity (ikr -_-||) is also allowed.")
  {
    auto constexpr gold_capacity = 4;

    using extent_type = cuco::experimental::extent<int32_t>;
    cuco::experimental::
      static_set<Key, extent_type, cuda::thread_scope_device, Equal, ProbeT, AllocatorT, StorageT>
        set{extent_type{-10}, cuco::empty_key<Key>{-1}};
    auto const capacity = set.capacity();
    REQUIRE(capacity == gold_capacity);

    auto ref                = set.ref(cuco::experimental::insert);
    auto const ref_capacity = ref.capacity();
    REQUIRE(ref_capacity == gold_capacity);
  }

  constexpr std::size_t num_keys{400};

  SECTION("Dynamic extent is evaluated at run time.")
  {
    auto constexpr gold_capacity = 422;  // 211 x 2

    using extent_type = cuco::experimental::extent<std::size_t>;
    cuco::experimental::
      static_set<Key, extent_type, cuda::thread_scope_device, Equal, ProbeT, AllocatorT, StorageT>
        set{num_keys, cuco::empty_key<Key>{-1}};
    auto const capacity = set.capacity();
    REQUIRE(capacity == gold_capacity);

    auto ref                = set.ref(cuco::experimental::insert);
    auto const ref_capacity = ref.capacity();
    REQUIRE(ref_capacity == gold_capacity);
  }

  SECTION("Dynamic extent is evaluated at run time.")
  {
    auto constexpr gold_capacity = 412;  // 103 x 2 x 2

    using probe = cuco::experimental::linear_probing<2, cuco::default_hash_function<Key>>;
    auto set    = cuco::experimental::static_set<Key,
                                              cuco::experimental::extent<std::size_t>,
                                              cuda::thread_scope_device,
                                              Equal,
                                              probe,
                                              AllocatorT,
                                              StorageT>{num_keys, cuco::empty_key<Key>{-1}};

    auto const capacity = set.capacity();
    REQUIRE(capacity == gold_capacity);

    auto ref                = set.ref(cuco::experimental::insert);
    auto const ref_capacity = ref.capacity();
    REQUIRE(ref_capacity == gold_capacity);
  }
}
