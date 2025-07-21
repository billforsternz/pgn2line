// supplementary.cpp : Do something special to LPGN file
//  golden.cpp, the golden age of chess command

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
#include "golden.h"

//
// Golden age of chess, find Lasker, Capablanca, Alekhines customers
//
//   Whoops don't really need this here as we broke it out and developed it further in rivals.cpp and rivals.exe
//

struct MicroGame
{
    std::string white;
    std::string black;
    int result;     // +1, -1 or 0
};

struct PlayerResult
{
    std::string name;   
    int         idx;        // idx in result tables
    int         wins=0;
    int         losses=0;
    int         draws=0;
    int         total=0;
    double      percent;
    std::string summary;   
};

static bool operator < (const PlayerResult &lhs, const PlayerResult &rhs )
{
    return lhs.percent < rhs.percent;
}

int cmd_golden( std::ifstream &in, std::ofstream &out )
{
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
        MicroGame mg;
        auto offset1 = line.find("@H[White \"");
        if( offset1 != std::string::npos )
        {
            auto offset2 = line.find("\"]@H[",offset1+10);
            if( offset2 != std::string::npos )
                mg.white = line.substr(offset1+10,offset2-offset1-10);
        }
        offset1 = line.find("@H[Black \"");
        if( offset1 != std::string::npos )
        {
            auto offset2 = line.find("\"]@H[",offset1+10);
            if( offset2 != std::string::npos )
                mg.black = line.substr(offset1+10,offset2-offset1-10);
        }
        offset1 = line.find("@H[Result \"");
        if( offset1 != std::string::npos )
        {
            auto offset2 = line.find("\"]",offset1+11);
            if( offset2 != std::string::npos )
            {
                std::string result = line.substr(offset1+11,offset2-offset1-11);
                mg.result = 0;
                if( result == "1-0" )
                    mg.result = +1;
                else if( result == "0-1" )
                    mg.result = -1;
            }
        }
        mgs.push_back(mg);
    }
    printf( "Looking through games\n" );
    std::set<std::string> capablanca_opponents;
    std::set<std::string> lasker_opponents;
    std::set<std::string> alekhine_opponents;
    std::vector<std::string> players;
    std::set<std::string> player_set;
    for( MicroGame mg: mgs )
    {
        if( mg.white == "Capablanca, Jose Raul" )
            capablanca_opponents.insert( mg.black );
        if( mg.black == "Capablanca, Jose Raul" )
            capablanca_opponents.insert( mg.white );
        if( mg.white == "Lasker, Emanuel" )
            lasker_opponents.insert( mg.black );
        if( mg.black == "Lasker, Emanuel" )
            lasker_opponents.insert( mg.white );
        if( mg.white == "Alekhine, Alexander" )
            alekhine_opponents.insert( mg.black );
        if( mg.black == "Alekhine, Alexander" )
            alekhine_opponents.insert( mg.white );
    }

    players.push_back( "Capablanca, Jose Raul" );
    players.push_back( "Lasker, Emanuel" );
    players.push_back( "Alekhine, Alexander" );
