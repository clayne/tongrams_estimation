#pragma once

#ifndef __APPLE__
#include <parallel/algorithm>
#endif

#include "util.hpp"
#include "hash_utils.hpp"
#include "ngrams_block.hpp"
#include "parallel_radix_sort.hpp"

namespace tongrams {

template <typename Value, typename Prober = hash_utils::linear_prober,
          typename EqualPred = equal_to>
struct ngrams_hash_block {
    static constexpr ngram_id invalid_ngram_id = ngram_id(-1);

    ngrams_hash_block() : m_size(0), m_num_bytes(0) {
        resize(0);
    }

    void init(uint8_t ngram_order, uint64_t size,
              Prober const& prober = Prober(),
              EqualPred const& equal_to = EqualPred()) {
        m_prober = prober;
        m_equal_to = equal_to;
        m_num_bytes = ngram_order * sizeof(word_id);
        m_block.init(ngram_order);
        resize(size);
    }

    void resize(uint64_t size) {
        uint64_t buckets = size * hash_utils::probing_space_multiplier;
        m_data.resize(buckets, invalid_ngram_id);
        m_block.resize_memory(size);

#ifdef LSD_RADIX_SORT
        m_block.resize_index(size);
#else
        m_block.resize_index(0);
#endif
    }

    bool find_or_insert(ngram const& key, iterator hint, ngram_id& at) {
        m_prober.init(hint, buckets());
        iterator start = *m_prober;
        iterator it = start;

        while (m_data[it] != invalid_ngram_id) {
            assert(it < buckets());
            bool equal = m_equal_to(pointer(m_data[it]).data, key.data.data(),
                                    m_num_bytes);
            if (equal) {
                at = m_data[it];
                return true;
            }
            ++m_prober;
            it = *m_prober;
            if (it == start) {  // back to starting point:
                                // thus all positions have been checked
                std::cerr << "ERROR: all positions have been checked"
                          << std::endl;
                at = invalid_ngram_id;
                return false;
            }
        }

        // insert
        m_data[it] = m_size++;
        at = m_data[it];
        m_block.set(at, key.data.begin(), key.data.end(), Value(1));
        return false;
    }

    template <typename Comparator>
    void sort(Comparator const& comparator) {
        std::cerr << "block size = " << m_size << std::endl;
#ifdef LSD_RADIX_SORT
        (void)comparator;
        auto begin = m_block.begin();
        auto end = begin + size();
        uint32_t max_digit = statistics().max_word_id;
        uint32_t num_digits = m_block.order();
        // std::cerr << "max_digit = " << max_digit
        //           << "; num_digits = " << num_digits << std::endl;
        parallel_lsd_radix_sorter<typename ngrams_block<Value>::iterator>
            sorter(max_digit, num_digits);
        sorter.sort(begin, end);
        assert(m_block.template is_sorted<Comparator>(begin, end));
#else
        m_index.resize(size());
        for (size_t i = 0; i != size(); ++i) m_index[i] = i;
#ifdef __APPLE__
        std::sort
#else
        __gnu_parallel::sort
#endif
            (m_index.begin(), m_index.end(), [&](size_t i, size_t j) {
                return comparator(m_block.access(i), m_block.access(j));
            });
#endif
    }

    inline auto pointer(ngram_id at) {
        assert(at < size());
#ifdef LSD_RADIX_SORT
        return m_block[at];
#else
        return m_block.access(at);
#endif
    }

    inline typename Value::value_type& operator[](ngram_id at) {
        assert(at < size());
        return m_block.value(at);
    }

    inline uint64_t size() const {
        return m_size;
    }

    inline bool empty() const {
        return size() == 0;
    }

    inline uint64_t buckets() const {
        return m_data.size();
    }

    double load_factor() const {
        return static_cast<double>(size()) / buckets();
    }

    auto& block() {
        return m_block;
    }

    auto begin() {
        return enumerator(*this);
    }

    auto end() {
        return enumerator(*this, m_size);
    }

    auto& index() {
        return m_index;
    }

    struct enumerator {
        enumerator(ngrams_hash_block<Value, Prober, EqualPred>& block,
                   size_t pos = 0)
            : m_pos(pos), m_block(block), m_index(block.index()) {}

        bool operator==(enumerator const& rhs) {
            return m_pos == rhs.m_pos;
        }

        bool operator!=(enumerator const& rhs) {
            return not(*this == rhs);
        }

        void operator++() {
            ++m_pos;
        }

        auto operator*() {
#ifdef LSD_RADIX_SORT
            return m_block.pointer(m_pos);
#else
            return m_block.pointer(m_index[m_pos]);
#endif
        }

    private:
        size_t m_pos;
        ngrams_hash_block<Value, Prober, EqualPred>& m_block;
        std::vector<ngram_id>& m_index;
    };

    void swap(ngrams_hash_block<Value, Prober, EqualPred>& other) {
        std::swap(m_size, other.m_size);
        std::swap(m_num_bytes, other.m_num_bytes);
        m_data.swap(other.m_data);
        m_block.swap(other.m_block);
        m_index.swap(other.m_index);
    }

    void release_hash_index() {
        std::vector<ngram_id>().swap(m_data);
    }

    void release() {
        ngrams_hash_block().swap(*this);
    }

    auto& statistics() {
        return m_block.stats;
    }

private:
    uint64_t m_size;
    size_t m_num_bytes;
    Prober m_prober;
    EqualPred m_equal_to;
    std::vector<ngram_id> m_data;
    ngrams_block<Value> m_block;
    std::vector<ngram_id> m_index;
};

}  // namespace tongrams
