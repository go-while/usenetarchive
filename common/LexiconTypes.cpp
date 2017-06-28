#include "LexiconTypes.hpp"

const char* LexiconNames[] = {
    "Content",
    "Signature",
    "Quote1",
    "Quote2",
    "Quote3",
    "Header",
    "Wrote"
};

float const LexiconWeights[] = {
    1.0f,
    0.5f,
    0.7f,
    0.5f,
    0.3f,
    0.1f,
    0.05f
};

const uint8_t LexiconHitTypeEncoding[] = {
    0x00,   // 0000 0000
    0xE0,   // 1110 0000
    0x80,   // 1000 0000
    0xA0,   // 1010 0000
    0xC0,   // 1100 0000
    0xF0,   // 1111 0000
    0xF8    // 1111 1000
};

const uint8_t LexiconHitPosMask[] = {
    0x7F,   // 0111 1111
    0x1F,   // 0001 1111
    0x1F,   // 0001 1111
    0x1F,   // 0001 1111
    0x0F,   // 0000 1111
    0x07,   // 0000 0111
    0x07    // 0000 0111
};


static_assert( sizeof( LexiconNames ) / sizeof( const char* ) == NUM_LEXICON_TYPES, "Bad table size" );
static_assert( sizeof( LexiconWeights ) / sizeof( float ) == NUM_LEXICON_TYPES, "Bad table size" );
static_assert( sizeof( LexiconHitTypeEncoding ) / sizeof( uint8_t ) == NUM_LEXICON_TYPES, "Bad table size" );
static_assert( sizeof( LexiconHitPosMask ) / sizeof( uint8_t ) == NUM_LEXICON_TYPES, "Bad table size" );