/*  printf( "** Alekhine's opponents\n" );
    for( std::string player: alekhine_opponents )
    {
        printf( "%s\n", player.c_str() );
    }
    printf( "** Lasker's opponents\n" );
    for( std::string player: lasker_opponents )
    {
        printf( "%s\n", player.c_str() );
    }
    printf( "** Capablanca's opponents\n" ); */
    for( std::string player: capablanca_opponents )
    {
        //printf( "%s\n", player.c_str() );
        //if( player == "Nimzowitsch, Aaron" )
        //    printf( "Debug\n" );
        auto it1 = lasker_opponents.find(player);
        auto it2 = alekhine_opponents.find(player);
        if( it1 != lasker_opponents.end() && it2 != alekhine_opponents.end() )
        {
            if( player != "NN" )
            {
                players.push_back(player);  // Player has played all of Capa + Lasker + Alekhine
            }
        }
    }

    printf( "** The initial set of players established\n" );
    std::map<std::string,int> player_nbr;
    int idx = 0;
    int nbr_players = (int)players.size();
    for( std::string player: players )
    {
        player_set.insert( player ) ;
        player_nbr.insert( std::pair<std::string,int> (player,idx) ) ;
        idx++;
    }
    #define MAX_PLAYERS 100
    if( nbr_players >= MAX_PLAYERS )
    {
        printf( "Sorry %d players exceeds hardwired limit of %d golden age players\n", nbr_players, MAX_PLAYERS );
        exit(-1);
    }

    // Count all games between all our players
    static int table[MAX_PLAYERS][MAX_PLAYERS];
    static int win  [MAX_PLAYERS][MAX_PLAYERS];
    static int lose [MAX_PLAYERS][MAX_PLAYERS];
    static int draw [MAX_PLAYERS][MAX_PLAYERS];
    static bool killed[MAX_PLAYERS];
    for( MicroGame mg: mgs )
    {
        auto it1 = player_set.find(mg.white);
        auto it2 = player_set.find(mg.black);
        if( it1 != player_set.end() && it2 != player_set.end() )
        {
            int white = player_nbr[mg.white];
            int black = player_nbr[mg.black];
            table[white][black]++;    // eg increment Alekhine has played Capablanca
            table[black][white]++;    // and also increment Capablanca has played Alekhine
            if( mg.result == 1 )
            {
                win [white][black]++;
                lose[black][white]++;
            }
            else if( mg.result == -1 )
            {
                lose[white][black]++;
                win [black][white]++;
            }
            else
            {
                draw[white][black]++;
                draw[black][white]++;
            }
        }
    }

    // Eliminate one player at a time, until all remaining players have played all remaining players
    int nbr_eliminated = 0;
    for(;;)
    {
        int min_so_far = MAX_PLAYERS;
        int min_so_far_idx = MAX_PLAYERS;
        int min_so_far_games = 0;
        for( int i=0; i<nbr_players; i++ )
        {
            if( killed[i] )
                continue;
            int nbr_played = 0;
            int nbr_games = 0;
            for( int j=0; j<nbr_players; j++ )
            {
                if( killed[j] )
                    continue;
                bool has_played = table[i][j] > 0;
                if( has_played )
                    nbr_played++;
                nbr_games += table[i][j];
            }
            if( nbr_played<min_so_far )
            {
                min_so_far = nbr_played;
                min_so_far_idx = i;
                min_so_far_games = nbr_games;
            }
            else if( nbr_played==min_so_far && nbr_games!=min_so_far_games )
            {
                if( nbr_games < min_so_far_games )
                {
                    min_so_far = nbr_played;
                    min_so_far_idx = i;
                    min_so_far_games = nbr_games;
                }
            }
        }

        // When all players have played all but one remaining player (themselves), stop
        if( min_so_far == nbr_players-nbr_eliminated-1 )    
        {
            printf( "Success! All remaining players have played all remaining players\n" );
            break;
        }
        else
        {
            for( int i=0; i<nbr_players; i++ )
            {
                if( killed[i] )
                    continue;
                int nbr_played = 0;
                int nbr_games = 0;
                for( int j=0; j<nbr_players; j++ )
                {
                    if( killed[j] )
                        continue;
                    bool has_played = table[i][j] > 0;
                    if( has_played )
                        nbr_played++;
                    nbr_games += table[i][j];
                }
                if( nbr_played==min_so_far && i!=min_so_far_idx )
                {
                    printf( "Elimination tie break required: %s (%d games) survives, %s (%d games) loses\n",
                        players[i].c_str(), nbr_games, players[min_so_far_idx].c_str(), min_so_far_games );
                }
            }
            printf( "Eliminating %s who has played only %d of %d players\n",
                players[min_so_far_idx].c_str(), min_so_far, nbr_players-nbr_eliminated );
            killed[min_so_far_idx] = true;
            player_set.erase(players[min_so_far_idx]);
            nbr_eliminated++;
        }
    }

    printf( "** The final set of %d players established\n", nbr_players-nbr_eliminated );
    std::vector<PlayerResult> prs;
    for( int i=0; i<nbr_players; i++ )
    {
        if( killed[i] )
            continue;
        PlayerResult pr;
        pr.name = players[i];
        pr.idx  = i;
        for( int j=0; j<nbr_players; j++ )
        {
            if( killed[j] )
                continue;
            pr.wins += win[i][j];
            pr.losses += lose[i][j];
            pr.draws += draw[i][j];
            pr.total += table[i][j];
            if( win[i][j] + lose[i][j] + draw[i][j] != table[i][j] )
                printf( "Check 1 failed !?\n" );
        }
        if( pr.wins + pr.losses + pr.draws != pr.total )
            printf( "Check 2 failed !?\n" );
        pr.percent = 100.0 * (2.0*pr.wins + 1.0*pr.draws) / (2.0 * pr.total);
        prs.push_back(pr);
    }
    std::sort( prs.rbegin(), prs.rend() );
    for( PlayerResult &pr: prs )
    {
        int score = pr.wins + pr.draws/2;
        bool plus_half = ((pr.draws&1) != 0);
        char buf[100];
        sprintf_s<100>( buf, "%.1f%%  +%-3d -%-3d =%-3d  %d%s/%d",
            pr.percent,
            pr.wins,
            pr.losses,
            pr.draws,
            score,
            plus_half?".5":"",
            pr.total
        );
        printf( "%-28s %s\n", pr.name.c_str(), buf );
        pr.summary = std::string(buf);
    }

    printf( "<table border=\"1\" cellpadding=\"25\" cellspacing=\"25\">\n" );
    printf( "<tr><th>Player</th><th>Summary</th>" );
    for( int i=0; i<nbr_players-nbr_eliminated; i++ )
        printf( "<th>%d</th>", i+1 );
    printf( "</tr>\n" );
    int i=0;
    for( PlayerResult &pr: prs )
    {
        printf( "<tr>" );
        printf( "<td>%2d %s</td>", i+1, pr.name.c_str() );
        printf( "<td>%s</td>", pr.summary.c_str() );
        int j=0;
        for( PlayerResult &opp: prs )
        {
            if( i == j )
                printf( "<td></td>" );
            else
            {
                int wins   = win  [pr.idx][opp.idx];
                int losses = lose [pr.idx][opp.idx];
                int draws  = draw [pr.idx][opp.idx];
                printf( "<td>+%d -%d =%d</td>", wins, losses, draws );
            }
            j++;
        }
        i++;
        printf( "</tr>\n" );
    }
    printf( "</table>\n" );
    printf( "Write out all games between our heroes\n" );
    in.clear();
    in.seekg(0);
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
        MicroGame mg;
        auto offset1 = line.find("@H[White \"");
        if( offset1 != std::string::npos )
        {
            auto offset2 = line.find("\"]@H[",offset1+10);
            if( offset2 != std::string::npos )
                mg.white = line.substr(offset1+10,offset2-offset1-10);
        }
        offset1 = line.find("@H[Black \"");
        if( offset1 != std::string::npos )
        {
            auto offset2 = line.find("\"]@H[",offset1+10);
            if( offset2 != std::string::npos )
                mg.black = line.substr(offset1+10,offset2-offset1-10);
        }
        auto it1 = player_set.find(mg.white);
        auto it2 = player_set.find(mg.black);
        if( it1 != player_set.end() && it2 != player_set.end() )
            util::putline(out,line);
    }
    return 0;
}
