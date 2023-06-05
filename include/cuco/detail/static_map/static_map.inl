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

#include <cuco/cuda_stream_ref.hpp>
#include <cuco/detail/error.hpp>
#include <cuco/detail/prime.hpp>
#include <cuco/detail/static_map/functors.cuh>
#include <cuco/detail/storage/counter_storage.cuh>
#include <cuco/detail/tuning.cuh>
#include <cuco/detail/utils.hpp>
#include <cuco/operator.hpp>
#include <cuco/static_map_ref.cuh>

#include <thrust/functional.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/iterator/transform_iterator.h>

#include <cub/device/device_reduce.cuh>
#include <cub/device/device_select.cuh>

#include <cstddef>

namespace cuco {
namespace experimental {

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
constexpr static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::
  static_map(Extent capacity,
             empty_key<Key> empty_key_sentinel,
             empty_value<T> empty_value_sentinel,
             KeyEqual const& pred,
             ProbingScheme const& probing_scheme,
             Allocator const& alloc,
             cuda_stream_ref stream)
  : static_map_impl_{std::make_unique<impl_type>(capacity,
                                                 empty_key_sentinel,
                                                 {empty_key_sentinel, empty_value_sentinel},
                                                 pred,
                                                 probing_scheme,
                                                 alloc,
                                                 stream)},
    empty_value_sentinel_{empty_value_sentinel}
{
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt>
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::size_type
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::insert(
  InputIt first, InputIt last, cuda_stream_ref stream)
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return 0; }

  auto counter =
    detail::counter_storage<size_type, thread_scope, allocator_type>{static_map_impl_->allocator()};
  counter.reset(stream);

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  auto const always_true = thrust::constant_iterator<bool>{true};
  detail::insert_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, always_true, thrust::identity{}, counter.data(), ref(op::insert));

  return counter.load_to_host(stream);
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::insert_async(
  InputIt first, InputIt last, cuda_stream_ref stream) noexcept
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return; }

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  auto const always_true = thrust::constant_iterator<bool>{true};
  detail::insert_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, always_true, thrust::identity{}, ref(op::insert));
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename StencilIt, typename Predicate>
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::size_type
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::insert_if(
  InputIt first, InputIt last, StencilIt stencil, Predicate pred, cuda_stream_ref stream)
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return 0; }

  auto counter =
    detail::counter_storage<size_type, thread_scope, allocator_type>{static_map_impl_->allocator()};
  counter.reset(stream);

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  detail::insert_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, stencil, pred, counter.data(), ref(op::insert));

  return counter.load_to_host(stream);
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename StencilIt, typename Predicate>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::
  insert_if_async(
    InputIt first, InputIt last, StencilIt stencil, Predicate pred, cuda_stream_ref stream) noexcept
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return; }

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  detail::insert_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, stencil, pred, ref(op::insert));
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::contains(
  InputIt first, InputIt last, OutputIt output_begin, cuda_stream_ref stream) const
{
  contains_async(first, last, output_begin, stream);
  stream.synchronize();
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::contains_async(
  InputIt first, InputIt last, OutputIt output_begin, cuda_stream_ref stream) const noexcept
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return; }

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  auto const always_true = thrust::constant_iterator<bool>{true};
  detail::contains_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, always_true, thrust::identity{}, output_begin, ref(op::contains));
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename StencilIt, typename Predicate, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::contains_if(
  InputIt first,
  InputIt last,
  StencilIt stencil,
  Predicate pred,
  OutputIt output_begin,
  cuda_stream_ref stream) const
{
  contains_if_async(first, last, stencil, pred, output_begin, stream);
  stream.synchronize();
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename StencilIt, typename Predicate, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::
  contains_if_async(InputIt first,
                    InputIt last,
                    StencilIt stencil,
                    Predicate pred,
                    OutputIt output_begin,
                    cuda_stream_ref stream) const noexcept
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return; }

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  detail::contains_if_n<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, stencil, pred, output_begin, ref(op::contains));
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::find(
  InputIt first, InputIt last, OutputIt output_begin, cuda_stream_ref stream) const
{
  find_async(first, last, output_begin, stream);
  stream.synchronize();
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename InputIt, typename OutputIt>
void static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::find_async(
  InputIt first, InputIt last, OutputIt output_begin, cuda_stream_ref stream) const
{
  auto const num_keys = cuco::detail::distance(first, last);
  if (num_keys == 0) { return; }

  auto const grid_size =
    (cg_size * num_keys + detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE - 1) /
    (detail::CUCO_DEFAULT_STRIDE * detail::CUCO_DEFAULT_BLOCK_SIZE);

  detail::find<cg_size, detail::CUCO_DEFAULT_BLOCK_SIZE>
    <<<grid_size, detail::CUCO_DEFAULT_BLOCK_SIZE, 0, stream>>>(
      first, num_keys, output_begin, ref(op::find));
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename OutputIt>
OutputIt
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::retrieve_all(
  OutputIt output_begin, cuda_stream_ref stream) const
{
  auto const is_filled = detail::slot_is_filled<key_type>(this->empty_key_sentinel());
  return static_map_impl_->retrieve_all(output_begin, is_filled, stream);
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::size_type
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::size(
  cuda_stream_ref stream) const noexcept
{
  return static_map_impl_->size(stream);
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
constexpr auto
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::capacity()
  const noexcept
{
  return static_map_impl_->capacity();
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
constexpr static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::key_type
static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::empty_key_sentinel()
  const noexcept
{
  return static_map_impl_->empty_key_sentinel();
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
constexpr static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::
  mapped_type
  static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::
    empty_value_sentinel() const noexcept
{
  return this->empty_value_sentinel_;
}

template <class Key,
          class T,
          class Extent,
          cuda::thread_scope Scope,
          class KeyEqual,
          class ProbingScheme,
          class Allocator,
          class Storage>
template <typename... Operators>
auto static_map<Key, T, Extent, Scope, KeyEqual, ProbingScheme, Allocator, Storage>::ref(
  Operators...) const noexcept
{
  static_assert(sizeof...(Operators), "No operators specified");
  return ref_type<Operators...>{cuco::empty_key<key_type>(static_map_impl_->empty_key_sentinel()),
                                static_map_impl_->predicate(),
                                static_map_impl_->probing_scheme(),
                                static_map_impl_->storage_ref()};
}
}  // namespace experimental
}  // namespace cuco
