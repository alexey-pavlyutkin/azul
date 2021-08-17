#include <gtest/gtest.h>
#include "accessor.h"
#include <azul/heap.h>
#include <stack>
#include <tuple>


namespace azul
{
    namespace ut
    {
        template < typename TestType >
        struct test_heap : ::testing::Test
        {
            using policy_type = typename TestType::policy_type;
            using heap_type = heap< policy_type >;
            using accessor_type = accessor< heap_type >;

            static void check_memory_piece( void* p, std::size_t size, std::size_t alignment )
            {
                EXPECT_TRUE( p );
                //
                EXPECT_EQ( 0, reinterpret_cast< intptr_t >( p ) % alignment );
                //
                static constexpr auto block_head_ptr_alignment = std::alignment_of_v< intptr_t >;
                auto block_head = *reinterpret_cast< intptr_t* >( ( ( reinterpret_cast< intptr_t >( p ) - sizeof( intptr_t ) ) / block_head_ptr_alignment ) * block_head_ptr_alignment );
                EXPECT_EQ( 0, block_head % policy_type::granularity );
                //
                auto block_size = *reinterpret_cast< ptrdiff_t* >( block_head );
                auto block_tile = static_cast< intptr_t >( block_head + block_size );
                EXPECT_EQ( 0, block_tile % policy_type::granularity );
                //
                EXPECT_GE( reinterpret_cast< intptr_t >( p ), block_head + static_cast< ptrdiff_t >( sizeof( ptrdiff_t ) + sizeof( intptr_t ) ) );
                EXPECT_LE( static_cast< intptr_t >( reinterpret_cast< intptr_t >( p ) + size ), block_tile );
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

        template < typename Policy, std::size_t Size, std::size_t Alignment, bool Positive, typename ExceptionType = std::exception >
        struct test_allocate_deallocate_on_pool
        {
            using policy_type = Policy;
            static constexpr std::size_t size = Size;
            static constexpr std::size_t alignment = Alignment;
            static constexpr bool positive = Positive;
            using exception_type = ExceptionType;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = 1;
            static constexpr std::size_t requested_alignment = 1;
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_1
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = 1;
            static constexpr std::size_t requested_alignment = 1;
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_2
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t );
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_3
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity / 2;
            static constexpr std::size_t requested_alignment = policy_type::granularity / 2;
            inline static const std::list< std::size_t > expected_garbage_state;
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_4
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_in_middle_of_garbage
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, 2 * policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_bottom_of_garbage
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, policy_type::granularity, 2 * policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_with_splitting_1
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { 3 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = 1;
            static constexpr std::size_t requested_alignment = 1;
            inline static const std::list< std::size_t > expected_garbage_state = { 2 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_top_of_garbage_with_splitting_2
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { 3 * policy_type::granularity, policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_in_middle_of_garbage_with_splitting
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, 3 * policy_type::granularity, policy_type::granularity };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_bottom_of_garbage_with_splitting
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = { policy_type::granularity, policy_type::granularity, 3 * policy_type::granularity, };
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = { policy_type::granularity, policy_type::granularity, policy_type::granularity };
        };

        template < typename Policy >
        struct test_allocate_on_garbage_search_depth_in
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth - 1, policy_type::granularity );
                result.emplace_back( 2 * policy_type::granularity );
                return std::move( result );
            }( );
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth - 1, policy_type::granularity );
                return std::move( result );
            }( );
        };

        template < typename Policy >
        struct test_allocate_on_garbage_search_depth_break
        {
            using policy_type = Policy;
            inline static const std::list< std::size_t > initial_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth, policy_type::granularity );
                result.emplace_back( 2 * policy_type::granularity );
                return std::move( result );
            }( );
            static constexpr std::size_t requested_size = policy_type::granularity - sizeof( ptrdiff_t ) - sizeof( intptr_t ) + 1;
            static constexpr std::size_t requested_alignment = sizeof( ptrdiff_t ) + sizeof( intptr_t );
            inline static const std::list< std::size_t > expected_garbage_state = []() {
                std::list< std::size_t > result( policy_type::garbage_search_depth, policy_type::granularity );
                return std::move( result );
            }( );
        };

        using test_types = ::testing::Types <

            // allocation on pool
            test_allocate_deallocate_on_pool< default_policy, 0, 64, false, std::invalid_argument >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< default_policy, 1, 0, false, std::invalid_argument >,
#endif
            test_allocate_deallocate_on_pool< default_policy, 1, 1, true >,
            test_allocate_deallocate_on_pool< default_policy, 1, 2, true >,
            test_allocate_deallocate_on_pool< default_policy, 1, 4, true >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< default_policy, 1, 5, true >,
            test_allocate_deallocate_on_pool< default_policy, 1, 6, true >,
            test_allocate_deallocate_on_pool< default_policy, 1, 7, true >,
