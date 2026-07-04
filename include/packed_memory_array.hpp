#pragma once

#include <vector>
#include <bit>
#include <cstring>
#include <memory>
#include <ranges>

// here I try to keep consistency with stl-like naming sice it's stl-like container, but for very specific use-case

template<typename Key_, typename Value_, typename Compare = std::less<>>
class packed_memory_array {
public:
    using key_t = std::remove_cvref_t<Key_>;
    using value_t = std::remove_cvref_t<Value_>;
    using skip_t = std::size_t;
    using value_type = value_t;
    using reference = value_t&;
    using const_reference = const value_t&;
    using pointer = value_t*;
    using const_pointer = const value_t*;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;

    template<bool Const>
    class basic_iterator {
        using container = std::conditional_t<Const, const packed_memory_array, packed_memory_array>;
        friend class packed_memory_array;
        std::size_t idx;
        container* cont;
    public:
        friend class packed_memory_array;

        using iterator_category = std::forward_iterator_tag;
        using value_type = value_t;
        using reference = std::conditional_t<Const, const value_t&, value_t&>;
        using pointer = std::conditional_t<Const, const value_t*, value_t*>;
        using difference_type = std::ptrdiff_t;

        basic_iterator(container* cont, std::size_t idx) : cont(cont), idx(idx) {}

        reference operator*() const {
            return cont->values[idx];
        }

        pointer operator->() const {
            return &cont->values[idx];
        }

        basic_iterator &operator++() {
            idx = cont->get_next_live(idx);
            return *this;
        }

        basic_iterator operator++(int) {
            auto cp = *this;
            idx = cont->get_next_live(idx);
            return cp;
        }

        bool operator==(const basic_iterator &b) const{
            return idx == b.idx;
        }

        bool operator!=(const basic_iterator &b) const {
            return idx != b.idx;
        }
    };

    using iterator = basic_iterator<false>;
    using const_iterator = basic_iterator<true>;

    static constexpr std::size_t skip_bits{sizeof(skip_t) * 8};
    static constexpr std::size_t npos{static_cast<std::size_t>(-1)};
    static constexpr std::size_t scaling_factor{2};
    static constexpr std::size_t alloc_min_chunk{128};

    packed_memory_array() {
        allocate(alloc_min_chunk);
        fill_gap(0, capacity_, key_t{});
    }

    packed_memory_array(const packed_memory_array &) = delete;
    packed_memory_array& operator=(const packed_memory_array &) = delete;

    packed_memory_array(packed_memory_array&& other) noexcept {
        other.keys = std::exchange(keys, nullptr);
        other.values = std::exchange(values, nullptr);
        other.capacity_ = std::exchange(capacity_, 0);
        other.size_ = std::exchange(size_, 0);
        other.used_fields = std::move(used_fields);
        other.keys_allocator = std::move(keys_allocator);
        other.values_allocator = std::move(values_allocator);
    }

    packed_memory_array& operator=(packed_memory_array&& other) noexcept {
        deallocate();
        other.keys = std::exchange(keys, nullptr);
        other.values = std::exchange(values, nullptr);
        other.capacity_ = std::exchange(capacity_, 0);
        other.size_ = std::exchange(size_, 0);
        other.used_fields = std::move(used_fields);
        other.keys_allocator = std::move(keys_allocator);
        other.values_allocator = std::move(values_allocator);
        return *this;
    }

    ~packed_memory_array() {
        deallocate();
    }

    iterator begin() {
        return iterator{this, test_bit(0)? 0 : get_next_live(0)};
    }

    iterator end() {
        return iterator{this, npos};
    }

    const_iterator begin() const {
        return const_iterator{this, test_bit(0)? 0 : get_next_live(0)};
    }

    const_iterator end() const {
        return const_iterator{this, npos};
    }

    const_iterator cbegin() const {
        return const_iterator{this, test_bit(0)? 0 : get_next_live(0)};
    }

    const_iterator cend() const {
        return const_iterator{this, npos};
    }

    iterator find(const key_t &key) {
        std::size_t idx = lower_bound_slot(key);
        if (idx >= capacity_) {
            return end();
        }

        if (!test_bit(idx)) {
            idx = get_next_live(idx);
        }

        if (idx != npos && keys[idx] == key) {
            return iterator{this, idx};
        }
        return end();
    }

    iterator lower_bound(const key_t &key) {
        std::size_t idx = lower_bound_slot(key);
        if (idx >= capacity_) {
            return end();
        }
        if (!test_bit(idx)) {
            idx = get_next_live(idx);
        }
        return iterator{this, idx};
    }

