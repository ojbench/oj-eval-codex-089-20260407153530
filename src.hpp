// Buddy Allocator implementation for ACMOJ Problem 2199
// Implements initialization and three operations without using STL containers.
// Interfaces exposed:
//   void init(int ram_size, int min_block_size);
//   int malloc(int size);
//   int malloc_at(int addr, int size);
//   void free_at(int addr, int size);
// Also provides alias initializers: allocator_init, initialize for compatibility.

#ifndef SRC_HPP_BUDDY_ALLOCATOR_2199
#define SRC_HPP_BUDDY_ALLOCATOR_2199

#include <cstddef>
#include <cstdint>

namespace buddy_internal {

// Node state: FREE (unallocated, not split), SPLIT (has children states), FULL (allocated or children fully occupied)
enum NodeState : uint8_t { FULL = 0, SPLIT = 1, FREE = 2 };

struct BuddyAllocator {
    int ram_size;         // total memory size in bytes
    int min_block;        // minimum block size in bytes
    int levels;           // L where ram_size = min_block * 2^L
    int tree_height;      // same as levels
    int total_nodes;      // 2^(L+1) - 1
    NodeState* state;     // 1-indexed complete binary tree array
    bool initialized;

    BuddyAllocator() : ram_size(0), min_block(0), levels(0), tree_height(0), total_nodes(0), state(nullptr), initialized(false) {}
    ~BuddyAllocator() { if (state) { delete[] state; state = nullptr; } }

    static bool is_power_of_two(int x) {
        return x > 0 && ( (x & (x - 1)) == 0 );
    }

    static int ilog2_floor(unsigned v) {
        // integer log2 for power-of-two inputs (0 undefined). For non-powers, returns floor.
        int r = -1;
        while (v) { v >>= 1; ++r; }
        return r;
    }

    void reset_states() {
        for (int i = 1; i <= total_nodes; ++i) state[i] = FULL; // set all to FULL first
        // Root represents entire memory; start fully free
        if (total_nodes >= 1) state[1] = FREE;
    }

    void init(int ram_size_, int min_block_) {
        // Validate and initialize tree
        ram_size = ram_size_;
        min_block = min_block_;
        if (ram_size <= 0) ram_size = 0;
        if (min_block <= 0) min_block = 1;
        int ratio = (min_block > 0) ? (ram_size / min_block) : 0;
        // Compute levels L so that ram_size == min_block * 2^L; if not exact, clamp by floor.
        if (min_block == 0 || ram_size % min_block != 0 || !is_power_of_two(ratio)) {
            // Fallback: find largest L with min_block * 2^L <= ram_size
            int t = ratio;
            // ensure t>0
            if (t <= 0) t = 1;
            int L = ilog2_floor((unsigned)t);
            levels = L;
        } else {
            levels = ilog2_floor((unsigned)ratio);
        }
        if (levels < 0) levels = 0;
        tree_height = levels; // leaves at layer 0 (min block), root at layer L
        // total nodes = 2^(L+1) - 1
        int nodes = 1;
        for (int i = 0; i < tree_height + 1; ++i) {
            // nodes at depth i from top doubles each level; keep cumulative at end
            if (i < tree_height) {
                // prevent overflow; but sizes are small
                nodes = (nodes << 1) | 1; // nodes = 2^(i+2)-1 at next iteration; final becomes 2^(L+1)-1
            }
        }
        // The above loop ends with nodes = 2^(L+1) - 1 if L>=0
        total_nodes = (tree_height >= 0) ? ((1 << (tree_height + 1)) - 1) : 0;
        // Allocate state array (1-indexed)
        if (state) { delete[] state; state = nullptr; }
        if (total_nodes < 1) total_nodes = 1;
        state = new NodeState[total_nodes + 1];
        reset_states();
        initialized = true;
    }

    // Map size (bytes) to target layer (0..L) where layer 0 is min_block, layer L is ram_size
    // Returns -1 if invalid (not an exact power-of-two multiple)
    int size_to_layer(int size) const {
        if (size <= 0 || min_block <= 0) return -1;
        if (size % min_block != 0) return -1;
        int factor = size / min_block;
        if (!is_power_of_two(factor)) return -1;
        int d = ilog2_floor((unsigned)factor);
        if (d > levels) return -1;
        return d;
    }

    // Leftmost allocation of a block at target layer D (0..L).
    // Node at index idx represents layer dNode with block starting at base_addr and size node_size.
    bool alloc_leftmost(int idx, int dNode, int D, int base_addr, int node_size, int &out_addr) {
        if (idx <= 0 || idx > total_nodes) return false;
        if (dNode < D) return false;
        NodeState st = state[idx];
        if (st == FULL) return false;
        if (dNode == D) {
            // Can only allocate if this node is FREE
            if (st == FREE) {
                state[idx] = FULL;
                out_addr = base_addr;
                return true;
            }
            return false; // SPLIT cannot satisfy this size
        }
        // Need to go deeper
        if (st == FREE) {
            // Split this free block into two free children
            state[idx] = SPLIT;
            int left = idx << 1, right = left | 1;
            if (left <= total_nodes) state[left] = FREE;
            if (right <= total_nodes) state[right] = FREE;
        }
        // Now st is SPLIT
        int half = node_size >> 1;
        int left = idx << 1, right = left | 1;
        int res_addr = -1;
        bool ok = false;
        if (left <= total_nodes) {
            ok = alloc_leftmost(left, dNode - 1, D, base_addr, half, out_addr);
        }
        if (!ok && right <= total_nodes) {
            ok = alloc_leftmost(right, dNode - 1, D, base_addr + half, half, out_addr);
        }
        // Update parent state: if both children FULL, set FULL; else keep SPLIT
        if (left <= total_nodes && right <= total_nodes) {
            if (state[left] == FULL && state[right] == FULL) {
                state[idx] = FULL;
            } else {
                state[idx] = SPLIT;
            }
        }
        return ok;
    }

