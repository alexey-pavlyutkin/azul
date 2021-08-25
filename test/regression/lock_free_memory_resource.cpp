// MIT License
//
// Copyright( c ) 2021 Alexey Pavlyutkin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#include <gtest/gtest.h>
#include "accessor.h"
#include <lfmr/lock_free_memory_resource.h>
#include <list>
#include <stack>
#include <tuple>
#include <utility>
#include <limits>
#include <cstring>


namespace bits
{
    namespace ut
    {
        template < typename TestType >
        struct test_heap : ::testing::Test
        {
            using policy_type = typename TestType::policy_type;
            using heap_type = lock_free_memory_resource< policy_type >;
            using accessor_type = accessor< heap_type >;
            using pointer_type = typename accessor_type::pointer_type;
            using size_type = typename accessor_type::size_type;

            static size_type calculate_block_size( std::size_t piece_size, std::size_t piece_alignment ) noexcept
            {
                return ( ( accessor_type::piece_internal_fields_size / piece_alignment ) * piece_alignment + piece_size / accessor_type::granularity ) * accessor_type::granularity;
            }

            static std::tuple< pointer_type, size_type > get_piece_internal_fields( void* piece ) noexcept
            {
                auto block_head = accessor_type::get_block_header_ptr_ref( reinterpret_cast< pointer_type >( piece ) );
                auto block_size = *reinterpret_cast< size_type* >( block_head );
                return { block_head, block_size };
            }

            static void check_memory_piece( void* p, std::size_t size, std::size_t alignment )
            {
                EXPECT_TRUE( p );
                //
                EXPECT_EQ( 0, reinterpret_cast< pointer_type >( p ) % alignment );
                //
                auto [block_head, block_size] = get_piece_internal_fields( p );
                EXPECT_EQ( 0, block_head % policy_type::granularity );
                //
                auto block_tile = static_cast< pointer_type >( block_head + block_size );
                EXPECT_EQ( 0, block_tile % policy_type::granularity );
                //
                EXPECT_GE( reinterpret_cast< pointer_type >( p ), block_head + static_cast< size_type >( sizeof( size_type ) + sizeof( pointer_type ) ) );
                EXPECT_LE( static_cast< pointer_type >( reinterpret_cast< pointer_type >( p ) + size ), block_tile );
                //
                std::memset( p, 0xCC, size );
            }
        };

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename PolicyType, std::size_t BlockSize >
        struct set_pool_block_size : public PolicyType
        {
            static constexpr std::size_t block_size = BlockSize;
        };

        template < typename PolicyType, std::size_t Granularity >
        struct set_granularity : public PolicyType
        {
            static constexpr std::size_t granularity = Granularity;
        };

        template < typename PolicyType, std::size_t GarbageSearchDepth >
        struct set_garbage_search_depth : public PolicyType
        {
            static constexpr std::size_t garbage_search_depth = GarbageSearchDepth;
        };

        template < typename Policy, std::size_t Size, std::size_t Alignment, typename ExceptionType >
        struct test_invalid_arguments
        {
            using policy_type = Policy;
            static constexpr bool is_invalid_arguments_test = true;
            static constexpr std::size_t requested_size = Size;
            static constexpr std::size_t requested_alignment = Alignment;
            using exception_type = ExceptionType;
        };

        static constexpr std::size_t use_max_piece_size_on_pool = std::numeric_limits< std::size_t >::max();

        template < typename Policy, std::size_t Size, std::size_t Alignment >
        struct test_allocate_deallocate_on_pool
        {
            using policy_type = Policy;
            static constexpr bool is_pool_allocation_test = true;
            inline static const std::size_t requested_size = ( Size == use_max_piece_size_on_pool ) ? accessor< lock_free_memory_resource< policy_type > >::pool_block_capacity - sizeof( ptrdiff_t) - sizeof( intptr_t ) : Size;
            static constexpr std::size_t requested_alignment = Alignment;
        };

        template < typename Policy, std::size_t Size, std::size_t Alignment >
        struct test_grow_pool
        {
            using policy_type = Policy;
            static constexpr bool is_grow_pool_test = true;
            inline static const std::size_t requested_size = Size;
            static constexpr std::size_t requested_alignment = Alignment;
        };