    template<typename Val>
    std::pair<iterator, bool> insert(key_t key, Val && value) {
        if (size_ == 0) {
            return insert_at(key, std::forward<Val>(value), capacity_ / 2);
        }

        std::size_t idx = lower_bound_slot(key);

        if (idx == capacity_) {
            return insert_at(key, std::forward<Val>(value), append_slot());
        }

        if (keys[idx] == key) {
            std::size_t live_idx = test_bit(idx) ? idx : get_next_live(idx);
            return {iterator{this, live_idx}, false};
        }

        if (!test_bit(idx)) {
            return insert_at(key, std::forward<Val>(value), idx);
        }

        return insert_at(key, std::forward<Val>(value), make_room_at(idx));
    }

    iterator erase(const key_t &key) {
        return erase(find(key));
    }

    iterator erase(iterator it) {
        if (it == end()) {
            return end();
        }
        std::size_t idx = it.idx;
        --size_;
        set_unoccupied(idx);
        std::allocator_traits<std::allocator<value_t>>::destroy(values_allocator, &values[idx]);

        std::size_t next_live = get_next_live(idx);
        std::size_t prev_live = get_prev_live(idx);
        std::size_t start = prev_live + 1;
        if (next_live != npos) {
            fill_gap(start, next_live, keys[next_live]);
        } else if (prev_live != npos) {
            fill_gap(start, capacity_, keys[prev_live]);
        }

        return iterator{this, next_live};
    }

    std::size_t size() const noexcept {
        return size_;
    }

