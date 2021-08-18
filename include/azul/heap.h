#ifndef __AZUL__HEAP__H__
#define __AZUL__HEAP__H__


#include <memory_resource>
#include <exception>
#include <new>
#include <mutex>
#include <type_traits>
#include <assert.h>
#ifdef _WIN32
#   include <windows.h>
#   ifdef max
#       undef max
#   endif
#   ifdef min
#       undef min
#   endif
#else
#   include <unistd.h>
#   include <sys/mman.h>
#endif


//
// Few STD implementations (e.g. GCC) do not support std::hardware_destructive_interference_size
//
#if ( __cpp_lib_hardware_interference_size < 201603 )

#   ifdef __powerpc64__
#       define CACHELINE_SIZE 128
#   elif defined( __arm__ ) && ( defined __ARM_ARCH_5T__ )
#       define CACHELINE_SIZE 32
#   endif

#   ifndef CACHELINE_SIZE
#       define CACHELINE_SIZE 64
#   endif

namespace std
{
    static constexpr size_t hardware_destructive_interference_size = CACHELINE_SIZE;
}

#   undef CACHELINE_SIZE
#endif


namespace azul
{
    //
    // forwarding declaration of UT's accessor
    //
    namespace ut { template < typename Heap > struct accessor; }


    /** Default heap policy
    */
    struct default_policy
    {
        static constexpr std::size_t block_size = 1 << 16;                                      //< desired pool block size in bytes
        static constexpr std::size_t granularity = std::hardware_destructive_interference_size; //< desired heap granularity
        static constexpr std::size_t garbage_search_depth = 64;                                 //< desired depth of garbage search
        static constexpr std::size_t spin_limit = 1024;                                         //< desired number of spins before thread goes asleep
    };


    template < typename Policy = default_policy >
    class heap : public std::pmr::memory_resource
    {
        // allows access to the private part
        template < typename HeapType > friend struct ut::accessor;

        using pointer_type = intptr_t;
        using size_type = ptrdiff_t;
        using mutex_type = std::mutex;
        using lock_type = std::unique_lock< mutex_type >;

        struct pool_block_header
        {
            pointer_type unallocated_;
            pointer_type next_;
        };

        struct garbage_block_header
        {
            size_type size_;
            pointer_type next_;
        };

        static constexpr size_type piece_internal_fields_size = sizeof( size_type ) + sizeof( pointer_type );
        static constexpr pointer_type hazard_ = 1;
        mutex_type   guard_;
        pointer_type pool_ = 0;
        pointer_type garbage_ = 0;


        /** Rounds given number upward with given modulo

        @param [in] value - value to be rounded
        @param [in] mod - modulo
        @retval a multiple of mod equal or greater than given value
        @throw never
        */
        static constexpr pointer_type ceil( pointer_type value, size_type mod ) noexcept
        {
            assert( mod );
            auto rem = value % mod;
            return value + ( rem ? mod - rem : 0 );
        }


        /** Rounds given number downward with given modulo

        @param [in] value - value to be rounded
        @param [in] mod - modulo
        @retval a multiple of mod equal or lesser than given value
        @throw never
        */
        static constexpr pointer_type floor( pointer_type value, size_type mod ) noexcept
        {
            assert( mod );
            return value - value % mod;
        }


        /** Heap granularity, i.e. heap allocation quantum
        */
        static_assert( Policy::granularity, "Policy::granularity supposed to be positive integer" );
        static constexpr size_type granularity_ = ceil( Policy::granularity, std::hardware_destructive_interference_size );


        /** Provides virtual memory allocation granularity supported by target OS

        @retval virtual memory allocation quantum size
        @thrown nothing
        */
        static size_type system_page_size() noexcept
        {
#ifdef _WIN32
            static const size_type sz = []() {
                SYSTEM_INFO si;
                ::GetSystemInfo( &si );
                return static_cast< size_type >( si.dwAllocationGranularity );
            }( );
#else
            static const size_type sz = static_cast< size_type >( ::sysconf( _SC_PAGE_SIZE ) );
#endif
            return sz;
        }


