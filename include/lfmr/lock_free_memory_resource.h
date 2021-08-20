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


#ifndef __LOCK_FREE_MEMORY_RESOURCE__H__
#define __LOCK_FREE_MEMORY_RESOURCE__H__


#include <memory_resource>
#include <exception>
#include <new>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <atomic>
#include <thread>
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


namespace thinks
{
    //
    // forwarding declaration of UT's accessor
    //
    namespace ut { template < typename lock_free_memory_resource > struct accessor; }


    /** Default lock_free_memory_resource policy
    */
    struct default_policy
    {
        static constexpr std::size_t block_size = 1 << 16;                                      //< desired pool block size in bytes
        static constexpr std::size_t granularity = std::hardware_destructive_interference_size; //< desired lock_free_memory_resource granularity
        static constexpr std::size_t garbage_search_depth = 64;                                 //< desired depth of garbage search
        static constexpr std::size_t spin_limit = 1024;                                         //< desired number of spins before thread goes asleep
    };


    /** Implements lock free monotonic memory resource
    */
    template < typename Policy = default_policy >
    class lock_free_memory_resource : public std::pmr::memory_resource
    {
        // allows access to the private part
        template < typename HeapType > friend struct ut::accessor;


        /** Representation of pointer */
        using pointer_type = intptr_t;


        /** Representation of a size */
        using size_type = ptrdiff_t;


        /** Rounds given number upward with given modulo

        @param [in] value - value to be rounded
        @param [in] mod - modulo (MUST be a power of 2 )
        @retval a multiple of mod equal or greater than given value
        @throw never
        */
        static constexpr pointer_type ceil( pointer_type value, size_type mod ) noexcept
        {
            auto mask = mod - 1;
            return ( ( value & mask ) != 0 ) ? ( value | mask ) + 1 : value;
        }


        /** Rounds given number downward with given modulo

        @param [in] value - value to be rounded
        @param [in] mod - modulo
        @retval a multiple of mod equal or lesser than given value
        @throw never
        */
        static constexpr pointer_type floor( pointer_type value, size_type mod ) noexcept
        {
            auto mask = mod - 1;
            return value & ~mask;
        }


        /** Holds pool block internal fields */
        struct pool_block_header
        {
            std::atomic< pointer_type > unallocated_;   //< pointer to unallocated area inside a pool block
            pointer_type next_;                         //< poniter to the next pool block
        };


        /** Holds internal data of a deallocated memory block*/
        struct garbage_block_header
        {
            size_type size_;
            std::atomic< pointer_type > next_;
        };


        /** lock_free_memory_resource granularity, i.e. lock_free_memory_resource allocation quantum
        */
        static_assert( Policy::granularity, "Policy::granularity supposed to be positive integer" );
        static constexpr size_type granularity_ = ceil( Policy::granularity, std::hardware_destructive_interference_size );

        static constexpr size_type piece_internal_fields_size_ = sizeof( size_type ) + sizeof( pointer_type );

        static constexpr size_type pool_block_header_size_ = ceil( sizeof( pool_block_header ), granularity_ );

        static constexpr size_type garbage_block_header_size = sizeof( garbage_block_header );

        static constexpr pointer_type hazard_ = 1;

        std::atomic< pointer_type > pool_ = 0;
        std::atomic< pointer_type > garbage_ = 0;
        std::condition_variable grow_cv_;


        /** Cycles given action till returned value statys hazarded (the lowest bit is signalled)

        If number of cycles exceeds the limit force current thread to yeild

        @param [in] action - action to be cycled
        @retval first non-hazarded result of the action
        @throw whatever action throws
        */
        template < typename ActionType >
        auto wait_till_hazarded( ActionType&& action )
        {
            while ( true )
            {
                for ( std::size_t spin = 0; spin < Policy::spin_limit; ++spin )
                {
                    if ( auto value = action(); ( value & hazard_ ) == 0 ) return value;
                }
                std::this_thread::yield();
            }
        }


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


        /** Provides maximum size of memory block to be allocated on the lock_free_memory_resource
        
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
#ifdef _WIN32
            auto block = ::VirtualAlloc( desire, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            if ( !block ) throw std::bad_alloc();
#else
            auto block = ::mmap( desire, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0 );
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


        /** Provides reference to block head pointer for given allocated piece

        @param [in] piece - allocate memory piece
        @retval reference to block head pointer
        @throw nothing
        */
        static pointer_type& get_block_header_ptr_ref( pointer_type piece ) noexcept
        {
            auto block_head_ptr = floor( piece - sizeof( pointer_type ), std::alignment_of_v< pointer_type > );
            return *reinterpret_cast< pointer_type* >( block_head_ptr );
        }