    bool alloc_at_exact(int idx, int dNode, int D, int base_addr, int node_size, int target_addr) {
        if (idx <= 0 || idx > total_nodes) return false;
        if (dNode < D) return false;
        NodeState st = state[idx];
        if (st == FULL) return false;
        if (dNode == D) {
            if (base_addr == target_addr && st == FREE) {
                state[idx] = FULL;
                return true;
            }
            return false;
        }
        // Need to descend to the child that contains target_addr
        if (st == FREE) {
            // Split into two free children
            state[idx] = SPLIT;
            int left = idx << 1, right = left | 1;
            if (left <= total_nodes) state[left] = FREE;
            if (right <= total_nodes) state[right] = FREE;
        }
        int half = node_size >> 1;
        int left = idx << 1, right = left | 1;
        bool ok = false;
        if (target_addr < base_addr + half) {
            if (left <= total_nodes) ok = alloc_at_exact(left, dNode - 1, D, base_addr, half, target_addr);
        } else {
            if (right <= total_nodes) ok = alloc_at_exact(right, dNode - 1, D, base_addr + half, half, target_addr);
        }
        // Update parent state if both children FULL
        if (left <= total_nodes && right <= total_nodes) {
            if (state[left] == FULL && state[right] == FULL) {
                state[idx] = FULL;
            } else {
                state[idx] = SPLIT;
            }
        }
        return ok;
    }

    bool free_exact(int idx, int dNode, int D, int base_addr, int node_size, int target_addr) {
        if (idx <= 0 || idx > total_nodes) return false;
        if (dNode < D) return false;
        if (dNode == D) {
            // Must be exactly at this node
            if (base_addr == target_addr && state[idx] == FULL) {
                state[idx] = FREE;
                return true;
            }
            return false;
        }
        // Must be split to have children allocated at deeper layers
        if (state[idx] != SPLIT) {
            // Inconsistent free request; ignore safely
            return false;
        }
        int half = node_size >> 1;
        int left = idx << 1, right = left | 1;
        bool ok = false;
        if (target_addr < base_addr + half) {
            if (left <= total_nodes) ok = free_exact(left, dNode - 1, D, base_addr, half, target_addr);
        } else {
            if (right <= total_nodes) ok = free_exact(right, dNode - 1, D, base_addr + half, half, target_addr);
        }
        if (!ok) return false;
        // After freeing child, if both children FREE, coalesce to FREE
        if (left <= total_nodes && right <= total_nodes) {
            if (state[left] == FREE && state[right] == FREE) {
                state[idx] = FREE;
            } else {
                state[idx] = SPLIT;
            }
        }
        return true;
    }

    int alloc_size(int size) {
        if (!initialized) return -1;
        int D = size_to_layer(size);
        if (D < 0) return -1;
        int out_addr = -1;
        bool ok = alloc_leftmost(1, tree_height, D, 0, ram_size, out_addr);
        return ok ? out_addr : -1;
    }

    int alloc_at(int addr, int size) {
        if (!initialized) return -1;
        // address must be aligned to size and within bounds
        if (addr < 0 || addr >= ram_size) return -1;
        if (addr % size != 0) return -1;
        int D = size_to_layer(size);
        if (D < 0) return -1;
        bool ok = alloc_at_exact(1, tree_height, D, 0, ram_size, addr);
        return ok ? addr : -1;
    }

    void free_at_addr(int addr, int size) {
        if (!initialized) return;
        if (addr < 0 || addr >= ram_size) return;
        if (addr % size != 0) return;
        int D = size_to_layer(size);
        if (D < 0) return;
        (void)free_exact(1, tree_height, D, 0, ram_size, addr);
    }
};

} // namespace buddy_internal

// Global allocator instance and C++ linkage wrappers
static buddy_internal::BuddyAllocator __buddy_allocator;

inline void init(int ram_size, int min_block_size) {
    __buddy_allocator.init(ram_size, min_block_size);
}

inline void allocator_init(int ram_size, int min_block_size) { init(ram_size, min_block_size); }
inline void initialize(int ram_size, int min_block_size) { init(ram_size, min_block_size); }

inline int malloc(int size) {
    return __buddy_allocator.alloc_size(size);
}

inline int malloc_at(int addr, int size) {
    return __buddy_allocator.alloc_at(addr, size);
}

inline void free_at(int addr, int size) {
    __buddy_allocator.free_at_addr(addr, size);
}

#endif // SRC_HPP_BUDDY_ALLOCATOR_2199