        /** Provides actual pool block size with respect to desired value and target system capabilities
        
        @retval actual pool block size
        @throw never
        */
        static size_type pool_block_size() noexcept
        {
            static_assert( Policy::block_size, "Policy::block_size supposed to be positive integer" );
            static size_type value = ceil( Policy::block_size, system_page_size() );
            return value;
        };


        /** Provides maximum size of memory block to be allocated on the heap
        
        @retval size of unallocated memory in new pool block
        @throw nothing
        */
        static size_type pool_block_capacity() noexcept
        {
            static size_type value = pool_block_size() - ceil( sizeof( pool_block_header ), granularity_ );
            return value;
        };


        /** Allocates virtual memory block

        @param [in] size - size of requested memory block
        @throw std::bad_alloc on failture
        */
        static void* virtual_alloc( size_type size, void* desire = nullptr )
        {
            assert( size && size % system_page_size() == 0 );
#ifdef _WIN32
            auto block = ::VirtualAlloc( desire, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            if ( !block ) throw std::bad_alloc();
#else
            auto block = ::mmap( desire, size, PROT_READ + PROT_WRITE, MAP_ANONYMOUS + MAP_SHARED, -1, 0 );
            if ( MAP_FAILED == block ) throw std::bad_alloc();
#endif
            return block;
        }

        /** Releases allocated block of virtual memory

        @param
        @throw never
        */
        static void virtual_free( void* p, [[maybe_unused]] size_type size ) noexcept
        {
#ifdef _WIN32
            ::VirtualFree( p, 0, MEM_RELEASE );
#else
            ::munmap( p, size );
#endif
        }

        //template < typename ActionType >
        //static void wait_till_pointer_hazarded( const ActionType& action ) noexcept
        //{
        //    static_assert( Policy::spin_limit, "Policy::spin_limit supposed to be positive integer" );
        //    while ( true )
        //    {
        //        for ( std::size_t spin = 0; spin < Policy::spin_limit; ++spin )
        //        {
        //            if ( action() & hazard == 0 ) return;
        //        }
        //        std::this_thread::yield();
        //    }
        //}


        /** Provides reference to block head pointer for given allocated piece

        @param [in] piece - allocate memory piece
        @retval reference to block head pointer
        @throw nothing
        */
        static pointer_type& get_block_header_ptr_ref( pointer_type piece ) noexcept
        {
            assert( piece );
            auto block_head_ptr = floor( piece - sizeof( pointer_type ), std::alignment_of_v< pointer_type > );
            return *reinterpret_cast< pointer_type* >( block_head_ptr );
        }


        /** Allocates large memory block directly in process's virtual space not in the heap

        @param [in] bytes - requested block size
        @param [in] alignment - requested block alignment
        @retval pointer to aligned memory region
        @throw std::bad_alloc on failture
        */
        void* allocate_large_block( std::size_t bytes, std::size_t alignment )
        {
            // calculate required size
            size_type sz = ceil( ceil( piece_internal_fields_size, alignment ) + bytes, system_page_size() );

            // allocate memory
            auto block = reinterpret_cast< pointer_type >( virtual_alloc( sz ) );

            // fill out block size
            assert( block % std::alignment_of_v< size_type > == 0 );
            *reinterpret_cast< size_type* >( block ) = sz;

            // find aligned region 
            auto aligned_area = ceil( block + piece_internal_fields_size, alignment );
            assert( aligned_area % alignment == 0 );
            assert( aligned_area + static_cast< size_type >( bytes )<= block + sz );

            // fill out block pointer
            get_block_header_ptr_ref( aligned_area ) = block;

            // return pointer to aligned region as the result
            return reinterpret_cast< void* >( aligned_area );
        }


        /** Allocates and prepends another pool block

        @throw std::bad_alloc on failture
        */
        void grow_pool()
        {
            // allocate new pool block
            auto new_pool_block = reinterpret_cast< intptr_t >( virtual_alloc( pool_block_size() ) );
            assert( new_pool_block % granularity_ == 0 );

            // initialize pointer to unallocated space
            reinterpret_cast< pool_block_header* >( new_pool_block )->unallocated_ = ceil( new_pool_block + sizeof( pool_block_header ), granularity_ );

            // put new block on the top of the pool
            reinterpret_cast< pool_block_header* >( new_pool_block )->next_ = pool_;
            pool_ = new_pool_block;
        }


        /** Allocates a region of specified size and alignment on the pool
        
        If there is not a pool block grows with unallocated area large enough - grows the pool with new block

        @param [in] bytes - size of requested region in bytes
        @param [in] alignment - alignment of requested region
        @retval pointer to aligned region of specified size
        @throws std::bad_alloc on failture
        */
        void* allocate_on_pool( std::size_t bytes, std::size_t alignment )
        {
            while ( true )
            {
                // through the pool blocks
                auto current_pool_block = pool_;
                while ( current_pool_block )
                {
                    // get pointer to unallocated memory inside the block
                    auto& unallocated = reinterpret_cast< pool_block_header* >( current_pool_block )->unallocated_;
                    assert( unallocated % granularity_ == 0 );

                    // get aligned pointer with respect to block's fields
                    auto aligned_area = ceil( unallocated + sizeof( size_type ) + sizeof( pointer_type ), alignment );
                    assert( aligned_area % alignment == 0 );

                    // calculate end of the block
                    auto tile = ceil( aligned_area + bytes, granularity_ );
                    assert( tile % granularity_ == 0 );

                    // if pool block has enough unallocated space
                    if ( tile <= current_pool_block + pool_block_size() )
                    {
                        // fill <block size> field
                        *reinterpret_cast< size_type* >( unallocated ) = static_cast< size_type >( tile - unallocated );

                        // fill <block head pointer> field
                        get_block_header_ptr_ref( aligned_area ) = unallocated;

                        // allocate block
                        unallocated = tile;

                        // return pointer to aligned region as the result
                        return reinterpret_cast< void* >( aligned_area );
                    }
                    current_pool_block = reinterpret_cast< pool_block_header* >( current_pool_block )->next_;
                }

                // if there is not pool block capable to fit requested block - grow the pool
                grow_pool();
            }
        }


        /** Tries to allocate a region of requested size and alignment from garbage
        */
        void* allocate_on_garbage( std::size_t bytes, std::size_t alignment )
        {
            static_assert( Policy::garbage_search_depth, "Policy::garbage_search_depth supposed to be positive integer" );

            // look for available block through the garbage
            auto garbage_block_ref = std::ref( garbage_ );
            std::size_t garbage_search_depth = 0;
            while ( true )
            {
                if ( auto garbage_block = garbage_block_ref.get() )
                {
                    // get garbage block tile
                    auto garbage_block_tile = garbage_block + reinterpret_cast< garbage_block_header* >( garbage_block )->size_;
                    assert( garbage_block_tile % granularity_ == 0 );

                    // calculate aligned region pointer and tile of requested block
                    auto aligned_area = ceil( garbage_block + sizeof( size_type ) + sizeof( pointer_type ), alignment );
                    auto tile = ceil( aligned_area + bytes, granularity_ );

                    // if current garbage block cannot fit requested region
                    if ( auto remainder = tile - garbage_block_tile; remainder < 0 )
                    {
                        // break garbage search if maximum depth reached
                        if ( garbage_search_depth++ >= Policy::garbage_search_depth ) break;

                        // proceed to the next garbage block
                        garbage_block_ref = reinterpret_cast< garbage_block_header* >( garbage_block )->next_;
                        continue;
                    }
                    else
                    {
                        // there is a reminder
                        if ( remainder > 0 )
                        {
                            // reminder MUST be multiplication of granularity_
                            assert( remainder % granularity_ == 0 );

                            // update <block size> field 
                            reinterpret_cast< garbage_block_header* >( garbage_block )->size_ = remainder;

                            // mark up new garbage block header at tile
                            reinterpret_cast< garbage_block_header* >( tile )->next_ = reinterpret_cast< garbage_block_header* >( garbage_block )->next_;
                            reinterpret_cast< garbage_block_header* >( tile )->size_ = remainder;

                            // replace allocated block with the reminder in the garbage list
                            garbage_block_ref.get() = tile;
                        }
                        else
                        {
                            // entirely remove current block from the list
                            garbage_block_ref.get() = reinterpret_cast< garbage_block_header* >( garbage_block )->next_;
                        }
                    }

                    // fill <block head ptr> field
                    get_block_header_ptr_ref( aligned_area ) = garbage_block;

                    // return aligned region as the result
                    return reinterpret_cast< void* >( aligned_area );
                }
                else
                {
                    break;
                }
            }

            return nullptr;
        }

    protected:

        /** Implements virtual std::prm::memory_resource::do_allocate()
        */
        void* do_allocate( std::size_t bytes, std::size_t alignment ) override
        {
            if ( !bytes ) throw std::invalid_argument( "azul::heap::do_allocate(): invalid requested size" );
            if ( !alignment || static_cast< size_type >( alignment ) > system_page_size() ) throw std::invalid_argument( "azul::heap::do_allocate(): invalid requested alignment" );

            // calculate size of pool block that could fit requested region
            auto required_pool_block_size = ceil( ceil( ceil( sizeof( size_type ) + sizeof( pointer_type ), granularity_ ) + sizeof( size_type ) + sizeof( pointer_type ), alignment ) + bytes, granularity_ );
            if ( required_pool_block_size < 0 ) throw std::bad_alloc();

            // if block too large to be allocated on pool
            if ( required_pool_block_size > pool_block_size() )
            {
                // allocate block directly in the process's virtual space
                return allocate_large_block( bytes, alignment );
            }

            lock_type lock( guard_ );

            // try allocate block on garbage
            if ( auto block = allocate_on_garbage( bytes, alignment ) )
            {
                return block;
            }
            else
            {
                // allocate block on pool
                return allocate_on_pool( bytes, alignment );
            }
        }


        /** Implements virtual std::prm::memory_resource::do_deallocate()

        @param [in] p - pointer to region to be deallocated
        @throw never
        */
        void do_deallocate( void* p, std::size_t, std::size_t ) override
        {
            if ( p )
            {
                auto block_head_ptr = get_block_header_ptr_ref( reinterpret_cast< pointer_type >( p ) );
                auto block_size = *reinterpret_cast< size_type* >( block_head_ptr );
                if ( block_size > pool_block_capacity() )
                {
                    virtual_free( reinterpret_cast< void* >( block_head_ptr ), block_size );
                }
                else
                {
                    lock_type lock( guard_ );

                    // prepend block to garbage ( no reason to touch <block size> field )
                    reinterpret_cast< garbage_block_header* >( block_head_ptr )->next_ = garbage_;
                    garbage_ = block_head_ptr;
                }
            }
        }


        /** Implements virtual std::prm::memory_resource::do_is_equal()

        @param [in] other - memory resource to be evaluated
        @retval true if the same instance
        */
        bool do_is_equal( const std::pmr::memory_resource& other ) const noexcept override
        {
            return ( this == &other );
        }

    public:

        /** Default constructor

        Allocates first pool block
        
        @throw std::bad_alloc if memory is low
        */
        heap() { grow_pool(); }


        /** Destructor

        Releases allocated birtual memory

        @throw never
        */
        ~heap()
        {
            while ( pool_ )
            {
                auto next = reinterpret_cast< pool_block_header* >( pool_ )->next_;
                virtual_free( reinterpret_cast< void* >( pool_ ), pool_block_size() );
                pool_ = next;
            }
        }
    };
}

#endif
