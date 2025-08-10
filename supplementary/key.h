// supplementary.cpp : Do something special to LPGN file
//  key.cpp, Key utilities, Tags are key value pairs

#ifndef KEY_H_INCLUDED
#define KEY_H_INCLUDED
#include <string>
#include <vector>
#include "../thc.h"

bool key_find( const std::string header, const std::string key, std::string &val );
bool key_replace( std::string &header, const std::string key, const std::string val );
bool key_delete( std::string &header, const std::string key );
void key_add( std::string &header, const std::string key, const std::string val );
void key_update( std::string &header, const std::string key, const std::string key_insert_after, const std::string val );
void key_update2( std::string &header, const std::string key, const std::string key_insert_after1,
                                                              const std::string key_insert_after2, const std::string val );
#endif
