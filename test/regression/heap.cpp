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
        struct set_pool_block_size : public PolicyType
        {
            static constexpr std::size_t block_size = BlockSize;
        };

        template < typename PolicyType, std::size_t Granularity >
        struct set_granularity : public PolicyType
        {
            static constexpr std::size_t granularity = Granularity;
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

        template < typename Policy, std::size_t BlockSize, std::size_t Size, std::size_t Alignment, bool Positive >
        struct test_allocate_on_garbage
        {
            using policy_type = Policy;
            static constexpr std::size_t block_size = BlockSize;
            static constexpr std::size_t size = Size;
            static constexpr std::size_t alignment = Alignment;
            static constexpr bool allocation_on_garbage_expected = Positive;
        };

        template < typename Policy, std::size_t BlockSize, std::size_t Size, std::size_t Alignment, bool Positive >
        struct test_allocate_on_garbage_with_splitting
        {
            using policy_type = Policy;
            static constexpr std::size_t block_size = BlockSize;
            static constexpr std::size_t size = Size;
            static constexpr std::size_t alignment = Alignment;
            static constexpr bool splitting_expected = Positive;
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

            // allocation on garbage without splitting
            test_allocate_on_garbage< default_policy, default_policy::granularity, 1, 1, true >,
            test_allocate_on_garbage< default_policy, default_policy::granularity, default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), sizeof( intptr_t ), true >,
            test_allocate_on_garbage< default_policy, default_policy::granularity, default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), sizeof( intptr_t ) + sizeof( ptrdiff_t ), true >,
            test_allocate_on_garbage< default_policy, default_policy::granularity, default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ) + 1, sizeof( intptr_t ), false >,
#ifndef _DEBUG
            test_allocate_on_garbage< default_policy, default_policy::granularity, default_policy::granularity - sizeof( intptr_t ) - sizeof( ptrdiff_t ), sizeof( intptr_t ) + sizeof( ptrdiff_t ) + 1, false >,
#endif

            // allocation on garbage with splitting
            test_allocate_on_garbage_with_splitting< default_policy, 2 * default_policy::granularity, 1, 1, true >
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
                        EXPECT_GE( reinterpret_cast< intptr_t >( p ), block_head + static_cast< ptrdiff_t >( sizeof( ptrdiff_t ) + sizeof( intptr_t ) ) );
                        EXPECT_LE( reinterpret_cast< intptr_t >( p ) + static_cast< ptrdiff_t >( size ), block_tile );

                        // check that the region lays in bound virtual space
                        std::memset( p, 0xCC, size );

                        // check block head field points to beginning of the block
                        auto head_ptr_alignment = std::alignment_of_v< intptr_t >;
                        auto block_head_ptr = reinterpret_cast< intptr_t* >( ( ( reinterpret_cast< intptr_t > ( p ) - sizeof( intptr_t ) ) / head_ptr_alignment ) * head_ptr_alignment );
                        EXPECT_EQ( block_head, *block_head_ptr );

                        // check block size field contaions valid value
                        auto block_size = block_tile - block_head;
                        EXPECT_EQ( block_size, *reinterpret_cast< ptrdiff_t* >( block_head ) );

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
                //} );
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
                decltype( U::block_size ) block_size = U::block_size,
                decltype( U::size ) size = U::size,
                decltype( U::alignment ) alignment = U::alignment,
                decltype( U::allocation_on_garbage_expected ) allocation_on_garbage_expected = U::allocation_on_garbage_expected ) noexcept
            {
                //EXPECT_NO_THROW( {
                try
                {
                    using heap_type = typename test_heap< U >::heap_type;
                    using accessor_type = typename test_heap< U >::accessor_type;

                    heap_type heap;
                    {
                        // allocate block an pool and deallocate it
                        auto p = heap.allocate( block_size - sizeof( ptrdiff_t ) - sizeof( intptr_t ), 1 );
                        ASSERT_TRUE( p );
                        heap.deallocate( p, size, 1 );

                        // make sure there is right block on garbage
                        ASSERT_NE( accessor_type::garbage_begin( heap ), accessor_type::garbage_end( heap ) );
                        ASSERT_EQ( static_cast< ptrdiff_t >( block_size ), accessor_type::garbage_begin( heap )->size_ );
                        ASSERT_FALSE( accessor_type::garbage_begin( heap )->next_ );
                    }

                    // remember pool state
                    auto pool_head = accessor_type::pool_begin( heap );
                    auto pool_unallocated = pool_head->unallocated_;


                    auto garbage_head = accessor_type::garbage_begin( heap );
                    auto garbage_block_head = static_cast< intptr_t >( garbage_head );
                    auto garbage_block_tile = garbage_block_head + garbage_head->size_;

                    // allocate region again
                    auto p = heap.allocate( size, alignment );
                    
                    if ( allocation_on_garbage_expected )
                    {
                        // check that pool stays untouched
                        EXPECT_EQ( pool_head, accessor_type::pool_begin( heap ) );
                        EXPECT_EQ( pool_unallocated, pool_head->unallocated_ );

                        // check garbage is empty
                        EXPECT_EQ( accessor_type::garbage_begin( heap ), accessor_type::garbage_end( heap ) );

                        // check pointer to allocated region is not NULL
                        EXPECT_TRUE( p );

                        // check alignment
                        EXPECT_EQ( 0, reinterpret_cast< intptr_t >( p ) % alignment );

                        // check that requested region entirely lays in pool block
                        EXPECT_GE( garbage_block_head + static_cast< ptrdiff_t >( sizeof( intptr_t ) + sizeof( ptrdiff_t ) ), reinterpret_cast< intptr_t >( p ) );
                        EXPECT_LE( reinterpret_cast< intptr_t >( p ) + static_cast< ptrdiff_t >( size ), garbage_block_tile );

                        // check block head field points to beginning of the block
                        auto head_ptr_alignment = std::alignment_of_v< intptr_t >;
                        auto block_head_ptr = reinterpret_cast< intptr_t* >( ( ( reinterpret_cast< intptr_t > ( p ) - sizeof( intptr_t ) ) / head_ptr_alignment ) * head_ptr_alignment );
                        EXPECT_EQ( garbage_block_head, *block_head_ptr );

                        // check block size field contaions valid value
                        EXPECT_EQ( garbage_block_tile - garbage_block_head, *reinterpret_cast< ptrdiff_t* >( garbage_block_head ) );
                    }
                    else
                    {
                        // check garbage is untouched
                        EXPECT_EQ( garbage_head, accessor_type::garbage_begin( heap ) );
                    }

                    // release the block
                    if ( p ) heap.deallocate( p, size, alignment );
                }
                catch ( ... )
                {
                    FAIL();
                }
                //} );
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

