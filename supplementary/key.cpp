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

void key_update_subkey( std::string &header, const std::string key, const std::string subkey, const std::string subval )
{
    printf( "subkey=%s, subval=%s\n", subkey.c_str(), subval.c_str() );
    std::string current;
    std::string subkey_eq = subkey + '=';
    bool exists = key_find( header, key, current );
    if( !exists )
    {
        key_add( header, key, subkey_eq+subval );
    }
    else
    {
        bool found=false;
        size_t offset=0;
        for(;;)
        {
            offset = current.find(subkey_eq,offset);
            if( offset == std::string::npos )
            {
                found = false;
                break;
            }
            if( offset == 0 )
            {
                found = true;
                break;
            }
            if( current[offset-1] = ' ' )
            {
                found = true;
                break;
            }
            offset++;
        }
        printf( "subkey=%s, subval=%s found=%s\n", subkey.c_str(), subval.c_str(), found?"true":"false" );
        if( !found )
        {
            if( current.length() > 0 )
                current += " ";
            current += subkey_eq;
            current += subval;
        }
        else
        {
            auto offset2 = current.find_first_of(" \t",offset);
            if( offset2 == std::string::npos )
                offset2 = current.length();
            size_t slice = offset2-offset;
            printf( "replace_once %s\n", subval.c_str() );
            util::replace_once( current, current.substr(offset,slice), subkey_eq+subval );
        }
        key_replace( header, key, current );
    }
}

bool key_find_subkey( std::string &header, const std::string key, const std::string subkey, std::string &subval )
{
    std::string current;
    std::string subkey_eq = subkey + '=';
    bool exists = key_find( header, key, current );
    bool found = false;
    if( exists )
    {
        size_t offset=0;
        for(;;)
        {
            offset = current.find(subkey_eq,offset);
            if( offset == std::string::npos )
            {
                found = false;
                break;
            }
            if( offset == 0 )
            {
                found = true;
                break;
            }
            if( current[offset-1] = ' ' )
            {
                found = true;
                break;
            }
            offset++;
        }
        if( found )
        {
            auto offset2 = current.find_first_of(" \t",offset);
            if( offset2 == std::string::npos )
                offset2 = current.length();
            size_t len = (offset2-offset) - subkey_eq.length();
            subval = current.substr( offset + subkey_eq.length(), len );
        }
    }
    return found;
}


// Add a simple token to a subfield
void key_update_subfield( std::string &header, const std::string key, const std::string subfield )
{
    std::string current;
    bool exists = key_find( header, key, current );
    if( !exists )
    {
        key_add( header, key, subfield );
    }
    else
    {
        bool found=false;
        size_t offset=0;
        for(;;)
        {
            offset = current.find(subfield,offset);
            if( offset == std::string::npos )
            {
                found = false;
                break;
            }
            char before = ' ';
            char after = ' ';
            if( offset>0 )
                before = current[offset-1];
            if( offset+subfield.length() < current.length() ) 
                after = current[offset+subfield.length()];
            if( before==' ' && after==' ' )
            {
                found = true;
                break;
            }
            offset++;
        }
        if( !found )
        {
            if( current.length() > 0 )
                current += " ";
            current += subfield;
            key_replace( header, key, current );
        }
    }
}

