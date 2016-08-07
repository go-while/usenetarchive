#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <strings.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#include <gmime/gmime.h>

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

static const char* userEncodings[] = {
    "ISO8859-2",
    "CP1250",
    "CP852",
    nullptr
};

// Tables for polish characters extracted from polish.c v4.3.20 https://ipsec.pl/cpl
static const unsigned char plChars[][18] = {
    { 161,198,202,163,209,211,166,172,175,177,230,234,179,241,243,182,188,191 },
    { 165,198,202,163,209,211,140,143,175,185,230,234,179,241,243,156,159,191 },
    { 164,143,168,157,227,224,151,141,189,165,134,169,136,228,162,152,171,190 }
};

int weights[18]= { 1, 1, 1, 1, 1, 1, 1, 1, 1, 15, 10, 16, 19, 1, 13, 13, 1, 18 };

static int deductions[sizeof(userEncodings)/sizeof(const char*)] = {};

static std::string ConvertToUTF8( guint8* data, gint64 len )
{
    if( g_utf8_validate( (gchar*)data, len, nullptr ) == TRUE )
    {
        return std::string( data, data + len );
    }

    int res[sizeof(userEncodings)/sizeof(const char*)-1] = {};
    auto charset = userEncodings;
    auto test = plChars;
    int idx = 0;
    while( *charset )
    {
        int cnt[18] = {};
        for( int i=0; i<len; i++ )
        {
            for( int j=0; j<18; j++ )
            {
                if( data[i] == (*test)[j] )
                {
                    cnt[j]++;
                    break;
                }
            }
        }
        for( int i=0; i<18; i++ )
        {
            res[idx] += cnt[i] * weights[i];
        }
        charset++;
        test++;
        idx++;
    }

    int sel = 0;
    int sres = res[0];
    for( int i=1; i<sizeof(userEncodings)/sizeof(const char*)-1; i++ )
    {
        if( res[i] > sres )
        {
            sres = res[i];
            sel = i;
        }
    }

    if( sres != 0 )
    {
        deductions[sel]++;
    }

    iconv_t cv = g_mime_iconv_open( "UTF-8", userEncodings[sel] );
    char* converted = g_mime_iconv_strndup( cv, (const char*)data, len );
    if( converted && g_utf8_validate( converted, -1, nullptr ) == TRUE )
    {
        std::string ret = converted;
        g_free( converted );
        g_mime_iconv_close( cv );
        return ret;
    }

    deductions[sizeof(userEncodings)/sizeof(const char*)-1]++;
    return std::string( data, data + len );
}