#endif
            test_allocate_deallocate_on_pool< default_policy, 1, 1024, true >,
            test_allocate_deallocate_on_pool< default_policy, 2047, 1024, true >,
            test_allocate_deallocate_on_pool< default_policy, 2048, 512, true >,
            test_allocate_deallocate_on_pool< default_policy, 2049, 256, true >,
            test_allocate_deallocate_on_pool< default_policy, default_policy::block_size - default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t), std::alignment_of_v< ptrdiff_t >, true >,
            test_allocate_deallocate_on_pool< default_policy, default_policy::block_size - default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), 2 * std::alignment_of_v< ptrdiff_t >, true >,
            //
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 0, 64, false, std::invalid_argument >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 0, false, std::invalid_argument >,
#endif
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 1, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 2, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 4, true >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 5, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 6, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 7, true >,
#endif
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 1, 1024, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 2047, 1024, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 2048, 512, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, 2049, 256, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, set_granularity< default_policy, 128 >::block_size - set_granularity< default_policy, 128 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), std::alignment_of_v< ptrdiff_t >, true >,
            test_allocate_deallocate_on_pool< set_granularity< default_policy, 128 >, set_granularity< default_policy, 128 >::block_size - set_granularity< default_policy, 128 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), 2 * std::alignment_of_v< ptrdiff_t >, true >,
            //
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 0, 64, false, std::invalid_argument >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 0, false, std::invalid_argument >,
#endif
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 1, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 2, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 4, true >,
#ifndef _DEBUG
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 5, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 6, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 7, true >,
#endif
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 1, 1024, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2047, 1024, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2048, 512, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, 2049, 256, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, set_pool_block_size< default_policy, 1 << 20 >::block_size - set_pool_block_size< default_policy, 1 << 20 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), std::alignment_of_v< ptrdiff_t >, true >,
            test_allocate_deallocate_on_pool< set_pool_block_size< default_policy, 1 << 20 >, set_pool_block_size< default_policy, 1 << 20 >::block_size - set_pool_block_size< default_policy, 1 << 20 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), 2 * std::alignment_of_v< ptrdiff_t >, true >,

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
            test_allocate_on_garbage_search_depth_break< default_policy >
        >;

        TYPED_TEST_SUITE( test_heap, test_types, );

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct allocate_deallocate_on_pool_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::size ) size = U::size,
                decltype( U::alignment ) alignment = U::alignment,
                decltype( U::positive ) positive = U::positive,
                decltype( U::exception_type ) * e = nullptr
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                try
                {
                    heap_type heap;

                    // check that there is at least one pool block
                    auto pool_block_begin = accessor_type::pool_begin( heap );
                    EXPECT_NE( accessor_type::pool_end( heap ), pool_block_begin );

                    // get pointer to unallocated space in the pool block
                    auto pool_block = accessor_type::pool_begin( heap );
                    auto block_head = pool_block->unallocated_;

                    // check granularity
                    EXPECT_EQ( 0, block_head % accessor_type::granularity );

                    // allocate a peice
                    auto p = heap.allocate( size, alignment );

                    // the point shall be unreachable for negative tests
                    if ( !positive) GTEST_FAIL();

                    // check allocated piece
                    test_heap< U >::check_memory_piece( p, size, alignment );

                    // get pointer to new unallocated space in the pool block
                    auto block_tile = pool_block->unallocated_;
                    EXPECT_GT( block_tile, block_head );

                    // check granularity
                    EXPECT_EQ( 0, block_tile % accessor_type::granularity );

                    // deallocate region
                    heap.deallocate( p, size, alignment );

                    // check that garbage is not empty
                    auto garbage_head = accessor_type::garbage_begin( heap );
                    EXPECT_NE( garbage_head, accessor_type::garbage_end( heap ) );

                    // check that garbage points exactly to just deallocated block
                    EXPECT_EQ( static_cast< intptr_t >( garbage_head ), block_head );

                    // check size field
                    EXPECT_EQ( block_size, garbage_head->size_ );
                            
                    // check next field
                    EXPECT_FALSE( garbage_head->next_ );
                }
                catch ( const U::exception_type& )
                {
                }
                catch ( ... )
                {
                    FAIL();
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
        struct allocate_on_garbage_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::initial_garbage_state ) const & initial_garbage_state = U::initial_garbage_state,
                decltype( U::requested_size ) requested_size = U::requested_size,
                decltype( U::requested_alignment ) requested_alignment = U::requested_alignment,
                decltype( U::expected_garbage_state ) expected_garbage_state = U::expected_garbage_state
            ) noexcept
            {
                using heap_type = typename test_heap< U >::heap_type;
                using accessor_type = typename test_heap< U >::accessor_type;

                heap_type heap;

                // prepare initial garbage state
                std::stack< std::tuple< void*, std::size_t, std::size_t, std::size_t > > pieces;
                for ( auto block_size : initial_garbage_state )
                {
                    ASSERT_EQ( 0, block_size % test_heap< U >::policy_type::granularity );
                    auto piece_size = block_size - sizeof( intptr_t ) - sizeof( ptrdiff_t );
                    auto p = heap.allocate( piece_size, 1 );
                    ASSERT_TRUE( p );
                    pieces.emplace( p, piece_size, 1, block_size );
                }
                while ( !pieces.empty() )
                {
                    auto [p, piece_size, alignment, block_size] = pieces.top();
                    heap.deallocate( p, piece_size, 1 );
                    ASSERT_EQ( static_cast< ptrdiff_t >( block_size ), accessor_type::garbage_begin( heap )->size_ );
                    pieces.pop();
                }
                ASSERT_EQ( initial_garbage_state.size(), accessor_type::garbage_size( heap ) );

                // allocate requested memory piece
                auto p = heap.allocate( requested_size, requested_alignment );

                // test memory piece
                test_heap< U >::check_memory_piece( p, requested_size, requested_alignment );

                // check garbage state against expected
                EXPECT_EQ( expected_garbage_state.size(), accessor_type::garbage_size( heap ) );
                auto it = std::begin( expected_garbage_state );
                auto garbage_it = accessor_type::garbage_begin( heap );
                for ( ; it != std::end( expected_garbage_state ); ++it, ++garbage_it )
                {
                    EXPECT_EQ( static_cast< ptrdiff_t >( *it ), garbage_it->size_ );
                }

                heap.deallocate( p, requested_size, requested_alignment );
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, allocate_on_garbage_impl )
        {
            allocate_on_garbage_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------
    }
}

