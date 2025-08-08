// supplementary.cpp : Do something special to LPGN file
//  tabiya.cpp, the Tabiya command

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "..\util.h"
#include "..\thc.h"
#include "key.h"
#include "lichess_utils.h"
#include "tabiya.h"

//
// Find some interesting Tabiyas
//  

#define TABIYA_PLY 16
#define CUTOFF 10
#define PLAYER_MIN_GAMES 20

struct Tabiya
{
    bool skip=false;        // skip over this one
    bool both=false;        // best in both nbr and proportion
    bool absolute=false;    // best in nbr
    bool white=true;
    int nbr_ply=0;
    int nbr_player_games=0;
    std::string squares;
    std::string fide_id;
    int nbr_hits;
    double proportion_hits;
};

static void func_tabiya_make_maps
(
    std::ifstream &in, 
    std::map<std::string,std::vector<std::string>> &white_games,
    std::map<std::string,std::vector<std::string>> &black_games
);
static void func_tabiya_body
(
    std::ofstream &out,
    std::map<std::string,std::vector<std::string>> &games,
    bool white, int nbr_ply
);

int cmd_tabiya( std::ifstream &in, std::ofstream &out )
{
    std::map<std::string,std::vector<std::string>> white_games;
    std::map<std::string,std::vector<std::string>> black_games;
    func_tabiya_make_maps(in,white_games,black_games);
    func_tabiya_body(out,white_games,true,TABIYA_PLY);
    func_tabiya_body(out,black_games,false,TABIYA_PLY);
    return 0;
}