static std::string mime_part_to_text( GMimeObject* obj )
{
    std::string ret;

    if( !GMIME_IS_PART( obj ) ) return ret;

    GMimeContentType* content_type = g_mime_object_get_content_type( obj );
    GMimeDataWrapper* c = g_mime_part_get_content_object( GMIME_PART( obj ) );
    GMimeStream* memstream = g_mime_stream_mem_new();

    gint64 len = g_mime_data_wrapper_write_to_stream( c, memstream );
    guint8* b = g_mime_stream_mem_get_byte_array( (GMimeStreamMem*)memstream )->data;

    const char* charset;
    if( ( charset = g_mime_content_type_get_parameter( content_type, "charset" ) ) && strcasecmp( charset, "utf-8" ) != 0 )
    {
        iconv_t cv = g_mime_iconv_open( "UTF-8", charset );
        if( char* converted = g_mime_iconv_strndup( cv, (const char*)b, len ) )
        {
            ret = converted;
            g_free( converted );
        }
        else
        {
            if( b )
            {
                ret = ConvertToUTF8( b, len );
            }
        }
        g_mime_iconv_close( cv );
    }
    else
    {
        if( b )
        {
            ret = ConvertToUTF8( b, len );
        }
    }

    g_mime_stream_close( memstream );
    g_object_unref( memstream );
    return ret;
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s source destination\n", argv[0] );
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

    g_mime_init( GMIME_ENABLE_RFC2047_WORKAROUNDS );
    const char* charsets[] = {
        "UTF-8",
        "ISO8859-2",
        "CP1250",
        0
    };
    g_mime_set_user_charsets( charsets );

    CreateDirStruct( argv[2] );

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::string dbase = argv[2];
    dbase.append( "/" );
    std::string dmetafn = dbase + "meta";
    std::string ddatafn = dbase + "data";

    FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
    FILE* ddata = fopen( ddatafn.c_str(), "wb" );

    int mime_fails = 0;
    std::ostringstream ss;
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

        GMimeStream* istream = g_mime_stream_mem_new_with_buffer( post, raw.size );
        GMimeParser* parser = g_mime_parser_new_with_stream( istream );
        g_object_unref( istream );

        GMimeMessage* message = g_mime_parser_construct_message( parser );
        g_object_unref( parser );

        GMimeHeaderList* ls = GMIME_OBJECT( message )->headers;
        GMimeHeaderIter* hit = g_mime_header_iter_new();

        if( g_mime_header_list_get_iter( ls, hit ) )
        {
            while( g_mime_header_iter_is_valid( hit ) )
            {
                auto name = g_mime_header_iter_get_name( hit );
                auto value = g_mime_header_iter_get_value( hit );
                auto tmp = g_mime_utils_header_decode_text( value );
                auto decode = g_mime_utils_header_decode_phrase( tmp );
                g_free( tmp );
                ss << name << ": " << decode << "\n";
                g_free( decode );
                if( !g_mime_header_iter_next( hit ) ) break;
            }
        }
        g_mime_header_iter_free( hit );

        std::string content;
        GMimeObject* last = nullptr;
        GMimePartIter* pit = g_mime_part_iter_new( (GMimeObject*)message );
        do
        {
            GMimeObject* part = g_mime_part_iter_get_current( pit );
            if( GMIME_IS_OBJECT( part ) && GMIME_IS_PART( part ) )
            {
                GMimeContentType* content_type = g_mime_object_get_content_type( part );
                if( content.empty() && ( !content_type || g_mime_content_type_is_type( content_type, "text", "plain" ) ) )
                {
                    content = mime_part_to_text( part );
                }
                if( g_mime_content_type_is_type( content_type, "text", "html" ) )
                {
                    last = part;
                }
            }
        }
        while( g_mime_part_iter_next( pit ) );
        g_mime_part_iter_free( pit );
        if( content.empty() )
        {
            if( last )
            {
                content = mime_part_to_text( last );
            }
        }
        if( content.empty() )
        {
            GMimeObject* body = g_mime_message_get_body( message );
            content = mime_part_to_text( body );
        }
        if( content.empty() )
        {
            mime_fails++;
            auto buf = post;
            for(;;)
            {
                auto end = buf;
                if( *end == '\n' ) break;
                while( *end != '\n' ) end++;
                buf = end + 1;
            }
            content = buf + 1;
        }

        ss << "\n" << content;
        g_object_unref( message );

        uint64_t size = ss.str().size();
        int maxSize = LZ4_compressBound( size );
        char* compressed = eb.Request( maxSize );
        int csize = LZ4_compress_HC( ss.str().c_str(), compressed, size, maxSize, 16 );

        fwrite( compressed, 1, csize, ddata );

        RawImportMeta packet = { offset, size, csize };
        fwrite( &packet, 1, sizeof( RawImportMeta ), dmeta );
        offset += csize;

        ss.str( "" );
    }

    printf( "Processed %i files.\n", size );

    fclose( dmeta );
    fclose( ddata );

    g_mime_shutdown();

    int idx = 0;
    while( userEncodings[idx] )
    {
        printf( "Deductions of %s encoding: %i\n", userEncodings[idx], deductions[idx] );
        idx++;
    }
    printf( "Deductions failed: %i\n", deductions[idx] );
    printf( "Completly broken messages: %i\n", mime_fails );

    return 0;
}
