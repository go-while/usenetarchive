#include <algorithm>
#include <iterator>

#include "Archive.hpp"
#include "../common/String.hpp"

Archive::Archive( const std::string& dir )
    : m_mview( dir + "/zmeta", dir + "/zdata", dir + "/zdict" )
    , m_mcnt( m_mview.Size() )
    , m_toplevel( dir + "/toplevel" )
    , m_midhash( dir + "/middata", dir + "/midhash", dir + "/midhashdata" )
    , m_connectivity( dir + "/connmeta", dir + "/conndata" )
    , m_strings( dir + "/strmeta", dir + "/strings" )
    , m_lexmeta( dir + "/lexmeta" )
    , m_lexstr( dir + "/lexstr" )
    , m_lexdata( dir + "/lexdata" )
    , m_lexhit( dir + "/lexhit" )
    , m_lexhash( dir + "/lexstr", dir + "/lexhash", dir + "/lexhashdata" )
{
}

const char* Archive::GetMessage( uint32_t idx )
{
    return idx >= m_mcnt ? nullptr : m_mview[idx];
}

const char* Archive::GetMessage( const char* msgid )
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetMessage( idx ) : nullptr;
}

ViewReference<uint32_t> Archive::GetTopLevel() const
{
    return ViewReference<uint32_t> { m_toplevel, m_toplevel.DataSize() };
}

int32_t Archive::GetParent( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data++;
    return (int32_t)*data;
}

int32_t Archive::GetParent( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetParent( idx ) : -1;
}

ViewReference<uint32_t> Archive::GetChildren( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data += 2;
    auto num = *data++;
    return ViewReference<uint32_t> { data, num };
}

ViewReference<uint32_t> Archive::GetChildren( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetChildren( idx ) : ViewReference<uint32_t> { nullptr, 0 };
}

uint32_t Archive::GetDate( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    return *data;
}

uint32_t Archive::GetDate( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetDate( idx ) : 0;
}

const char* Archive::GetFrom( uint32_t idx ) const
{
    return m_strings[idx*2];
}

const char* Archive::GetFrom( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetFrom( idx ) : nullptr;
}

const char* Archive::GetSubject( uint32_t idx ) const
{
    return m_strings[idx*2+1];
}

const char* Archive::GetSubject( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetSubject( idx ) : nullptr;
}

struct PostData
{
    uint32_t postid;
    uint8_t hitnum;
    uint8_t children;
    const uint8_t* hits;
};

std::vector<uint32_t> Archive::Search( const char* query ) const
{
    std::vector<uint32_t> ret;

    std::vector<std::string> terms;
    split( query, std::back_inserter( terms ) );

    std::vector<int32_t> words;
    words.reserve( terms.size() );
    for( auto& v : terms )
    {
        auto res = m_lexhash.Search( v.c_str() );
        if( res >= 0 )
        {
            words.emplace_back( res );
        }
    }

    if( words.empty() ) return ret;

    std::vector<std::vector<PostData>> wdata;
    wdata.reserve( words.size() );
    for( auto& v : words )
    {
        auto meta = m_lexmeta[v];
        auto data = m_lexdata + ( meta.data / sizeof( LexiconDataPacket ) );

        wdata.emplace_back();
        auto& vec = wdata.back();
        vec.reserve( meta.dataSize );
        for( uint32_t i=0; i<meta.dataSize; i++ )
        {
            uint8_t children = data->postid >> LexiconChildShift;
            auto hits = m_lexhit + data->hitoffset;
            auto hitnum = *hits++;
            vec.emplace_back( PostData { data->postid & LexiconPostMask, hitnum, children, hits } );
            data++;
        }
    }

    std::vector<PostData> merged;
    if( wdata.size() == 1 )
    {
        std::swap( merged, *wdata.begin() );
    }
    else
    {
        auto& vec = *wdata.begin();
        auto wsize = wdata.size();
        for( auto& post : vec )
        {
            bool ok = true;
            for( size_t i=1; i<wsize; i++ )
            {
                auto& vtest = wdata[i];
                if( !std::binary_search( vtest.begin(), vtest.end(), post, [] ( const auto& l, const auto& r ) { return l.postid < r.postid; } ) )
                {
                    ok = false;
                    break;
                }
            }
            if( ok )
            {
                merged.emplace_back( post );
            }
        }
    }

    if( merged.empty() ) return ret;

    return ret;
}
