#pragma once

#include "suffix_trie.hpp"

namespace tongrams {

typedef suffix_trie<double_valued_mpht64,           // vocabulary
                    quantized_sequence_collection,  // values
                    pef::uniform_pef_sequence,      // words ids
                    ef_sequence                     // pointers
                    >
    suffix_trie_index;

}