        template < typename Policy >
        struct test_trace_pool
        {
            using policy_type = Policy;
            static constexpr bool is_trace_pool_test = true;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_1
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = 1;
            static constexpr std::size_t requested_alignment = 1;
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_2
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t );
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_3
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity / 2;
            static constexpr std::size_t requested_alignment = policy_type::granularity / 2;
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_4
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_in_middle_of_garbage
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, 2 * policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_bottom_of_garbage
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, policy_type::granularity, 2 * policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_with_splitting_1
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { 3 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = 1;
            static constexpr std::size_t requested_alignment = 1;
            inline static const std::list< std::size_t > expected_garbage_state = { 2 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_with_splitting_2
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { 3 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_in_middle_of_garbage_with_splitting
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, 3 * policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_bottom_of_garbage_with_splitting
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, policy_type::granularity, 3 * policy_type::granularity, };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_garbage_search_depth_in
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth, policy_type::granularity );
                result.emplace_back( 2 * policy_type::granularity );
                return result;
            }( );
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth, policy_type::granularity );
                return result;
            }( );
        };

        template < typename Policy >
        struct test_allocate_on_garbage_search_depth_break
        {
            using policy_type = Policy;
            static constexpr bool is_allocate_on_garbage_test = true;
            inline static const std::list< std::size_t > initial_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth + 1, policy_type::granularity );
                result.emplace_back( 2 * policy_type::granularity );
                return result;
            }( );
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth + 1, policy_type::granularity );
                result.emplace_back( 2 * policy_type::granularity );
                return result;
            }( );
        };

        template < typename Policy >
        struct test_allocate_deallocate_large_block
        {
            using policy_type = Policy;
            using accessor_type = accessor< lock_free_memory_resource < policy_type > >;
            static constexpr bool is_large_block_test = true;
            inline static const std::size_t requested_size = accessor_type::pool_block_size - accessor_type::pool_block_header_size - accessor_type::piece_internal_fields_size + 1;
            static constexpr std::size_t requested_alignment = 1;
        };

        using test_types = ::testing::Types <

            // test on invalid arguments
            test_invalid_arguments< default_policy, 0, 1, std::invalid_argument >,
#ifndef _DEBUG
            test_invalid_arguments< default_policy, 1, 0, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 3, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 5, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 6, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 7, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 9, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 15, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 17, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 255, std::invalid_argument >,
            test_invalid_arguments< default_policy, 1, 257, std::invalid_argument >,