        /** Allocates large memory block directly in process's virtual space not in the lock_free_memory_resource

        @param [in] bytes - requested block size
        @param [in] alignment - requested block alignment
        @retval pointer to aligned memory region
        @throw std::bad_alloc on failture
        */
        void* allocate_large_block( std::size_t bytes, std::size_t alignment )
        {
            // calculate required size
            size_type sz = ceil( ceil( piece_internal_fields_size_, alignment ) + bytes, system_page_size() );

            // allocate memory
            auto block = reinterpret_cast< pointer_type >( virtual_alloc( sz ) );

            // fill out block size
            *reinterpret_cast< size_type* >( block ) = sz;

            // find aligned region 
            auto aligned_area = ceil( block + piece_internal_fields_size_, alignment );

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
            // try lock pool for growing
            if ( auto pool = pool_.fetch_or( hazard_, std::memory_order_acquire ); ( pool & hazard_ ) == 0 )
            {
                // allocate new pool block
                auto new_pool_block = reinterpret_cast< intptr_t >( virtual_alloc( pool_block_size() ) );

                // fill next field
                reinterpret_cast< pool_block_header* >( new_pool_block )->next_ = pool & ~hazard_;

                // initialize pointer to unallocated space
                auto& unallocated_ref = reinterpret_cast< pool_block_header* >( new_pool_block )->unallocated_;
                unallocated_ref.store( ceil( new_pool_block + sizeof( pool_block_header ), granularity_ ), std::memory_order_relaxed );

                // put new block on top of the pool
                pool_.store( new_pool_block, std::memory_order_release );

                // notify waiting threads that pool growing completed
                grow_cv_.notify_all();
            }
            else
            {
                // allocation of new pool block is expansive operation, so just put current thread asleep until growing thread will have completed
                std::mutex fake_mutex;
                std::unique_lock< std::mutex > fake_lock( fake_mutex );
                grow_cv_.wait( fake_lock );
            }
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
            // get current pool pointer
            auto current_pool = pool_.load( std::memory_order_acquire ) & ~hazard_;

            while ( true )
            {
                // search through pool blocks
                auto current_pool_block = current_pool;
                while ( current_pool_block )
                {
                    // get reference to unallocated field
                    auto& unallocated_ref = reinterpret_cast< pool_block_header* >( current_pool_block )->unallocated_;

                    while ( true )
                    {
                        // get current pointer to unallocated area inside current block pool
                        auto unallocated = unallocated_ref.load( std::memory_order_acquire );

                        // get aligned pointer with respect to block's fields
                        auto aligned_area = ceil( unallocated + sizeof( size_type ) + sizeof( pointer_type ), alignment );

                        // calculate end of the block
                        auto tile = ceil( aligned_area + bytes, granularity_ );

                        // if pool block has NOT enough unallocated space
                        if ( tile > current_pool_block + pool_block_size() ) break;

                        // try allocate required memory block from current pool block
                        if ( unallocated_ref.compare_exchange_weak( unallocated, tile, std::memory_order_acq_rel, std::memory_order_relaxed ) )
                        {
                            // gotcha! -> fill block size field
                            *reinterpret_cast< size_type* >( unallocated ) = static_cast< size_type >( tile - unallocated );

                            // fill block head pointer
                            get_block_header_ptr_ref( aligned_area ) = unallocated;

                            // return pointer to aligned region as the result
                            return reinterpret_cast< void* >( aligned_area );
                        }

                        // another thread outrun this one -> try again on current pool block
                    }

                    // current block does not have enough space -> proceed to the next one
                    current_pool_block = reinterpret_cast< pool_block_header* >( current_pool_block )->next_;
                }

                // get pool pointer
                auto new_pool = pool_.load( std::memory_order_acquire ) & ~hazard_;

                // if just received value is not equal to current pool pointer -> somebody else has already grown the pool
                if ( new_pool != current_pool )
                {
                    // repeat with new pool pointer
                    current_pool = new_pool;
                    continue;
                }

                // if there is not pool block capable to fit requested block - grow the pool
                grow_pool();
            }
        }


