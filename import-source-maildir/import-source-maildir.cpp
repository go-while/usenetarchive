#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/RawImportMeta.hpp"

static int idx = 0;
static int dirs = 0;
static FILE* meta;
static FILE* data;
static uint64_t offset = 0;
static ExpandingBuffer eb1, eb2, eb3;

void RecursivePack( const char* dir )
{
    dirs++;
    const auto list = ListDirectory( dir );
    char in[1024];
    int fpos = strlen( dir );
    memcpy( in, dir, fpos );
    in[fpos++] = '/';

    for( const auto& f : list )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "[%i] %i\r", dirs, idx );
            fflush( stdout );
        }

        if( f[0] == '.' ) continue;
        strcpy( in+fpos, f.c_str() );
        if( f.back() == '/' )
        {
            RecursivePack( in );
        }
        else
        {
            idx++;
            uint64_t size = GetFileSize( in );
            char* buf = eb1.Request( size );
            FILE* fsrc = fopen( in, "rb" );
            fread( buf, 1, size, fsrc );
            fclose( fsrc );

            char* processed = eb3.Request( size );
            auto osp = size;
            auto src = buf;
            auto dst = processed;
            for( int i=0; i<osp; i++ )
            {
                if( *src == '\r' )
                {
                    size--;
                    src++;
                }
                else
                {
                    *dst++ = *src++;
                }
            }

            int maxSize = LZ4_compressBound( size );
            char* compressed = eb2.Request( maxSize );
            int csize = LZ4_compress_HC( processed, compressed, size, maxSize, 16 );

            fwrite( compressed, 1, csize, data );

            RawImportMeta metaPacket = { offset, uint32_t( size ), uint32_t( csize ) };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
            offset += csize;
        }
    }
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s input output\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );

    std::string metafn = argv[2];
    metafn.append( "/" );
    std::string datafn = metafn;
    metafn.append( "meta" );
    datafn.append( "data" );
    meta = fopen( metafn.c_str(), "wb" );
    data = fopen( datafn.c_str(), "wb" );

    RecursivePack( argv[1] );

    printf( "%i files processed in %i directories.\n", idx, dirs );

    fclose( meta );
    fclose( data );

    return 0;
}
