#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "../contrib/zstd/common/zstd.h"
#include "../contrib/zstd/dictBuilder/zdict.h"

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    auto size = mview.Size();

    std::string zmetafn = base + "zmeta";
    std::string zdatafn = base + "zdata";
    std::string zdictfn = base + "zdict";

    printf( "Building dictionary\n" );

    std::string buf1fn = base + ".sb.tmp";
    std::string buf2fn = base + ".ss.tmp";

    FILE* buf1 = fopen( buf1fn.c_str(), "wb" );
    FILE* buf2 = fopen( buf2fn.c_str(), "wb" );
    uint64_t total = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto raw = mview.Raw( i );
        if( total + raw.size >= ( 1U<<31 ) )
        {
            printf( "Limiting sample size to %i MB - %i samples in, %i samples out.\n", total >> 20, i, size - i );
            size = i;
            break;
        }
        total += raw.size;

        auto post = mview[i];

        fwrite( post, 1, raw.size, buf1 );
        fwrite( &raw.size, 1, sizeof( size_t ), buf2 );
    }
    fclose( buf1 );
    fclose( buf2 );

    enum { DictSize = 1024*1024 };
    auto dict = new char[DictSize];
    size_t realDictSize;

    {
        auto samplesBuf = FileMap<char>( buf1fn );
        auto samplesSizes = FileMap<size_t>( buf2fn );

        printf( "\nWorking...\n" );
        fflush( stdout );

        ZDICT_params_t params;
        memset( &params, 0, sizeof( ZDICT_params_t ) );
        params.notificationLevel = 3;
        params.compressionLevel = 16;
        realDictSize = ZDICT_trainFromBuffer_advanced( dict, DictSize, samplesBuf, samplesSizes, size, params );
    }

    unlink( buf1fn.c_str() );
    unlink( buf2fn.c_str() );

    printf( "Dict size: %i\n", realDictSize );

    auto zdict = ZSTD_createCDict( dict, realDictSize, 16 );
    auto zctx = ZSTD_createCCtx();

    FILE* zdictfile = fopen( zdictfn.c_str(), "wb" );
    fwrite( dict, 1, realDictSize, zdictfile );
    fclose( zdictfile );
    delete[] dict;

    FILE* zmeta = fopen( zmetafn.c_str(), "wb" );
    FILE* zdata = fopen( zdatafn.c_str(), "wb" );

    printf( "Repacking\n" );
    ExpandingBuffer eb;
    uint64_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto raw = mview.Raw( i );

        auto predSize = ZSTD_compressBound( raw.size );
        auto dst = eb.Request( predSize );
        auto dstSize = ZSTD_compress_usingCDict( zctx, dst, predSize, post, raw.size, zdict );

        RawImportMeta packet = { offset, raw.size, dstSize };
        fwrite( &packet, 1, sizeof( RawImportMeta ), zmeta );

        fwrite( dst, 1, dstSize, zdata );
        offset += dstSize;
    }

    ZSTD_freeCDict( zdict );
    ZSTD_freeCCtx( zctx );

    fclose( zmeta );
    fclose( zdata );

    return 0;
}
