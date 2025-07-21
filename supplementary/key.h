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

// Park these here temporarily
#define GET_MAIN_LINE_MASK_HAS_VARIATIONS 1
#define GET_MAIN_LINE_MASK_HAS_COMMENTS 2
#define GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO 4
int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt, int &nbr_comments );
void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out );
int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, int &nbr_comments );

#endif
