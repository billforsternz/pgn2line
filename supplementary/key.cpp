// supplementary.cpp : Do something special to LPGN file
//  key.cpp, Key utilities, Tags are key value pairs

//
//  Note that older commands do not use these utilities (but could profitably be recoded to use them)
//

#include "..\util.h"
#include "..\thc.h"
#include "key.h"

bool key_find( const std::string header, const std::string key, std::string &val )
{
    bool found = false;
    std::string skey = std::string("@H[") + key + " \"";
    auto offset1 = header.find(skey);
    size_t key_len = skey.length();
    if( offset1 != std::string::npos )
    {
        auto offset2 = header.find("\"]",offset1+key_len);
        if( offset2 != std::string::npos )
        {
            found = true;
            val = header.substr(offset1+key_len,offset2-offset1-key_len);
        }
    }
    return found;
}

bool key_replace( std::string &header, const std::string key, const std::string val )
{
    bool found = false;
    std::string skey = std::string("@H[") + key + " \"";
    auto offset1 = header.find(skey);
    size_t key_len = skey.length();
    if( offset1 != std::string::npos )
    {
        auto offset2 = header.find("\"]",offset1+key_len);
        if( offset2 != std::string::npos )
        {
            found = true;
            std::string tail = header.substr(offset2);
            header = header.substr(0,offset1);
            header += skey;
            header += val;
            header += tail;
        }
    }
    return found;
}

bool key_delete( std::string &header, const std::string key )
{
    bool found = false;
    std::string skey = std::string("@H[") + key + " \"";
    auto offset1 = header.find(skey);
    size_t key_len = skey.length();
    if( offset1 != std::string::npos )
    {
        auto offset2 = header.find("\"]",offset1+key_len);
        if( offset2 != std::string::npos )
        {
            found = true;
            offset2 += 2;
            std::string tail = header.substr(offset2);
            header = header.substr(0,offset1);
            header += tail;
        }
    }
    return found;
}

void key_add( std::string &header, const std::string key, const std::string val )
{
    header += (std::string("@H[") + key + " \"");
    header += val;
    header += std::string("\"]");
}

void key_update( std::string &header, const std::string key, const std::string key_insert_after, const std::string val )
{
    if( !key_replace(header,key,val) )
    {
        bool found = false;
        std::string skey = std::string("@H[") + key_insert_after + " \"";
        auto offset1 = header.find(skey);
        size_t key_len = skey.length();
        if( offset1 != std::string::npos )
        {
            auto offset2 = header.find("\"]",offset1+key_len);
            if( offset2 != std::string::npos )
            {
                offset2 += 2;
                found = true;
                std::string tail = header.substr(offset2);
                header = header.substr(0,offset2);
                header += (std::string("@H[") + key + " \"");
                header += val;
                header += std::string("\"]");
                header += tail;
            }
        }
        if( !found || key_insert_after=="" )
            key_add(header,key,val);
    }
}

void key_update2( std::string &header, const std::string key, const std::string key_insert_after1,
                                                              const std::string key_insert_after2, const std::string val )
{
    if( !key_replace(header,key,val) )
    {
        bool found = false;
        for( int i=0; !found && i<2; i++ )
        {
            std::string skey = std::string("@H[") + (i==0?key_insert_after1:key_insert_after2) + " \"";
            auto offset1 = header.find(skey);
            size_t key_len = skey.length();
            if( offset1 != std::string::npos )
            {
                auto offset2 = header.find("\"]",offset1+key_len);
                if( offset2 != std::string::npos )
                {
                    offset2 += 2;
                    found = true;
                    std::string tail = header.substr(offset2);
                    header = header.substr(0,offset2);
                    header += (std::string("@H[") + key + " \"");
                    header += val;
                    header += std::string("\"]");
                    header += tail;
                }
            }
        }
        if( !found || key_insert_after1=="" )
            key_add(header,key,val);
    }
}
