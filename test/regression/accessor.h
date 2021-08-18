#ifndef __AZUL__UT__ACCESSOR__H__
#define __AZUL__UT__ACCESSOR__H__

#include <iterator>

namespace azul
{
    namespace ut
    {
        template < typename HeapType >
        struct accessor
        {
            using pointer_type = typename HeapType::pointer_type;
            using size_type = typename HeapType::size_type;
            using pool_block_header_type = typename HeapType::pool_block_header;
            using garbage_block_header_type = typename HeapType::garbage_block_header;

            static constexpr auto granularity = HeapType::granularity_;
            inline static const auto pool_block_size = HeapType::pool_block_size();
            inline static const auto pool_block_capacity = HeapType::pool_block_capacity();
            static constexpr auto pool_block_header_size = HeapType::ceil( sizeof( pool_block_header_type ), granularity );
            static constexpr auto piece_internal_fields_size = HeapType::piece_internal_fields_size;

            static pointer_type ceil( pointer_type value, size_type mod ) noexcept { return HeapType::ceil( value, mod ); }
            static pointer_type floor( pointer_type value, size_type mod ) noexcept { return HeapType::floor( value, mod ); }
            static pointer_type& get_block_header_ptr_ref( pointer_type piece ) noexcept { return HeapType::get_block_header_ptr_ref( piece ); }
            static void* virtual_alloc( size_type size, void* desire = nullptr ) { return HeapType::virtual_alloc( size, desire ); }
            static void virtual_free( void* p, size_type size ) noexcept { return HeapType::virtual_free( p, size ); }

        private:

            template < typename ValueType >
            struct iterator
            {
                using iterator_category = std::forward_iterator_tag;
                using value_type = ValueType;
                using difference_type = ptrdiff_t;
                using pointer = const value_type*;
                using reference = const value_type&;
                using self_type = iterator;

                iterator() noexcept = default;
                explicit iterator( pointer_type it ) noexcept : it_( it ) {}
                reference operator*() const noexcept { return *reinterpret_cast< value_type* >( it_ ); }
                pointer operator->() const noexcept { return reinterpret_cast< value_type* >( it_ ); }
                self_type& operator++() noexcept { it_ = reinterpret_cast< value_type* >( it_ )->next_; return *this; }
                self_type operator++( int ) noexcept { self_type tmp = *this; ++( *this ); return tmp; }
                bool friend operator==( const self_type& lhs, const self_type& rhs ) noexcept { return lhs.it_ == rhs.it_; }
                bool friend operator!=( const self_type& lhs, const self_type& rhs ) noexcept { return !( lhs == rhs ); }
                explicit operator pointer_type() const noexcept { return it_; }

            private:
                pointer_type it_ = 0;
            };

        public:

            using pool_iterator = iterator< pool_block_header_type >;
            using garbage_iterator = iterator< garbage_block_header_type >;

            static pool_iterator pool_begin( const HeapType& heap ) noexcept { return pool_iterator( heap.pool_ ); }
            static pool_iterator pool_end( const HeapType& ) noexcept { return pool_iterator(); }
            static garbage_iterator garbage_begin( const HeapType& heap ) noexcept { return garbage_iterator( heap.garbage_ ); }
            static garbage_iterator garbage_end( const HeapType& ) noexcept { return garbage_iterator(); }

            static std::size_t pool_size( const HeapType& heap ) noexcept
            {
                std::size_t sz = 0;
                for ( auto it = pool_begin( heap ), end = pool_end( heap ); it != end; ++it, ++sz );
                return sz;
            }

            static std::size_t garbage_size( const HeapType& heap ) noexcept
            {
                std::size_t sz = 0;
                for ( auto it = garbage_begin( heap ), end = garbage_end( heap ); it != end; ++it, ++sz );
                return sz;
            }
        };
    }
}

#endif
