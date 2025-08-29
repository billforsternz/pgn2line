// supplementary.cpp : Do something special to LPGN file
//  lichess-utils.h, Misc lichess file related stuff

#ifndef BABY_H_INCLUDED
#define BABY_H_INCLUDED
#include <vector>
#include <string>

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times 
void baby_clk_encode( const std::vector<int> &clk_times, std::string &encoded_clk_times );

//  For now the reverse procedure is a test only
void baby_clk_decode( const std::string &encoded_clk_times, std::vector<int> &clk_times );

//  Generate extra stats output for diagnostics
std::string baby_stats_extra();

#endif // BABY_H_INCLUDED
