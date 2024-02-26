// pgn2crosstable.cpp
//
//    Adapted from lichess-fixup in this repo
//

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

bool core_pgn2line( std::string fin, std::vector<std::string> &vout );
bool key_find( const std::string header, const std::string key, std::string &val );
bool key_replace( std::string &header, const std::string key, const std::string val );
bool key_delete( std::string &header, const std::string key );
void key_add( std::string &header, const std::string key, const std::string val );
void key_update( std::string &header, const std::string key, const std::string key_insert_after, const std::string val );
static int func_gen_crosstable( std::vector<std::string> &in, FILE *fout, std::vector<std::string> &tiebreaks );

int main( int argc, const char **argv )
{
#ifdef _DEBUG
    const char *args[] =
    {
        "dontcare.exe",
        "C:/Users/Bill/Downloads/tournament_pgn_39740.pgn",
        "crosstable.txt",
        "tiebreak.txt"
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args;
#endif
    bool ok = (argc==3 || argc==4);
    if( !ok )
    {
        printf( 
            "Generate Crosstable from PGN\n"
            "Usage:\n"
            "pgn2crosstable in.pgn out.txt [tiebreak.txt]\n"
            "optional tiebreak.txt is an ordered list of players, to guide tiebreaks\n"
        );
        return -1;
    }
    std::string fin(argv[1]);
    FILE *fout=0;
    fopen_s(&fout,argv[2],"wt");
    if( !fout )
    {
        printf( "Error; Cannot open file %s for writing\n", argv[3] );
        return -1;
    }
    std::vector<std::string> tiebreaks;

    // Read the tiebreaks file
    if( argc == 4 )
    {
        std::string fin_tiebreaks(argv[3]);
        std::ifstream in_tiebreaks(fin_tiebreaks.c_str());
        if( !in_tiebreaks )
        {
            printf( "Error; Cannot open file %s for reading\n", fin_tiebreaks.c_str() );
            return -1;
        }
        bool first=true;
        for(;;)
        {
            std::string line;
            if( !std::getline(in_tiebreaks, line) )
                break;

            // Strip out UTF8 BOM mark (hex value: EF BB BF)
            if( first && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                line = line.substr(3);
            util::rtrim(line);
            tiebreaks.push_back(line);
            first = false;
        }
    }
    std::vector<std::string> lpgn_games_in;
    std::vector<std::string> lpgn_games_out;
    ok = core_pgn2line( fin, lpgn_games_in );
    if( !ok )
        return -1;

    func_gen_crosstable( lpgn_games_in, fout, tiebreaks );
    fclose(fout);
    return 0;
}

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

#define MAX_ROUND 100

struct RESULT
{
    std::string white;
    std::string black;
    int round = 0;
    std::string result;
    int hits=0;
};

struct PLAYER
{
    std::string player;
    std::string elo;
    RESULT results[MAX_ROUND+1];
    int nbr_games = 0;
    int total = 0;
    int place = 0;
    int tiebreak = INT_MIN;
    const bool operator <(const PLAYER &rhs)
    {
        if( total == rhs.total )
            return tiebreak < rhs.tiebreak;
        return total<rhs.total;
    } 
};

std::map<std::string,PLAYER> players;
std::map<std::string,int> placings;
std::map<std::string,int> tiebreaks;

static int func_gen_crosstable( std::vector<std::string> &in, FILE *fout,  std::vector<std::string> &s_tiebreaks )
{
    // Map player -> tiebreak, best tiebreak = -1, worst = INT_MIN
    int line_nbr=1;
    for( std::string &player: s_tiebreaks )
    {
        tiebreaks[player] = 0-line_nbr;
        line_nbr++;
    }

    // Read games
    unsigned int matches=0;
    unsigned int mismatches=0;
    unsigned int misses_white=0, misses_black=0, misses_result=0, misses_round=0;
    unsigned int round_illformed=0, round_toobig=0;
    for( std::string line: in )
    {
        util::rtrim(line);
        auto offset = line.find("@M");
        if( offset == std::string::npos )
            continue;
        std::string header = line.substr(0,offset);
        std::string moves  = line.substr(offset);
        std::string white, black, result, round;
        //fprintf( fout, "Moves: %s\n", moves.c_str() );
        bool ok = key_find( header, "White", white );
        if( ok )
            ; //fprintf( fout, "White: %s\n", white.c_str() );
        else
            misses_white++;
        ok = key_find( header, "Black", black );
        if( ok )
            ;// fprintf( fout, "Black: %s\n", black.c_str() );
        else
            misses_black++;
        ok = key_find( header, "Round", round );
        int iround = 0;
        if( ok )
        {
            // fprintf( fout, "Round: %s\n", round.c_str() );
            iround = atoi(round.c_str());
            if( iround <= 0 )
                round_illformed++;
            if( iround > MAX_ROUND )
                round_toobig++;
        }
        else
            misses_round++;
        ok = key_find( header, "Result", result );
        if( ok )
            ;// fprintf( fout, "Result: [%s]\n", result.c_str() );
        else
            misses_result++;
        const char *p = moves.c_str();
        const char *s = p + moves.length();
        while( s > p )
        {
            if( *s==' ' || *s=='\t' || *s=='\n' || *s=='M' || *s=='}')
            {
                s++;
                break;
            }
            s--;
        }
        // fprintf( fout, "Moves Result: [%s]\n", s );
        if( std::string(s) == result )
            matches++;
        else
            mismatches++;
        RESULT r;
        r.white = white;
        r.black = black;
        r.result = result;
        r.round = iround;
        PLAYER &w = players[white];
        std::string white_elo;
        ok = key_find( header, "WhiteElo", white_elo );
        if( ok )
        {
            if( w.player=="" )
                w.elo=white_elo;
            else
            {
                if( w.elo != white_elo )
                {
                    //printf( "Warning: Player %s has different Elo values (eg %s and %s)\n",
                    //    w.player.c_str(), w.elo.c_str(), white_elo.c_str() );
                }
            }
        }
        w.player = white;
        if( 1<=iround && iround<=MAX_ROUND )
        {
            int hits = w.results[iround].hits + 1;
            w.results[iround] = r;
            w.results[iround].hits = hits;
            if( hits > 1 )
            {
                printf( "Player %s, round %d, hits %d\n", white.c_str(), iround, hits );
            }
        }    
        PLAYER &b = players[black];
        std::string black_elo;
        ok = key_find( header, "BlackElo", black_elo );
        if( ok )
        {
            if( b.player=="" )
                b.elo=black_elo;
            else
            {
                if( b.elo != black_elo )
                {
                    //printf( "Warning: Player %s has different Elo values (eg %s and %s)\n",
                    //    b.player.c_str(), b.elo.c_str(), black_elo.c_str() );
                }
            }
        }
        b.player = black;
        if( 1<=iround && iround<=MAX_ROUND )
        {
            int hits = b.results[iround].hits + 1;
            b.results[iround] = r;
            b.results[iround].hits = hits;
            if( hits > 1 )
            {
                printf( "Player %s, round %d, hits %d\n", black.c_str(), iround, hits );
            }
        }    
    }
    std::vector<PLAYER> vplayers;
    for( auto it = players.begin(); it!=players.end(); it++ )
    {
        std::string name = it->first;
        PLAYER &player = it->second;
        auto it2 = tiebreaks.find(name);
        if( it2 != tiebreaks.end() )
            player.tiebreak = it2->second;
        // fprintf( fout, "%s\n", name.c_str() );
        for( int i=0; i<=MAX_ROUND; i++ )
        {
            RESULT &r = player.results[i];
            if( r.hits == 1)
            {
                player.nbr_games++;
                // fprintf( fout, " Round: %d\n", r.round );
                // fprintf( fout, "  White: %s\n", r.white.c_str() );
                // fprintf( fout, "  Black: %s\n", r.black.c_str() );
                // fprintf( fout, "  Result: %s\n", r.result.c_str() );
                if( r.result == "1-0")
                {
                    if( name == r.white )
                        player.total += 2;
                }
                else if( r.result == "0-1")
                {
                    if( name == r.black )
                        player.total += 2;
                }
                else if( r.result == "1/2-1/2")
                {
                    player.total++;
                }
                else if( r.result != "0-0")
                {
                    printf( "Odd result %s\n", r.result.c_str() );
                }
            }
        }
        vplayers.push_back(player);
    }
    std::sort( vplayers.rbegin(), vplayers.rend() );
    int place=1;
    for( PLAYER &player: vplayers )
    {
        player.place = place;
        placings[player.player] = place;
        place++;
        // fprintf( fout, "%s: games %d, score %.1f\n", player.player.c_str(), player.nbr_games, player.total/2.0 );
    }

    for( PLAYER &player: vplayers )
    {
        std::string name = player.player;
        name = name.substr(0,28);
        fprintf( fout, "%-3d %-28s %4s  %.1f : ", player.place, name.c_str(), player.elo.c_str(), player.total/2.0 );
        for( int round=1; round<=11; round++ )
        {
            RESULT &r = player.results[round];
            std::string name = player.player;
            std::string field = ".";
            if( r.result == "1-0")
            {
                if( name == r.white )
                    field = util::sprintf( "+W%d", placings[r.black] );
                else if( name == r.black )
                    field = util::sprintf( "-B%d", placings[r.white] );
            }
            else if( r.result == "0-1")
            {
                if( name == r.white )
                    field = util::sprintf( "-W%d", placings[r.black] );
                else if( name == r.black )
                    field = util::sprintf( "+B%d", placings[r.white] );
            }
            else if( r.result == "1/2-1/2")
            {
                if( name == r.white )
                    field = util::sprintf( "=W%d", placings[r.black] );
                else if( name == r.black )
                    field = util::sprintf( "=B%d", placings[r.white] );
            }
            else if( r.result == "0-0")
            {
                if( name == r.white )
                    field = util::sprintf( "%s%d", "--", placings[r.black] );
                else if( name == r.black )
                    field = util::sprintf( "%s%d", "--", placings[r.white] );
            }
            fprintf( fout, " %-5s", field.c_str() );
        }
        fprintf(fout,"\n" );
    }

    printf( "Number of games: %u, result mismatches: %u\n", in.size(), mismatches );
    printf( "Number of misses: White %u, Black %u, Result %u, Round %u\n",
        misses_white, misses_black, misses_result, misses_round );
    printf( "Number of wrong rounds: illformed %u, toobig %u\n",
        round_illformed, round_toobig );
    return 0;
}

