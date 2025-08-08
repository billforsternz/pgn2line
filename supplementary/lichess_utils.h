// supplementary.cpp : Do something special to LPGN file
//  lichess-utils.h, Misc lichess file related stuff

#ifndef LICHESS_UTILS_H_INCLUDED
#define LICHESS_UTILS_H_INCLUDED
#include <vector>
#include <string>
#include "..\thc.h"
#include "key.h"
#include "lichess_utils.h"

// Park these here temporarily
#define GET_MAIN_LINE_MASK_HAS_VARIATIONS 1
#define GET_MAIN_LINE_MASK_HAS_COMMENTS 2
#define GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO 4

// Some refinement with naming etc definitely possible!
void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out );
std::string lichess_moves_to_normal_pgn_extra_stats();
int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, std::string &encoded_clk_times, int &nbr_comments );
int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::vector<int> &clk_times, std::string &moves_txt, int &nbr_comments );

#endif // LICHESS_UTILS_H_INCLUDED
