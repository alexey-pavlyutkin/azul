#include <gtest/gtest.h>
#include "accessor.h"
#include <azul/heap.h>


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
            //using pointer_type = typename accessor_type::pointer_type;
            //using size_type = typename accessor_type::size_type;
            //using pool_block_header_type = typename accessor_type::pool_block_header_type;
            //using garbage_block_header_type = typename accessor_type::garbage_block_header_type;
        };

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename PolicyType, std::size_t BlockSize >
        struct set_block_size : public PolicyType
        {
            static constexpr std::size_t block_size = BlockSize;
        };

        template < typename PolicyType, std::size_t Granularity >
        struct set_granularity : public PolicyType
        {
            static constexpr std::size_t granularity = Granularity;
        };

        template < typename Policy, std::size_t Size, std::size_t Alignment, bool Positive, typename ExceptionType = std::exception >
        struct test_allocate_on_pool
        {
            using policy_type = Policy;
            static constexpr std::size_t size = Size;
            static constexpr std::size_t alignment = Alignment;
            static constexpr bool positive = Positive;
            using exception_type = ExceptionType;
        };

        using test_types = ::testing::Types <
            test_allocate_on_pool< default_policy, 0, 64, false, std::invalid_argument >,
#ifndef _DEBUG
            test_allocate_on_pool< default_policy, 1, 0, false, std::invalid_argument >,
#endif
            test_allocate_on_pool< default_policy, 1, 1, true >,
            test_allocate_on_pool< default_policy, 1, 2, true >,
            test_allocate_on_pool< default_policy, 1, 4, true >,
#ifndef _DEBUG
            test_allocate_on_pool< default_policy, 1, 5, true >,
            test_allocate_on_pool< default_policy, 1, 6, true >,
            test_allocate_on_pool< default_policy, 1, 7, true >,
#endif
            test_allocate_on_pool< default_policy, 1, 1024, true >,
            test_allocate_on_pool< default_policy, 2047, 1024, true >,
            test_allocate_on_pool< default_policy, 2048, 512, true >,
            test_allocate_on_pool< default_policy, 2049, 256, true >,
            test_allocate_on_pool< default_policy, default_policy::block_size - default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t), std::alignment_of_v< ptrdiff_t >, true >,
            test_allocate_on_pool< default_policy, default_policy::block_size - default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), 2 * std::alignment_of_v< ptrdiff_t >, true >,

            test_allocate_on_pool< set_granularity< default_policy, 128 >, 0, 64, false, std::invalid_argument >,
#ifndef _DEBUG
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 0, false, std::invalid_argument >,
#endif
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 1, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 2, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 4, true >,
#ifndef _DEBUG
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 5, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 6, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 7, true >,
#endif
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 1, 1024, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 2047, 1024, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 2048, 512, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, 2049, 256, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, set_granularity< default_policy, 128 >::block_size - set_granularity< default_policy, 128 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), std::alignment_of_v< ptrdiff_t >, true >,
            test_allocate_on_pool< set_granularity< default_policy, 128 >, set_granularity< default_policy, 128 >::block_size - set_granularity< default_policy, 128 >::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), 2 * std::alignment_of_v< ptrdiff_t >, true >
        >;

        TYPED_TEST_SUITE( test_heap, test_types, );

        //-----------------------------------------------------------------------------------------------------------------------------------------------------

        template < typename T >
        struct allocate_on_pool_impl
        {
            static void run( ... ) noexcept {}

            template < typename U >
            static void run(
                U&&,
                decltype( U::size ) size = U::size,
                decltype( U::alignment ) alignment = U::alignment,
                decltype( U::positive ) positive = U::positive ) noexcept
            {
                //EXPECT_NO_THROW( {
                    try
                    {
                        using heap_type = typename test_heap< U >::heap_type;
                        using accessor_type = typename test_heap< U >::accessor_type;

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

                        // check pointer is not NULL
                        EXPECT_TRUE( p );

                        // check beginning of aligned region
                        EXPECT_GE( reinterpret_cast< intptr_t >( p ), block_head + static_cast< ptrdiff_t >( sizeof( ptrdiff_t ) + sizeof( intptr_t ) ) );

                        // check alignment
                        EXPECT_EQ( 0, reinterpret_cast< intptr_t >( p ) % alignment );

                        // check that the pool didn't grow
                        EXPECT_EQ( pool_block, accessor_type::pool_begin( heap ) );

                        // get pointer to new unallocated space in the pool block
                        auto block_tile = pool_block->unallocated_;
                        EXPECT_GT( block_tile, block_head );

                        // check granularity
                        EXPECT_EQ( 0, block_tile % accessor_type::granularity );

                        // check that requested region entirely lays in pool block
                        EXPECT_LE( reinterpret_cast< intptr_t >( p ) + static_cast< ptrdiff_t >( size ), block_tile );

                        // check that the region lays in bound virtual space
                        std::memset( p, 0xCC, size );

                        // check block head field points to beginning of the block
                        auto head_ptr_alignment = std::alignment_of_v< intptr_t >;
                        auto block_head_ptr = reinterpret_cast< intptr_t* >( ( ( reinterpret_cast< intptr_t > ( p ) - sizeof( intptr_t ) ) / head_ptr_alignment ) * head_ptr_alignment );
                        EXPECT_EQ( block_head, *block_head_ptr );

                        // check block size field contaions valid value
                        EXPECT_EQ( block_tile  - block_head, *reinterpret_cast< ptrdiff_t* >( block_head ) );
                    }
                    catch ( const U::exception_type& )
                    {
                    }
                    catch ( ... )
                    {
                        FAIL();
                    }
                //} );
            }

            void operator()() const noexcept { run( T() ); }
        };

        TYPED_TEST( test_heap, allocate_on_pool )
        {
            allocate_on_pool_impl< TypeParam >()( );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------
    }
}