    std::size_t capacity() const noexcept {
        return capacity_;
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

private:
        std::size_t make_room_at(std::size_t idx) {
        if (std::size_t right = get_next_gap(idx); right != npos) {
            std::size_t to_move_count = right - idx;
            shift_right(idx, to_move_count);
            return idx;
        }
        if (std::size_t left = get_prev_gap(idx); left != npos) {
            // we know that idx points to an element greater than we want to insert, so we want to see if we can
            // move left part of the array so we could insert before that that key
            std::size_t prev_idx = idx - 1;
            std::size_t to_move_count = prev_idx - left;
            shift_left(prev_idx, to_move_count);
            return prev_idx;
        }

        // we didn't find a gap so after allocation it must be in both left and right end of the old buffer, offset adjusted
        if (idx < capacity_ / 2) {
            std::size_t offset = allocate(capacity_ * scaling_factor);
            idx += offset;
            std::size_t left = offset - 1;
            std::size_t prev_idx = idx - 1;
            std::size_t to_move_count = prev_idx - left;

            shift_left(prev_idx, to_move_count);
            return prev_idx;
        }
        std::size_t offset = allocate(capacity_ * scaling_factor);
        idx += offset;
        std::size_t next_gap_location = 3 * offset;
        shift_right(idx,  next_gap_location - idx);
        return idx;
    }

    std::size_t rightmost_live() const {
        for (auto [idx, word]: used_fields | std::views::enumerate | std::views::reverse) {
            if (word != 0) {
                return idx * skip_bits + (skip_bits - 1 - static_cast<std::size_t>(std::countl_zero(word)));
            }
        }
        return npos;
    }

    std::size_t append_slot() {
        std::size_t m = rightmost_live();
        if (m + 1 < capacity_) {
            return m + 1;
        }
        allocate(capacity_ * scaling_factor);
        return rightmost_live() + 1;
    }

    void shift_right(std::size_t start, std::size_t count) {
        std::memmove(&keys[start] + 1, &keys[start], count * sizeof(key_t));
        std::memmove(&values[start] + 1, &values[start], count * sizeof(value_t));
        set_occupied(start + count);
        set_unoccupied(start);
    }

    void shift_left(std::size_t start, std::size_t count) {
        std::memmove(&keys[start - count], &keys[start - count + 1], count * sizeof(key_t));
        std::memmove(&values[start - count], &values[start - count + 1], count * sizeof(value_t));
        set_occupied(start - count);
        set_unoccupied(start);
    }

    void deallocate() {
        for (std::size_t idx = test_bit(0) ? 0 : get_next_live(0); idx != npos; idx = get_next_live(idx)) {
            std::allocator_traits<std::allocator<value_t>>::destroy(values_allocator, &values[idx]);
        }
        keys_allocator.deallocate(keys, capacity_);
        values_allocator.deallocate(values, capacity_);
    }

    std::size_t allocate(std::size_t new_capacity) {
        std::unique_ptr<key_t[]> keys_buf{keys_allocator.allocate(new_capacity)};
        std::unique_ptr<value_t[]> values_buf{values_allocator.allocate(new_capacity)};
        std::vector<skip_t> new_skip_fields(new_capacity / skip_bits, 0);

        std::size_t offset{new_capacity / 4};

        std::memmove(keys_buf.get() + offset, keys, capacity_ * sizeof(key_t));
        std::memmove(values_buf.get() + offset, values, capacity_ * sizeof(value_t));
        std::memmove(new_skip_fields.data() + offset / skip_bits, used_fields.data(), used_fields.size() * sizeof(skip_t));

        keys_allocator.deallocate(keys, capacity_);
        values_allocator.deallocate(values, capacity_);

        keys = keys_buf.release();
        capacity_ = new_capacity;
        fill_gap(0, offset, keys[offset]);
        fill_gap(3 * offset, capacity_, keys[3 * offset - 1]);
        values = values_buf.release();
        used_fields = std::move(new_skip_fields);
        return offset;
    }

    std::size_t lower_bound_slot(const key_t& key) const {
        if (size_ == 0) {
            return capacity_ / 2;
        }
        std::size_t low{0};
        std::size_t high{capacity_};
        while (low < high) {
            auto mid = (low + high) / 2;
            if (compare(keys[mid], key)) {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        return low;
    }

    template<typename Val>
    std::pair<iterator, bool> insert_at(key_t key, Val && value, std::size_t idx) {
        keys[idx] = key;
        std::allocator_traits<std::allocator<value_t>>::construct(values_allocator, &values[idx], std::forward<value_t>(value));
        set_occupied(idx);
        std::size_t start = get_prev_live(idx) + 1;
        std::size_t end {idx};
        std::size_t next = idx + 1;
        if (next < capacity_ && compare(keys[next], key)) {
            end = get_next_live(idx);
            if (end == npos) {
                end = capacity_;
            }
        }

        fill_gap(start, end, key);
        ++size_;
        return {iterator{this, idx}, true};
    }

    void fill_gap(std::size_t start, std::size_t end, key_t key) {
        for (std::size_t idx{start}; idx < end; ++idx) {
            keys[idx] = key;
        }
    }

    void flip_bit(std::size_t idx) {
        used_fields[idx/skip_bits] ^= (skip_t{1} << (idx % skip_bits));
    }

    bool test_bit(std::size_t idx) const {
        return used_fields[idx/skip_bits] & (skip_t{1} << (idx % skip_bits));
    }

    void set_occupied(std::size_t idx) {
        used_fields[idx/skip_bits] |=  skip_t{1} << (idx % skip_bits);
    }

    void set_unoccupied(std::size_t idx) {
        used_fields[idx/skip_bits] &= ~(skip_t{1} << (idx % skip_bits));
    }

    template<typename F>
    std::size_t get_next(std::size_t idx, F&& operation_on_bit_field) const {
        if (idx >= capacity_) {
            return npos;
        }

        std::size_t skip_vec_idx{idx/skip_bits};
        std::size_t bit = idx % skip_bits;

        std::size_t mask = (bit == skip_bits -1) ? std::size_t{0} : std::numeric_limits<std::size_t>::max() << bit + 1;
        std::size_t current_word = operation_on_bit_field(used_fields[skip_vec_idx]) & mask;

        if (current_word != 0) {
            return skip_vec_idx * skip_bits + static_cast<std::size_t>(std::countr_zero(current_word));
        }

        for (std::size_t i = skip_vec_idx + 1; i < used_fields.size(); ++i) {
            if (operation_on_bit_field(used_fields[i]) != 0) {
                return (i * skip_bits) + std::countr_zero(operation_on_bit_field(used_fields[i]));
            }
        }
        return npos;
    }

    std::size_t get_next_live(std::size_t idx) const {
        return get_next(idx, std::identity{});
    }

    std::size_t get_next_gap(std::size_t idx) const {
        return get_next(idx, [](std::size_t field) { return ~field; });
    }

    template<typename F>
    std::size_t get_prev(std::size_t idx, F&& operation_on_bit_field) const {
        if (idx == 0) {
            return npos;
        }
        if (idx >= capacity_) {
            idx = capacity_ - 1;
        }

        std::size_t skip_vec_idx{idx/skip_bits};
        std::size_t bit = idx % skip_bits;

        std::size_t mask = bit == 0 ? 0: std::numeric_limits<std::size_t>::max() >> (64 - bit);
        std::size_t current_word = operation_on_bit_field(used_fields[skip_vec_idx]) & mask;
        if (current_word != 0) {
            return skip_vec_idx * skip_bits + (skip_bits - 1 - static_cast<std::size_t>(std::countl_zero(current_word)));
        }

        while (skip_vec_idx != 0) {
            --skip_vec_idx;
            std::size_t word = operation_on_bit_field(used_fields[skip_vec_idx]);
            if (word != 0) {
                return (skip_vec_idx * skip_bits) + (skip_bits - 1 - static_cast<std::size_t>(std::countl_zero(word)));
            }
        }

        return npos;
    }

    std::size_t get_prev_live(std::size_t idx) const {
        return get_prev(idx, std::identity{});
    }

    std::size_t get_prev_gap(std::size_t idx) const {
        return get_prev(idx, [](std::size_t field) { return ~field; });
    }

    key_t *keys{nullptr};
    value_t *values{nullptr};
    std::vector<skip_t> used_fields{};
    std::size_t size_{0};
    std::size_t capacity_{0};
    [[no_unique_address]] Compare compare{};
    std::allocator<key_t> keys_allocator{};
    std::allocator<value_t> values_allocator{};
};


