//
//  rivals.cpp
//  ==========
//
//                                          Bill Forster 3rd May 2022
// 

const char *usage =

"For a small set of \"Seed players\" (eg Alekhine, Capablanca, Lasker) find\n"
"all players who've played all seed players. We start with this group of\n"
"relevant players (plus the seed players themselves).\n"
"\n"
"Now eliminate the player who's played the smallest set of other players in\n"
"the group. Then recalculate for the new smaller group, and eliminate another\n"
"player.\n"
"\n"
"Repeat until we have a group who've all played each other. The idea is that\n"
"this reduced group will be the key rivals and contemporaries of the seed\n"
"players.\n"
"\n"
"Usage: rivals in out seeds...\n"
"Eg:    rivals in.lpgn out.lpgn \"Capablanca, Jose Raul\" \"Lasker, Emanuel\"\n"
"\n"
"Please refer to https://github.com/billforsternz/pgn2line for details of\n"
"the .lpgn format. It is essential to use a high quality input database with\n"
"good consistent player names in particular.\n";

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

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

bool operator < (const PlayerResult &lhs, const PlayerResult &rhs )
{
    return lhs.percent < rhs.percent;
}

static void read_players_and_result( MicroGame &mg, const std::string &line );

int main( int argc, const char **argv )
{
#if 0
    const char *args[] =
    {
        "C:/Users/Bill/Documents/Github/pgn2line/release/rivals.exe",
        "C:/Users/Bill/Documents/ChessDatabase/golden/a.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/golden/b-out.lpgn",
        "Capablanca, Jose Raul",
        "Lasker, Emanuel",
        "Alekhine, Alexander"
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args;
#endif

    //
    //  Parse command line
    //
    bool ok = (argc>3);
    if( !ok )
    {
        printf( "%s", usage );
        return -1;
    }
    std::string fin(argv[1]);
    std::string fout(argv[2]);
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return -1;
    }
    std::ofstream out( fout.c_str() );
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return -1;
    }
    std::vector<std::string> seeds;
    printf( "Seed players are;\n" );
    for( int i=3; i<argc; i++ )
    {
        printf( " %s\n", argv[i] );
        seeds.push_back(argv[i]);
    }

    //  
    //  Read the whole file and create players plus result "MicroGame" for all games
    //

    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
        MicroGame mg;
        read_players_and_result( mg, line );
        mgs.push_back(mg);
    }

    //  
    //  Find all opponents in games played by our seed players
    //

    printf( "Looking through games\n" );
    std::vector <std::set<std::string>> seed_opponents;
    std::vector<std::string> players;
    std::set<std::string> player_set;
    for( std::string seed: seeds )
    {
        players.push_back( seed );
        std::set<std::string> opponents;
        for( MicroGame mg: mgs )
        {
            if( mg.white == seed )
                opponents.insert( mg.black );
            if( mg.black == seed )
                opponents.insert( mg.white );
        }
        seed_opponents.push_back(opponents);
        printf( "Seed player %s; %u opponents found\n", seed.c_str(), opponents.size() );
    }

    //
    //  Seed players are already on the players list. Now add
    //   all opponents who've played all of the seed players
    //

    for( std::string player: seed_opponents[0] )
    {

        // This player has played the first seed, has he played
        //  all the others?
        bool keep_player = true;
        for( unsigned int i=1; i<seed_opponents.size(); i++ )
        {
            auto it = seed_opponents[i].find(player);
            if( it == seed_opponents[i].end() )
                keep_player = false;
        }
        if( keep_player && player!="NN" )
            players.push_back(player);  // player has played all seeds    }
    }

    //  
    //  The starting set of relevant players has now been established
    //

    printf( "** The initial set of players established\n" );
    std::map<std::string,int> player_nbr;
    int idx = 0;
    int nbr_players = players.size();
    for( std::string player: players )
    {
        player_set.insert( player ) ;
        player_nbr.insert( std::pair<std::string,int> (player,idx) ) ;
        idx++;
    }

    //  
    //  Count all games between all our players
    //

    #define MAX_PLAYERS 300     // Sorry about the old school 2 dimensional array (pragmatism)
    if( nbr_players >= MAX_PLAYERS )
    {
        printf( "Sorry %d players exceeds hardwired limit of %d players\n", nbr_players, MAX_PLAYERS );
        exit(-1);
    }
    static int total[MAX_PLAYERS][MAX_PLAYERS];
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
            total[white][black]++;    // eg increment Alekhine has played Capablanca
            total[black][white]++;    // and also increment Capablanca has played Alekhine
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

    //
    //  Eliminate one player at a time, until all remaining players have played all remaining players
    //

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
                bool has_played = total[i][j] > 0;
                if( has_played )
                    nbr_played++;
                nbr_games += total[i][j];
            }
            if( nbr_played < min_so_far )
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
        else if( min_so_far_idx < MAX_PLAYERS )
        {

            // Print some tie-break info to make it easier or at least possible to sense
            //  injustices
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
                    bool has_played = total[i][j] > 0;
                    if( has_played )
                        nbr_played++;
                    nbr_games += total[i][j];
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

    //
    //  Calculate players win/lose/draw and percent score
    //

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
            pr.wins   += win[i][j];
            pr.losses += lose[i][j];
            pr.draws  += draw[i][j];
            pr.total  += total[i][j];
            if( win[i][j] + lose[i][j] + draw[i][j] != total[i][j] )
                printf( "Check 1 failed !?\n" );
        }
        if( pr.wins + pr.losses + pr.draws != pr.total )
            printf( "Check 2 failed !?\n" );
        pr.percent = 100.0 * (2.0*pr.wins + 1.0*pr.draws) / (2.0 * pr.total);
        prs.push_back(pr);
    }

    //
    //  Print out a sorted summary
    //

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

    //
    //  Print out a cross-table for html / markdown
    //

    printf( "This table might be useful to paste into an html or markdown document;\n" );
    printf( "<table border=\"1\" cellpadding=\"25\" cellspacing=\"25\">\n" );
    printf( "<tr><th>Player</th><th>Total</th>" );
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

    //
    //  Write out all games between players in the final group
    //

    printf( "Write out all games between our heroes\n" );
    in.clear();
    in.seekg(0);
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
        MicroGame mg;
        read_players_and_result( mg, line );
        auto it1 = player_set.find(mg.white);
        auto it2 = player_set.find(mg.black);
        if( it1 != player_set.end() && it2 != player_set.end() )
        {
            out.write( line.c_str(), line.length() );
            out.write( "\n", 1 );
        }
    }
    return 0;
}


static void read_players_and_result( MicroGame &mg, const std::string &line )
{
    for( int i=0; i<3; i++ )
    {
        std::string s, t;
        switch(i)
        {
            case 0: s = "White";    break;
            case 1: s = "Black";    break;
            case 2: s = "Result";   break;
        }
        s = "@H[" + s + " \"";
        auto offset1 = line.find(s);
        if( offset1 != std::string::npos )
        {
            offset1 += s.length();
            auto offset2 = line.find("\"",offset1);
            if( offset2 != std::string::npos )
                t = line.substr(offset1,offset2-offset1);
        }
        switch(i)
        {
            case 0: mg.white = t;   break;
            case 1: mg.black = t;   break;
            case 2:
            {
                mg.result = 0;
                if( t == "1-0" )
                    mg.result = +1;
                else if( t == "0-1" )
                    mg.result = -1;
            }
        }
    }
}