static void func_tabiya_make_maps
(
    std::ifstream &in, 
    std::map<std::string,std::vector<std::string>> &white_games,
    std::map<std::string,std::vector<std::string>> &black_games
)
{
    std::vector<Tabiya> best_tabiyas_nbr;
    std::vector<Tabiya> best_tabiyas_proportion;
    std::string line;
    int line_nbr=0;
    int nbr_white_players=0, nbr_black_players=0;

    // Build maps of (vector of games) for each player
    printf( "Begin reading input file, make White and Black maps of games for each player\n");
    line_nbr=0;
    for(;;)
    {
        if( !std::getline(in, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        if( line_nbr%10000 == 0 )
            printf( "%d lines\n", line_nbr );
        util::rtrim(line);
        auto offset = line.find("@M");
        if( offset == std::string::npos )
            continue;
        std::string header = line.substr(0,offset);
        std::string white_fide_id;

#ifdef TEMP_DONT_HAVE_FIDE_ID
        bool ok  = key_find( header, "White", white_fide_id );
#else
        bool ok  = key_find( header, "WhiteFideId", white_fide_id );
#endif
        if( ok )
        {
            auto it = white_games.find(white_fide_id);
            if( it != white_games.end() )
            {
                std::vector<std::string> &games = it->second;
                games.push_back(line);
            }
            else
            {
                std::vector<std::string> games;
                games.push_back(line);
                white_games[white_fide_id] = games;
                nbr_white_players++;
            }
        }
        std::string black_fide_id;
#ifdef TEMP_DONT_HAVE_FIDE_ID
        ok = key_find( header, "Black", black_fide_id );
#else
        ok = key_find( header, "BlackFideId", black_fide_id );
#endif
        if( ok )
        {
            auto it = black_games.find(black_fide_id);
            if( it != black_games.end() )
            {
                std::vector<std::string> &games = it->second;
                games.push_back(line);
            }
            else
            {
                std::vector<std::string> games;
                games.push_back(line);
                black_games[black_fide_id] = games;
                nbr_black_players++;
            }
        }
    }
    printf( "End reading input file %d white players, %d black players, %d lines\n", nbr_white_players, nbr_black_players, line_nbr );
}

static bool lt_nbr_hits( const Tabiya &left, const Tabiya &right )
{
    return left.nbr_hits < right.nbr_hits;
}

static bool lt_proportion_hits( const Tabiya &left, const Tabiya &right )
{
    return left.proportion_hits < right.proportion_hits;
}

static void func_tabiya_body
(
    std::ofstream &out,
    std::map<std::string,std::vector<std::string>> &games,
    bool white, int nbr_ply
)
{
    printf( "Look for %s Tabiyas after %d ply\n", white?"White":"Black", nbr_ply );
    std::vector<Tabiya> best_tabiyas_nbr;
    std::vector<Tabiya> best_tabiyas_proportion;
    int nbr_players = 0;

    // For each player in turn
    for( const std::pair<std::string,std::vector<std::string>> &player: games )
    {
        size_t nbr_of_games = player.second.size();
        nbr_players++;
        if( nbr_players%100 == 0)
            printf( "%d players\n", nbr_players );
        if( nbr_of_games < PLAYER_MIN_GAMES )
            continue;

        // Build a vector of positions after nbr_ply half moves
        std::vector<std::string> positions;
        for( const std::string &line: player.second )
        {
            std::vector<std::string> main_line;
            std::vector<int> clk_times;
            int nbr_comments;
            std::string moves_txt;
            get_main_line( line, main_line, clk_times, moves_txt, nbr_comments );
            std::vector<thc::Move> moves;
            convert_moves( main_line, moves );
            thc::ChessRules cr;
            int count = 0;
            for( thc::Move mv: moves )
            {
                cr.PlayMove( mv );
                if( ++count == nbr_ply )
                {
                    positions.push_back(cr.squares);
                    break;
                }
            }
        }

        // Look for multiple instances of one position
        std::sort( positions.begin(), positions.end() );
        int in_a_row = 0;
        int max_so_far = 0;
        std::string best_so_far;
        std::string previous;
        for( const std::string &pos: positions )
        {
            if( in_a_row == 0 )
            {
                previous = pos;
                in_a_row = 1;
            }
            else
            {
                if( previous == pos )
                    in_a_row++;
                else
                {
                    if( in_a_row >= max_so_far )
                    {
                        max_so_far = in_a_row;
                        best_so_far = previous;
                    }
                    previous = pos;
                    in_a_row = 1;
                }
            }
        }
        if( max_so_far < 2 )
            continue;

        // We now have the "best" (most frequent) position for this player
        //  is it good enough to be a tabiya ?
        double proportion = ( (double)(max_so_far) / (double)(nbr_of_games?nbr_of_games:1) );
        Tabiya tabiya;
        tabiya.fide_id = player.first;
        tabiya.nbr_hits = max_so_far;
        tabiya.proportion_hits = proportion;
        tabiya.nbr_player_games = (int)nbr_of_games;
        tabiya.squares = best_so_far;
        tabiya.white = white;
        if( best_tabiyas_nbr.size() < CUTOFF )
            best_tabiyas_nbr.push_back(tabiya);
        else
        {
            std::sort( best_tabiyas_nbr.begin(),  best_tabiyas_nbr.end(), lt_nbr_hits );
            if( max_so_far > best_tabiyas_nbr[0].nbr_hits )
                best_tabiyas_nbr[0] = tabiya;
        }
        if( best_tabiyas_proportion.size() < CUTOFF )
            best_tabiyas_proportion.push_back(tabiya);
        else
        {
            std::sort( best_tabiyas_proportion.begin(),  best_tabiyas_proportion.end(), lt_proportion_hits );
            if( proportion > best_tabiyas_proportion[0].proportion_hits )
                best_tabiyas_proportion[0] = tabiya;
        }
    }
    printf( "%d players\n", nbr_players );

    // Merge the two sets of Tabiyas
    std::vector<Tabiya> merged;
    for( Tabiya &t1: best_tabiyas_nbr )
    {
        t1.absolute = true;
        if( t1.nbr_hits == 0 )
            continue;
        for( Tabiya &t2: best_tabiyas_proportion )
        {
            if( t1.fide_id==t2.fide_id && t1.squares==t2.squares )
            {
                t1.both = true;
                t2.skip = true;
            }
        }
        merged.push_back(t1);
    }
    for( Tabiya &t2: best_tabiyas_proportion )
    {
        t2.absolute = false;
        if( t2.nbr_hits == 0 )
            continue;
        if( !t2.skip )
            merged.push_back(t2);
    }

    // For each Tabiya
    for( const Tabiya &tabiya: merged )
    {

        const std::string first_game = games[tabiya.fide_id][0];
        std::string name("?");
        key_find( first_game, tabiya.white?"White":"Black", name );
        std::string desc = util::sprintf( "Player %s, fide_id=%s, %s, %d games = %.2f%% percent of total=%d",
            name.c_str(),
            tabiya.fide_id.c_str(),
            tabiya.white ? "White" : "Black",
            tabiya.nbr_hits,
            tabiya.proportion_hits * 100.0,
            tabiya.nbr_player_games
        );
        if( tabiya.both )
            desc += " (high in absolute and relative terms)";
        else
            desc += (tabiya.absolute ? " (high in absolute terms)" : " (high in relative terms)");
        desc += util::sprintf( ", position after %d ply =\n", tabiya.nbr_ply );
        char buf[100];
        char *dst = buf;
        const char *src = tabiya.squares.c_str();
        for( int rank=0; rank<8; rank++ )
        {
            for( int file=0; file<8; file++ )
            {
                char c = *src++;
                *dst++ = (c==' '?'.':c);
            }
            *dst++ = '\n';
        }
        *dst++ = '\0';
        desc += buf; 
        printf( "%s\n", desc.c_str() );

        // For each of the players games, see if it reaches the Tabiya
        for( const std::string game: games[tabiya.fide_id] )
        {
            
            std::vector<std::string> main_line;
            std::vector<int> clk_times;
            int nbr_comments;
            std::string moves_txt;
            get_main_line( game, main_line, clk_times, moves_txt, nbr_comments );
            std::vector<thc::Move> moves;
            convert_moves( main_line, moves );
            thc::ChessRules cr;
            int count = 0;
            for( thc::Move mv: moves )
            {
                cr.PlayMove( mv );
                if( ++count == nbr_ply )
                {
                    if( cr.squares == tabiya.squares )
                    {
                        // If it does, add it to output
                        util::putline(out,game);
                    }
                    break;
                }
            }
        }

    }
}