        /** Tries to allocate a region of requested size and alignment from garbage
        */
        void* allocate_on_garbage( std::size_t bytes, std::size_t alignment ) noexcept
        {
            static_assert( Policy::garbage_search_depth, "Policy::garbage_search_depth supposed to be positive integer" );

            std::size_t garbage_search_depth = 0;

            // use this->garbage_ as current garbage block an lock it
            auto current_garbage_block_ref = std::ref( garbage_ );
            auto current_garbage_block = wait_till_hazarded( [&]() {
                return current_garbage_block_ref.get().fetch_or( hazard_, std::memory_order_acq_rel ); }
            );

            while ( true )
            {
                // nothing left to search through
                if ( !current_garbage_block )
                {
                    // unlock current garbage block and leave
                    current_garbage_block_ref.get().store( current_garbage_block, std::memory_order_release );
                    return nullptr;
                }

                // get reference to the next garbage block pointer
                auto next_garbage_block_ref = std::ref( reinterpret_cast< garbage_block_header* >( current_garbage_block )->next_ );

                // get current garbage block tile
                auto current_garbage_block_tile = current_garbage_block + reinterpret_cast< garbage_block_header* >( current_garbage_block )->size_;

                // calculate aligned region placement and tile of requested block
                auto aligned_area = ceil( current_garbage_block + sizeof( size_type ) + sizeof( pointer_type ), alignment );
                auto tile = ceil( aligned_area + bytes, granularity_ );

                // if current garbage block cannot fit requested region
                if ( auto remainder = current_garbage_block_tile - tile; remainder < 0 )
                {
                    // if maximum search depth reached
                    if ( garbage_search_depth++ >= Policy::garbage_search_depth )
                    {
                        // unlock current garbage block and admit failture
                        current_garbage_block_ref.get().store( current_garbage_block, std::memory_order_release );
                        return nullptr;
                    }

                    // wait till the next garbage block gets unlocked
                    auto next_garbage_block = wait_till_hazarded( [&]() noexcept {
                        return next_garbage_block_ref.get().load( std::memory_order_acquire ); }
                    );

                    // get lock over the next garbage block (no reason to sync the value, we'll do it immediately after)
                    next_garbage_block_ref.get().store( next_garbage_block | hazard_, std::memory_order_relaxed );

                    // unlock current garbage block 
                    current_garbage_block_ref.get().store( current_garbage_block, std::memory_order_release );

                    // proceed to the next block
                    current_garbage_block_ref = next_garbage_block_ref;
                    current_garbage_block = next_garbage_block;
                    continue;
                }
                else
                {
                    // there is a reminder
                    if ( remainder > 0 )
                    {
                        // update size field of current garbage block
                        reinterpret_cast< garbage_block_header* >( current_garbage_block )->size_ = tile - current_garbage_block;

                        // wait till the next garbage block gets unlocked
                        auto next_garbage_block = wait_till_hazarded( [ & ]() noexcept {
                            return next_garbage_block_ref.get().load( std::memory_order_acquire ); }
                        );

                        // mark up new garbage block header at tile
                        reinterpret_cast< garbage_block_header* >( tile )->size_ = remainder;
                        reinterpret_cast< garbage_block_header* >( tile )->next_.store( next_garbage_block, std::memory_order_relaxed );

                        // replace allocated block with the reminder in the garbage list
                        current_garbage_block_ref.get().store( tile, std::memory_order_release );
                    }
                    else
                    {
                        // wait till pointer to next block gets unlocked and assign it to garbage_block_ref cutting current garbage block from the sequence
                        auto next = wait_till_hazarded( [&]() noexcept {
                            return next_garbage_block_ref.get().load( std::memory_order_acquire ); }
                        );
                        current_garbage_block_ref.get().store( next );
                    }
                }

                // fill <block head ptr> field
                get_block_header_ptr_ref( aligned_area ) = current_garbage_block;

                // return aligned region as the result
                return reinterpret_cast< void* >( aligned_area );
            }
        }


    protected:

        /** Implements virtual std::prm::memory_resource::do_allocate()
        */
        void* do_allocate( std::size_t bytes, std::size_t alignment ) override
        {
            if ( !bytes )
            {
                throw std::invalid_argument( "azul::lock_free_memory_resource::do_allocate(): invalid requested size" );
            }

            // check alignment
            if ( !alignment ||
                ( alignment & ( alignment - 1 ) ) != 0 ||
                static_cast< size_type >( alignment ) > system_page_size() )
            {
                throw std::invalid_argument( "azul::lock_free_memory_resource::do_allocate(): invalid requested alignment" );
            }

            // calculate size of pool block that could fit requested region
            auto required_pool_block_size = ceil( ceil( ceil( sizeof( pool_block_header ), granularity_ ) + piece_internal_fields_size_, alignment ) + bytes, granularity_ );
            if ( required_pool_block_size < 0 ) throw std::bad_alloc();

            // if block too large to be allocated on pool
            if ( required_pool_block_size > pool_block_size() )
            {
                // allocate block directly in the process's virtual space
                return allocate_large_block( bytes, alignment );
            }

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
                    while ( true )
                    {
                        // prepend block to garbage ( no reason to touch <block size> field )
                        auto garbage = garbage_.load( std::memory_order_acquire );
                        reinterpret_cast< garbage_block_header* >( block_head_ptr )->next_.store( garbage, std::memory_order_relaxed );
                        if ( garbage_.compare_exchange_weak( garbage, block_head_ptr, std::memory_order_acq_rel, std::memory_order_relaxed ) ) break;
                    }
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
        lock_free_memory_resource() { grow_pool(); }


        /** Destructor

        Releases allocated birtual memory

        @throw never
        */
        ~lock_free_memory_resource()
        {
            auto pool = pool_.load( std::memory_order_acquire );
            while ( pool )
            {
                auto next = reinterpret_cast< pool_block_header* >( pool )->next_;
                virtual_free( reinterpret_cast< void* >( pool ), pool_block_size() );
                pool = next;
            }
        }
    };
}

#endif