#endif
            test_invalid_arguments< default_policy, 1, 1 << 17, std::invalid_argument >,
            test_invalid_arguments< set_pool_block_size< default_policy, 1 << 17 >, 1, 1 << 18, std::invalid_argument >,
            test_invalid_arguments< default_policy, std::numeric_limits< ptrdiff_t >::max(), 1, std::bad_alloc >,

            // allocation on pool
            test_allocate_deallocate_on_pool< default_policy, 1, 1 >,
            test_allocate_deallocate_on_pool< default_policy, 1, 2 >,
            test_allocate_deallocate_on_pool< default_policy, 1, 4 >,
            test_allocate_deallocate_on_pool< default_policy, 1, 1024 >,
            test_allocate_deallocate_on_pool< default_policy, 2047, 1024 >,
            test_allocate_deallocate_on_pool< default_policy, 2048, 512 >,
            test_allocate_deallocate_on_pool< default_policy, 2049, 256 >,
            test_allocate_deallocate_on_pool< default_policy, use_max_piece_size_on_pool, 1 >,
            test_allocate_deallocate_on_pool< default_policy, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > >,
            test_allocate_deallocate_on_pool< default_policy, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > + std::alignment_of_v< intptr_t > >,

            // with other granularity
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 1, 1 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 1, 2 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 1, 4 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 1, 1024 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 2047, 1024 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 2048, 512 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, 2049, 256 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, use_max_piece_size_on_pool, 1 >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 0x100 >, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > +std::alignment_of_v< intptr_t > >,

            // with other pool block size
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 1 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 2 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 4 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 1024 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2047, 1024 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2048, 512 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2049, 256 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, use_max_piece_size_on_pool, 1 >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, use_max_piece_size_on_pool, std::alignment_of_v< ptrdiff_t > +std::alignment_of_v< intptr_t > >,

            // test pool grow
            test_grow_pool< default_policy, default_policy::block_size / 2 - sizeof( ptrdiff_t ) - sizeof( intptr_t ), 1 >,
            test_grow_pool< default_policy, default_policy::block_size / 2 - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1, 1 >,
            test_grow_pool< set_pool_block_size< default_policy, 1 << 20 >, set_pool_block_size< default_policy, 1 << 20 >::block_size / 2 - sizeof( ptrdiff_t ) - sizeof( intptr_t ), 1 >,
            test_grow_pool< set_pool_block_size< default_policy, 1 << 20 >, set_pool_block_size< default_policy, 1 << 20 >::block_size / 2 - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1, 1 >,

            // test pool journey
            test_trace_pool< default_policy >,

            // allocation on garbage
            test_allocate_on_top_of_garbage_1< default_policy >,
            test_allocate_on_top_of_garbage_2< default_policy >,
            test_allocate_on_top_of_garbage_3< default_policy >,
            test_allocate_on_top_of_garbage_4< default_policy >,
            test_allocate_in_middle_of_garbage< default_policy >,
            test_allocate_on_bottom_of_garbage< default_policy >,
            test_allocate_on_top_of_garbage_with_splitting_1< default_policy >,
            test_allocate_on_top_of_garbage_with_splitting_2< default_policy >,
            test_allocate_in_middle_of_garbage_with_splitting< default_policy >,
            test_allocate_on_bottom_of_garbage_with_splitting< default_policy >,

            // other granularity
            test_allocate_on_top_of_garbage_1< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_top_of_garbage_2< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_top_of_garbage_3< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_top_of_garbage_4< set_granularity< default_policy, 0x100 > >,
            test_allocate_in_middle_of_garbage< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_bottom_of_garbage< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_top_of_garbage_with_splitting_1< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_top_of_garbage_with_splitting_2< set_granularity< default_policy, 0x100 > >,
            test_allocate_in_middle_of_garbage_with_splitting< set_granularity< default_policy, 0x100 > >,
            test_allocate_on_bottom_of_garbage_with_splitting< set_granularity< default_policy, 0x100 > >,

            // check garbage search depth
            test_allocate_on_garbage_search_depth_in< set_garbage_search_depth< default_policy, 4 > >,
            test_allocate_on_garbage_search_depth_break< set_garbage_search_depth< default_policy, 4 > >,
            test_allocate_on_garbage_search_depth_in< default_policy >,
            test_allocate_on_garbage_search_depth_break< default_policy >,

            //
            test_allocate_deallocate_large_block< default_policy >
        >;

        TYPED_TEST_SUITE( test_heap, test_types, );

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct invalid_agruments_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_invalid_arguments_test ) = U::is_invalid_arguments_test,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;

                try
                {
                    heap_type lock_free_memory_resource;
                    [[maybe_unused]]auto p = lock_free_memory_resource.allocate( requested_size, requested_alignment );
                    GTEST_FAIL();
                }
                catch ( const typename U::exception_type& )
                {
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, invalid_agruments )
        {
            invalid_agruments_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct allocate_deallocate_on_pool_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_pool_allocation_test ) = U::is_pool_allocation_test,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type lock_free_memory_resource;

                    // check that there is at least one pool block
                    auto pool_block_begin = accessor_type::pool_begin( lock_free_memory_resource );
                    EXPECT_NE( accessor_type::pool_end( lock_free_memory_resource ), pool_block_begin );

                    // get pointer to unallocated space in the pool block
                    auto pool_block = accessor_type::pool_begin( lock_free_memory_resource );
                    auto unallocated = pool_block->unallocated_.load();

                    // allocate a peice
                    auto p = lock_free_memory_resource.allocate( requested_size, requested_alignment );

                    // check allocated piece
                    test_heap< U >::check_memory_piece( p, requested_size, requested_alignment );

                    // get pointer to new unallocated space in the pool block
                    auto [ block_head, block_size ] = test_heap< U >::get_piece_internal_fields( p );
                    auto block_tile = block_head + block_size;
                    EXPECT_EQ( unallocated, block_head );
                    EXPECT_EQ( block_tile, pool_block->unallocated_ );

                    // deallocate region
                    lock_free_memory_resource.deallocate( p, requested_size, requested_alignment );

                    // check that garbage is not empty
                    auto garbage_head = accessor_type::garbage_begin( lock_free_memory_resource );
                    EXPECT_NE( garbage_head, accessor_type::garbage_end( lock_free_memory_resource ) );

                    // check that garbage points exactly to just deallocated block
                    EXPECT_EQ( static_cast< typename accessor_type::pointer_type >( garbage_head ), block_head );

                    // check size field
                    EXPECT_EQ( block_tile - block_head, garbage_head->size_ );
                            
                    // check next field
                    EXPECT_FALSE( garbage_head->next_ );
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, allocate_deallocate_on_pool )
        {
            allocate_deallocate_on_pool_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct grow_pool_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_grow_pool_test ) = U::is_grow_pool_test,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type lock_free_memory_resource;

                    // allocate a half of the 1st pool block
                    auto lock_sz = accessor_type::pool_block_size / 2 - accessor_type::pool_block_header_size - accessor_type::piece_internal_fields_size;
                    auto lock_block = lock_free_memory_resource.allocate( lock_sz, 1 );
                    ASSERT_TRUE( lock_block );

                    // get free space in the 1st pool block
                    auto pool_head = accessor_type::pool_begin( lock_free_memory_resource );
                    auto block_free_space = static_cast< typename accessor_type::pointer_type >( pool_head ) + accessor_type::pool_block_size - pool_head->unallocated_;

                    // allocate requested size
                    auto p = lock_free_memory_resource.allocate( requested_size, requested_alignment );

                    // check allocated memory piece
                    test_heap< U >::check_memory_piece( p, requested_size, requested_alignment );

                    // get memory block for the piece
                    auto [ block_head, block_size] = test_heap< U >::get_piece_internal_fields( p );

                    // if the 1st pool block could fit the piece
                    if ( block_size <= block_free_space )
                    {
                        // check that pool didn't grow
                        EXPECT_EQ( pool_head, accessor_type::pool_begin( lock_free_memory_resource ) );
                    }
                    else
                    {
                        // else make sure the pool has grown
                        EXPECT_EQ( static_cast< typename accessor_type::pointer_type >( pool_head ), accessor_type::pool_begin( lock_free_memory_resource )->next_ );
                    }

                    lock_free_memory_resource.deallocate( p, requested_size, requested_alignment );
                    lock_free_memory_resource.deallocate( lock_block, lock_sz, 1 );
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, grow_pool )
        {
            grow_pool_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct pool_trace_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_trace_pool_test ) = U::is_trace_pool_test
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type lock_free_memory_resource;

                    std::size_t half_block_sz = accessor_type::pool_block_size / 2 - accessor_type::pool_block_header_size - accessor_type::piece_internal_fields_size;
                    std::size_t full_block_sz = accessor_type::pool_block_size - accessor_type::pool_block_header_size - accessor_type::piece_internal_fields_size;

                    // prepare initial pool state
                    std::list< std::tuple< std::size_t, std::size_t, void* > > pieces;
                    for ( auto sz : { full_block_sz, half_block_sz, full_block_sz } )
                    {
                        void* piece = lock_free_memory_resource.allocate( sz, 1 );
                        ASSERT_TRUE( piece );
                        pieces.emplace_back( sz, 1, piece );
                    }
                    auto pool_size = accessor_type::pool_size( lock_free_memory_resource );
                    ASSERT_EQ( 3, pool_size );

                    auto capable_pool_block = ++accessor_type::pool_begin( lock_free_memory_resource );
                    auto unallocated = capable_pool_block->unallocated_.load();

                    // allocate a piece that can fit into 2nd pool block
                    std::size_t sz = accessor_type::pool_block_size / 2 - accessor_type::piece_internal_fields_size;
                    auto p = lock_free_memory_resource.allocate( accessor_type::pool_block_size / 2 - accessor_type::piece_internal_fields_size, 1 );
                    EXPECT_TRUE( p );
                    pieces.emplace_back( sz, 1, p );

                    // check allocated piece
                    test_heap< U >::check_memory_piece( p, sz, 1 );

                    // make sure exactly 2nd pool block fits the piece
                    EXPECT_EQ( pool_size, accessor_type::pool_size( lock_free_memory_resource ) );
                    auto [block_head, block_size] = test_heap< U >::get_piece_internal_fields( p );
                    EXPECT_LE( unallocated, block_head );
                    EXPECT_LE( block_head + block_size, capable_pool_block->unallocated_ );

                    for ( auto [size, alignment, piece] : pieces )
                    {
                        lock_free_memory_resource.deallocate( piece, size, alignment );
                    }
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, pool_trace )
        {
            pool_trace_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct allocate_on_garbage_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_allocate_on_garbage_test ) = U::is_allocate_on_garbage_test,
                const decltype( U::initial_garbage_state ) & initial_garbage_state = U::initial_garbage_state,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment,
                const decltype( U::expected_garbage_state ) & expected_garbage_state = U::expected_garbage_state
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type lock_free_memory_resource;

                    // prepare initial garbage state
                    std::stack< std::tuple< void*, std::size_t, std::size_t, std::size_t > > pieces;
                    for ( auto block_size : initial_garbage_state )
                    {
                        ASSERT_EQ( 0, block_size % test_heap< U >::policy_type::granularity );
                        auto piece_size = block_size - accessor_type::piece_internal_fields_size;
                        auto p = lock_free_memory_resource.allocate( piece_size, 1 );
                        ASSERT_TRUE( p );
                        pieces.emplace( p, piece_size, 1, block_size );
                    }
                    while ( !pieces.empty() )
                    {
                        auto [p, piece_size, alignment, block_size] = pieces.top();
                        lock_free_memory_resource.deallocate( p, piece_size, 1 );
                        ASSERT_EQ( static_cast< typename accessor_type::size_type >( block_size ), accessor_type::garbage_begin( lock_free_memory_resource )->size_ );
                        pieces.pop();
                    }
                    ASSERT_EQ( initial_garbage_state.size(), accessor_type::garbage_size( lock_free_memory_resource ) );

                    // allocate requested memory piece
                    auto p = lock_free_memory_resource.allocate( requested_size, requested_alignment );

                    // test memory piece
                    test_heap< U >::check_memory_piece( p, requested_size, requested_alignment );

                    // check garbage state against expected
                    EXPECT_EQ( expected_garbage_state.size(), accessor_type::garbage_size( lock_free_memory_resource ) );
                    auto it = std::begin( expected_garbage_state );
                    auto garbage_it = accessor_type::garbage_begin( lock_free_memory_resource );
                    for ( ; it != std::end( expected_garbage_state ); ++it, ++garbage_it )
                    {
                        EXPECT_EQ( static_cast< typename accessor_type::pointer_type >( *it ), garbage_it->size_ );
                    }

                    lock_free_memory_resource.deallocate( p, requested_size, requested_alignment );
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, allocate_on_garbage )
        {
            allocate_on_garbage_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct allocate_deallocate_large_block_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::is_large_block_test ) = U::is_large_block_test,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type lock_free_memory_resource;

                    // get current pool state
                    auto pool_head = accessor_type::pool_begin( lock_free_memory_resource );
                    auto unallocated = pool_head->unallocated_.load();

                    // allocate requested memory piece
                    auto p = lock_free_memory_resource.allocate( requested_size, requested_alignment );

                    // test memory piece
                    test_heap< U >::check_memory_piece( p, requested_size, requested_alignment );
                    
                    // check that pool stays untouched
                    EXPECT_EQ( pool_head, accessor_type::pool_begin( lock_free_memory_resource ) );
                    EXPECT_EQ( unallocated, pool_head->unallocated_ );

                    // get block head and size
                    auto block_head = accessor_type::get_block_header_ptr_ref( reinterpret_cast< typename accessor_type::pointer_type >( p ) );
                    auto block_size = *reinterpret_cast< typename accessor_type::size_type* >( block_head );

                    // gotcha! deallocate piece
                    lock_free_memory_resource.deallocate( p, requested_size, requested_alignment );

                    // check that allocated virtual space was released
                    p = accessor_type::virtual_alloc( block_size, reinterpret_cast< void* >( block_head ) );
                    accessor_type::virtual_free( p, block_size );
                }
                catch ( ... )
                {
                    GTEST_FAIL();
                }
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, allocate_deallocate_large_block )
        {
            allocate_deallocate_large_block_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        TYPED_TEST( test_heap, compare_heaps )
        {
            using heap_type = typename test_heap< TypeParam >::heap_type;
            heap_type h1, h2;
            EXPECT_TRUE( h1.is_equal( h1 ) );
            EXPECT_FALSE( h1.is_equal( h2 ) );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------
    }
}

