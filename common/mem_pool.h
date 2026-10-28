#pragma once

#include <cstddef>  // std::size_t, std::byte
#include <limits>   // std::numeric_limits
#include <memory>   // std::construct_at, std::destroy_at
#include <utility>  // std::forward
#include <vector>

namespace Common {
/// Fixed-capacity pool of T objects.
///
/// All memory is allocated once, in the constructor. allocate() and deallocate()
/// never call new/delete: an object is constructed in place in a free slot and
/// destroyed in place when it is given back. Objects live in one contiguous
/// block, so hot objects stay cache-local.
///
/// Layout ("structure of arrays"): the free/in-use flags live in their own
/// bit-packed vector, separate from the object storage, because the scan for a
/// free slot only needs the flags.
template <typename T>
class MemPool final {
public:
    /// Reserve room for num_elems objects. No T is constructed yet.
    explicit MemPool(std::size_t num_elems) : slots_(num_elems), is_free_(num_elems, true), num_free_(num_elems) {}

    /// Destroy every object that is still alive; the vectors then release the raw memory.
    ~MemPool() {
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            if (!is_free_[i]) std::destroy_at(ptr(i));
        }
    }

    /// Construct a T in a free slot, forwarding args to T's constructor.
    /// Returns nullptr if the pool is exhausted.
    template <typename... Args>
    T* allocate(Args&&... args) noexcept {
        const std::size_t idx = findFreeSlot();
        if (idx == kNoSlot) return nullptr;
        T* obj = std::construct_at(ptr(idx), std::forward<Args>(args)...);
        is_free_[idx] = false;
        --num_free_;
        next_free_index_ = (idx + 1 == slots_.size()) ? 0 : idx + 1;
        return obj;
    }

    /// Destroy *elem and mark its slot free. elem must have come from this pool.
    void deallocate(const T* elem) noexcept {
        const std::size_t idx = indexOf(elem);
        std::destroy_at(elem);
        is_free_[idx] = true;
        ++num_free_;
    }

    std::size_t capacity() const noexcept { return slots_.size(); }
    std::size_t numFree() const noexcept { return num_free_; }

    // Deleted special members: the pool owns its backing storage and the free
    // list; copying would alias both.
    MemPool() = delete("a MemPool owns its backing storage and free list");
    MemPool(const MemPool&) = delete("a MemPool owns its backing storage and free list");
    MemPool(MemPool&&) = delete("a MemPool owns its backing storage and free list");
    MemPool& operator=(const MemPool&) = delete("a MemPool owns its backing storage and free list");
    MemPool& operator=(MemPool&&) = delete("a MemPool owns its backing storage and free list");


private:
    /// Raw, correctly aligned bytes for exactly one T. Nothing is constructed
    /// here until allocate() places an object.
    struct alignas(T) Slot {
        std::byte bytes[sizeof(T)];
    };

    static constexpr std::size_t kNoSlot = std::numeric_limits<std::size_t>::max();

    T* ptr(std::size_t idx) noexcept { return reinterpret_cast<T*>(slots_[idx].bytes); }

    std::size_t indexOf(const T* elem) const noexcept {
        return static_cast<std::size_t>(reinterpret_cast<const Slot*>(elem) - slots_.data());
    }

    /// Find a free slot, scanning from next_free_index_ and wrapping around.
    std::size_t findFreeSlot() const noexcept {
        if (num_free_ == 0) return kNoSlot;
        std::size_t idx = next_free_index_;
        while (!is_free_[idx]) {
            ++idx;
            if (idx == slots_.size()) idx = 0;
        }
        return idx;
    }

    std::vector<Slot> slots_;          // raw object storage, contiguous
    std::vector<bool> is_free_;        // one bit per slot: true = free
    std::size_t num_free_;             // how many slots are free right now
    std::size_t next_free_index_ = 0;  // where the next scan starts
};

}   // namespace Common