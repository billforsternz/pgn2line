// supplementary.cpp : Do something special to LPGN file
//


// To download games eg https://lichess.org/api/tournament/PmNmfo9m/games

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

static int pluck( std::ifstream &in1, std::ifstream &in2, std::ofstream &out, bool reorder );
static int func_add_ratings( std::ifstream &in1, std::ifstream &in2, std::ofstream &out );
static void get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt );
static void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out );
static int do_refine_dups( std::ifstream &in, std::ofstream &out );
static int do_remove_exact_pairs( std::ifstream &in, std::ofstream &out );
static void lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal );
bool key_find( const std::string header, const std::string key, std::string &val );
bool key_replace( std::string &header, const std::string key, const std::string val );
bool key_delete( std::string &header, const std::string key );
void key_add( std::string &header, const std::string key, const std::string val );
void key_update( std::string &header, const std::string key, const std::string key_insert_after, const std::string val );

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

int main( int argc, const char **argv )
{
#ifdef _DEBUG
    const char *args[] =
    {
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/supp.exe",
        "r",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/south-rapid.txt",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/south-rapid.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/out/south-rapid.lpgn",
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args;
/*    const char *args[] =
    {
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/supp.exe",
        "5",
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/twic-unsorted.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/dups-in-unsorted.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/dups-out-unsorted.lpgn"
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args; */
#endif


    enum {time,event,teams,hardwired,pluck_games,pluck_games_reorder,refine_dups,remove_exact_pairs,golden,add_ratings} purpose=time;
    bool ok = (argc>2);
    if( ok )
    {
        std::string s(argv[1]);
        if( s == "g" )
        {
            ok = (argc==4);
            purpose = golden;
        }
        else if( s == "1" )
        {
            ok = (argc==4);
            purpose = time;
        }
        else if( s == "2" )
        {
            ok = (argc==5);
            purpose = event;
        }
        else if( s == "3" )
        {
            ok = (argc==6);
            purpose = teams;
        }
        else if( s == "4" )
        {
            ok = (argc==4);
            purpose = hardwired;
        }
        else if( s == "5" )
        {
            ok = (argc==4);
            purpose = refine_dups;
        }
        else if( s == "6" )
        {
            ok = (argc==5);
            purpose = pluck_games;
        }
        else if( s == "7" )
        {
            ok = (argc==5);
            purpose = pluck_games_reorder;
        }
        else if( s == "8" )
        {
            ok = (argc==4);
            purpose = remove_exact_pairs;
        }
        else if( s == "r" )
        {
            ok = (argc==5);
            purpose = add_ratings;
        }
    }
    if( !ok )
    {
        printf( 
            "Usage:\n"
            "r ratings.txt in.lpgn out.lpgn         ;Fix Lichess names and add ratings\n"
            "g in.lpgn out.lpgn                     ;golden age of chess program\n"
            "1 in.lpgn out.lpgn                     ;adds 'Lost on time' comment\n"
            "2 in.lpgn out.lpgn replacement-event\n"
            "3 in.lpgn out.lpgn name-tsv teams-csv\n"
            "4 in.lpgn out.lpgn                     ;hardwired Elo fix\n"
            "5 in.lpgn out.lpgn                     ;refine candidate dup pairs\n"
            "6 bulk.lpgn in.lpgn out.lpgn           ;replace games in input with close matches from bulk\n"
            "7 bulk.lpgn in.lpgn out.lpgn           ;like 6, but assume pairs and re-order based on bulk location\n"
            "8 in.lpgn out.lpgn                     ;remove exact dup pairs\n"
            "Notes: for 1 adds 'Lost on time' comment if \"Time forfeit\" Termination field in Lichess LPGN\n"
        );
        return -1;
    }
    std::string fin(argv[2]);
    std::string fout(argv[(purpose==pluck_games||purpose==pluck_games_reorder||purpose==add_ratings)?4:3]);
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
    std::string replace_event;
    std::map<std::string,std::string> elo_lookup;
    std::map<std::string,std::string> handle_team;
    std::map<std::string,std::string> handle_real_name;
    if( purpose == refine_dups )
        return do_refine_dups( in, out );
    if( purpose == remove_exact_pairs )
        return do_remove_exact_pairs( in, out );
    else if( purpose == pluck_games ||  purpose == pluck_games_reorder )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return pluck( in, in2,  out, purpose==pluck_games_reorder );
    }
    else if( purpose == add_ratings )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return func_add_ratings( in, in2,  out );
    }
    else if( purpose == event )
        replace_event = argv[4];
    else if( purpose == hardwired )
    {
        elo_lookup["Gong, Daniel"] = "2296";
        elo_lookup["Fan, Allen"] = "2125";
        elo_lookup["Winter, Ryan"] = "1958";
        elo_lookup["Qin, Oscar"] = "1876";
        elo_lookup["Sole, Michael"] = "1864";
        elo_lookup["Park-Tamati, Philli"] = "1761";
        elo_lookup["Vickers, Josia"] = "1730";
        elo_lookup["McDougall, Euan"] = "1655";
        elo_lookup["Loke, Kayden"] = "1603";
        elo_lookup["Mudaliar, Rohit"] = "1539";
        elo_lookup["Ning, Isabelle"] = "1479";
        elo_lookup["Kichavadi, Tejasvi"] = "1432";
        elo_lookup["Mao, Daqi"] = "1394";
        elo_lookup["Al-Afaghani, Baraa"] = "1388";
        elo_lookup["Joseph, Ritika"] = "1297";
        elo_lookup["Gan, Emily"] = "998";
        elo_lookup["Mudaliar, Tarun"] = "781";
        elo_lookup["Wang, Max"] = "0";
    }
    else if( purpose == teams )
    {
        std::string fin_team(argv[5]);
        std::ifstream in_team(fin_team.c_str());
        for(;;)
        {
            std::string line;
            if( !std::getline(in_team, line) )
                break;
            std::string team_name;
            std::string handle;
            util::ltrim(line);
            util::rtrim(line);
            size_t offset = line.find(',');
            if( offset != std::string::npos )
            {
                handle = line.substr(0,offset);
                team_name = line.substr(offset+1);
                util::ltrim(handle);
                util::rtrim(handle);
                util::ltrim(team_name);
                util::rtrim(team_name);
                if( handle.length()>0 && team_name.length()>0 )
                    handle_team[handle] = team_name;
            }
        }
        std::string fin_pgn_name(argv[4]);
        std::ifstream in_pgn_name(fin_pgn_name.c_str());
        for(;;)
        {
            std::string line;
            if( !std::getline(in_pgn_name, line) )
                break;
            std::string pgn_name;
            std::string handle;
            util::ltrim(line);
            util::rtrim(line);
            size_t offset = line.find('\t');
            if( offset != std::string::npos )
            {
                handle = line.substr(0,offset);
                pgn_name = line.substr(offset+1);
                util::ltrim(handle);
                util::rtrim(handle);
                util::ltrim(pgn_name);
                util::rtrim(pgn_name);
                if( handle.length()>0 && pgn_name.length()>0 )
                {
                    auto offset = pgn_name.find(", ");
                    if( offset != std::string::npos )
                    {
                        std::string real_name = pgn_name.substr(offset+2) + " " + pgn_name.substr(0,offset);
                        handle_real_name[handle] = real_name;
                    }
                }
            }
        }
    }
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
        switch(purpose)
        {
            case teams:
            {
                std::string white, black;
                auto offset1 = line.find("@H[White \"");
                if( offset1 != std::string::npos )
                {
                    auto offset2 = line.find("\"]@H[",offset1+10);
                    if( offset2 != std::string::npos )
                        white = line.substr(offset1+10,offset2-offset1-10);
                }
                offset1 = line.find("@H[Black \"");
                if( offset1 != std::string::npos )
                {
                    auto offset2 = line.find("\"]@H[",offset1+10);
                    if( offset2 != std::string::npos )
                        black = line.substr(offset1+10,offset2-offset1-10);
                }
                bool have_teams=true, have_real_names=true;
                auto it_white = handle_real_name.find(white);
                if( it_white == handle_real_name.end() )
                {
                    printf( "Unknown handle: %s\n", white.c_str() );
                    have_real_names = false;
                }
                auto it_black = handle_real_name.find(black);
                if( it_black == handle_real_name.end() )
                {
                    printf( "Unknown handle: %s\n", black.c_str() );
                    have_real_names = false;
                }
                auto it_white_team = handle_team.find(white);
                if( it_white_team == handle_team.end() )
                {
                    printf( "Unknown team:   %s\n", white.c_str() );
                    have_teams = false;
                }
                auto it_black_team = handle_team.find(black);
                if( it_black_team == handle_team.end() )
                {
                    printf( "Unknown team:   %s\n", black.c_str() );
                    have_teams = false;
                }
                if( have_teams && it_white_team->second == it_black_team->second )
                {
                    printf( "Check teams: %s (%s) plays %s (%s)\n", white.c_str(), it_white_team->second.c_str(), black.c_str(), it_black_team->second.c_str() );
                }
                auto offset = line.find("]@M");
                if( offset != std::string::npos )
                {
                    std::string begin = line.substr(0,offset+1);   // up to @M
                    std::string end   = line.substr(offset+3);     // after @M
                    std::string s = "@H[WhiteHandle \"";
                    s += white;
                    s += "\"]@H[BlackHandle \"";
                    s += black;
                    if( have_teams )
                    {
                        s += "\"]@H[WhiteTeam \"";
                        s += it_white_team->second;
                        s += "\"]@H[BlackTeam \"";
                        s += it_black_team->second;
                    }
                    s += "\"]@M";
                    std::string comment;
                    if( have_real_names && have_teams )
                    {
                        comment = util::sprintf( "{%s \"%s\", %s vs %s \"%s\", %s} ",
                            it_white->second.c_str(), white.c_str(), it_white_team->second.c_str(),
                            it_black->second.c_str(), black.c_str(), it_black_team->second.c_str() );
                    }
                    else if( have_real_names )
                    {
                        comment = util::sprintf( "{%s \"%s\" vs %s \"%s\"} ",
                            it_white->second.c_str(), white.c_str(),
                            it_black->second.c_str(), black.c_str() );
                    }
                    line = begin + s + comment + end;
                }
                break;
            }
            case event:
            {
                auto offset1 = line.find("@H[Event \"");
                if( offset1 != std::string::npos )
                {
                    auto offset2 = line.find("\"]@H[",offset1+10);
                    if( offset2 != std::string::npos )
                    {
                        std::string start = line.substr(0,offset1);
                        std::string end   = line.substr(offset2);
                        line = start;
                        line += "@H[Event \"";
                        line += replace_event;
                        line += end;
                    }
                }
                break;
            }
            case hardwired:
            {
                auto offset1 = line.find("@H[White \"");
                if( offset1 != std::string::npos )
                {
                    auto offset2 = line.find("\"]@H[",offset1+10);
                    if( offset2 != std::string::npos )
                    {
                        std::string name  = line.substr(offset1+10,offset2-offset1-10);
                        if( elo_lookup.find(name) == elo_lookup.end() )
                            printf( "Error: Could not find name: %s\n", name.c_str() );
                        else
                        {
                            std::string new_elo = elo_lookup[name];
                            auto offset3 = line.find("@H[WhiteElo \"");
                            if( offset3 != std::string::npos )
                            {
                                auto offset4 = line.find("\"]@H[",offset3+13);
                                if( offset4 != std::string::npos )
                                {
                                    std::string start  = line.substr(0,offset3);
                                    std::string end    = line.substr(offset4);
                                    line = start;
                                    line += "@H[WhiteElo \"";
                                    line += new_elo;
                                    line += end;
                                }
                            }
                        }
                    }
                }
                offset1 = line.find("@H[Black \"");
                if( offset1 != std::string::npos )
                {
                    auto offset2 = line.find("\"]@H[",offset1+10);
                    if( offset2 != std::string::npos )
                    {
                        std::string name  = line.substr(offset1+10,offset2-offset1-10);
                        if( elo_lookup.find(name) == elo_lookup.end() )
                            printf( "Error: Could not find name: %s\n", name.c_str() );
                        else
                        {
                            std::string new_elo = elo_lookup[name];
                            auto offset3 = line.find("@H[BlackElo \"");
                            if( offset3 != std::string::npos )
                            {
                                auto offset4 = line.find("\"]@H[",offset3+13);
                                if( offset4 != std::string::npos )
                                {
                                    std::string start  = line.substr(0,offset3);
                                    std::string end    = line.substr(offset4);
                                    line = start;
                                    line += "@H[BlackElo \"";
                                    line += new_elo;
                                    line += end;
                                }
                            }
                        }
                    }
                }
                break;
            }
            case time:
            {
                if( std::string::npos!=line.find("@H[Termination \"Time forfeit\"]") )
                {
                    unsigned int len = line.length();
                    if( util::suffix(line,"1-0") )
                    {
                        line = line.substr(0,len-3);
                        line += "{White won on time} 1-0";
                    }
                    else if( util::suffix(line,"0-1") )
                    {
                        line = line.substr(0,len-3);
                        line += "{Black won on time} 0-1";
                    }
                }
            }
            case golden:
            {
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
                break;
            }
        }
        if( purpose != golden )
            util::putline(out,line);
    }
    if( purpose == golden )
    {
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
        int nbr_players = players.size();
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
    }
    return 0;
}

static void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out )
{
    thc::ChessRules cr;
    out.clear();
    for( std::string s: in )
    {
        thc::Move m;
        m.NaturalInFast( &cr, s.c_str() );
        out.push_back(m);
    }
}

static void lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal )
{
    std::vector<std::string> main_line;
    std::string moves_txt;
    get_main_line( moves, main_line, moves_txt );

    // Step 1 add move counts and result
    std::string step1;
    bool white = true;
    int  normal_cnt = 0;
    for( char c: moves_txt )
    {
        if( c == ' ' )
        {
            if( white )
            {
                if( normal_cnt > 0 )
                    step1 += ' ';
                normal_cnt++;
                step1 += util::sprintf( "%d.", normal_cnt );
            }
            white = !white;
        }
        step1 += c;
    }
    std::string val;
    bool ok = key_find(header,"Result",val);
    if( !ok )
        printf( "No result(??): %s\n", step1.c_str() );
    else
    {
        step1 += ' ';
        step1 += val;
    }
    step1 += '\n';

    // Step 2, wrap to 79 columns max
    int idx=0, col=0;
    int last_space_idx=0;
    std::string step2;
    for( char c: step1 )
    {
        if( c==' ' || c=='\n' )
        {
            if( col < 79 ) // it's okay to have 79 or less characters in a line
                ;
            else
            {
                step2[last_space_idx] = '\n';
                col = idx - last_space_idx;
            }
            last_space_idx = idx;
            if( c == '\n' )
                break;
        }
        step2 += c;
        idx++;
        col++;
    }

    // Step 3, Add @M new line move headers
    normal = "@M";
    for( char c: step2 )
    {
        if( c == '\n' )
            normal += "@M";
        else
            normal += c;
    }

}

static void get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt )
{
    main_line.clear();
    moves_txt.clear();
	size_t offset = s.find("@M");
	if( offset == std::string::npos )
        return;
    offset += 2;
    int nest_depth = 0;
    std::string move;
    size_t len = s.length();
    enum {in_main_line,in_move,in_variation,in_comment} state=in_main_line, old_state=in_main_line, save_state=in_main_line;
    while( offset < len )
    {
        char c = s[offset++];
        old_state = state;
        switch( state )
        {
            case in_main_line:
            {
                if( c == '{' )
                    state = in_comment;     // comments don't nest
                else if( c == '(' )
                {
                    state = in_variation;
                    nest_depth = 1;        // variations do nest, so need nest depth
                }
                else if ( c == 'M' )
                    ;  // @M shouldn't change state
                else if( isascii(c) && isalpha(c) )
                {
                    state = in_move;
                    move = c;
                }
                break;
            }
            case in_move:
            {
                if( c == '{' )
                    state = in_comment;
                else if( c == '(' )
                {
                    state = in_variation;
                    nest_depth = 1;
                }
                else if( !isascii(c) )
                    state = in_main_line;
                else if( c=='+' || c=='#' ) // Qg7#
                {
                    move += c;
                    state = in_main_line;
                }
                else if( isalpha(c) || isdigit(c) ||
                          c=='-' || c=='=' ) // O-O, e8=Q
                    move += c;
                else
                    state = in_main_line;
                break;
            }
            case in_variation:
            {
                if( c == '{' )
                    state = in_comment;
                else if( c == '(' )
                    nest_depth++;
                else if( c == ')' )
                {
                    nest_depth--;
                    if( nest_depth == 0 )
                        state = in_main_line;
                }
                // no need to parse moves in this state, wait until
                //  we return to in_main_line state
                break;
            }
            case in_comment:
            {
                if( c == '}' )
                    state = save_state;
                break;
            }
        }
        if( state != old_state )
        {
            if( state == in_comment )
                save_state = old_state;
            else if( old_state == in_move )
            {
                main_line.push_back(move);
                moves_txt += " ";
                moves_txt += move;
            }
        }
    }
}

struct GAME
{
    int line_nbr;
    bool replaced;
    std::string moves_txt;
    std::vector<thc::Move> moves;
};

struct PLAYER
{
    std::string name;
    std::string surname;
    std::string forename;
    std::vector<std::string> forenames;
    std::string rating;
};

bool find_player( std::string name, const std::vector<PLAYER> &ratings, PLAYER &rating )
{
    //if( name == "Hasnula Babaranda" )
    //    printf( "Debug!\n" );
    bool found = false;
    //printf( "in: %s\n", name.c_str() );
    util::rtrim(name);
    auto offset = name.find_last_of(' ');
    if( offset != std::string::npos && offset<name.length() )
    {
        std::string surname = name.substr( offset+1 );
        std::string forename = name.substr( 0, offset );
        util::rtrim(forename);
        //printf( "surname: %s, forename: %s\n", surname.c_str(), forename.c_str() );
        int count = 0;
        for( PLAYER r: ratings )
        {
            if( r.surname == surname )
            {
                rating = r;
                found = true;
                count++;
            }
        }
        if( count == 1 )
            ; //printf( "Ideal: %s\n", rating.name.c_str() );
        else if( count == 0 )
        {
            std::string space_surname = std::string(" ") + surname;
            bool match=false;
            for( PLAYER r: ratings )
            {
                auto offset = r.name.find( space_surname );
                match = (offset != std::string::npos);
                if( match )
                {
                    rating = r;
                    found = true;
                    break;
                }
            }
            if( found )
                ; //printf( "Fallback: %s\n", rating.name..c_str() );
            else
            {
                printf( "in: %s\n", name.c_str() );
                printf( "surname: %s, forename: %s\n", surname.c_str(), forename.c_str() );
                printf( "***** Not found!\n");
            }
        }
        else
        {
            found = false;
            std::vector<std::string> forenames;
            util::split( forename, forenames );
            if( forenames.size() > 0 && forenames[0].length() )
            {
                count = 0;
                for( PLAYER r: ratings )
                {
                    if( r.surname == surname
                        && r.forenames.size() > 0 && r.forenames[0].length() )
                    {
                        if( r.forenames[0] == forenames[0] )
                        {
                            count++;
                            rating = r;
                        }
                    }
                }
                if( count == 1 )
                    found = true;
                else
                {
                    count = 0;
                    for( PLAYER r: ratings )
                    {
                        if( r.surname == surname
                            && r.forenames.size() > 0 && r.forenames[0].length() )
                        {
                            if( r.forenames[0][0] == forenames[0][0] )
                            {
                                if( count == 0 )
                                    rating = r;
                                count++;
                            }
                        }
                    }
                    if( count > 0 )
                        found = true;
                    if( count > 1 )
                    {
                        printf( "in: %s\n", name.c_str() );
                        printf( "surname: %s, forename: %s\n", surname.c_str(), forename.c_str() );
                        printf( "***** Warning, multiple name initials matches, picking first!\n");
                    }
                }
            }
            if( !found )
            {
                printf( "in: %s\n", name.c_str() );
                printf( "surname: %s, forename: %s\n", surname.c_str(), forename.c_str() );
                printf( "***** Not found!\n");
            }
        }
    }
    return found;
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

static int func_add_ratings( std::ifstream &in1, std::ifstream &in2, std::ofstream &out )
{
    // Read the input and fix it
    std::string line;
    if( !std::getline(in1, line) )
        return(-1);

    // Strip out UTF8 BOM mark (hex value: EF BB BF)
    if( line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        line = line.substr(3);
    std::string event_val = line;
    if( !std::getline(in1, line) )
        return(-1);
    std::string site_val = line;

    // Read the ratings
    std::vector<PLAYER> ratings;
    for(;;)
    {
        std::string line;
        if( !std::getline(in1, line) )
            break;
        util::rtrim(line);
        size_t offset = line.find( ',' );
        if( offset != std::string::npos )
        {
            PLAYER r;
            r.surname = line.substr(0,offset);
            size_t offset2 = line.find_first_of( "0123456789", offset );
            if( offset2 != std::string::npos )
            {
                r.name   = line.substr(0,offset2);
                util::rtrim(r.name);
                r.rating = line.substr(offset2);
                r.forename = line.substr(offset+1, offset2-(offset+1) );
                util::ltrim(r.forename);
                util::rtrim(r.forename);
                util::split( r.forename, r.forenames );
                // printf( "%s %s: %s\n", r.forename.c_str(), r.surname.c_str(), r.rating.c_str() );
                ratings.push_back(r);
            }        
        }
    }

    // Fix line by line
    int line_nbr=0;
    for(;;)
    {
        if( !std::getline(in2, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        util::rtrim(line);
        auto offset = line.find("@M");
        if( offset == std::string::npos )
            continue;
        std::string header = line.substr(0,offset);
        std::string moves  = line.substr(offset);
        std::string date;
        bool ok = key_find( header, "UTCDate", date );
        if( ok )
            key_update(header,"Date","Site",date);
        std::string event_original;
        ok = key_find( header, "Event", event_original );
        if( ok )
        {
            std::vector<std::string> fields;
            util::split(event_original,fields);
            if( fields.size() >=2 && (fields[0]=="Round"||fields[0]=="round") )
            {
                int round = atoi( fields[1].c_str() );
                if( round > 0 )
                    key_update(header,"Round","Date",util::sprintf("%d",round));
            }
            else if( fields.size() >=2 && fields[1].length()>=3 &&
                fields[1][0]=='R' &&
                isdigit(fields[1][1]) &&
                !isdigit(fields[1][2])  )
            {
                int round = fields[1][1]-'0';
                if( round > 0 )
                    key_update(header,"Round","Date",util::sprintf("%d",round));
            }
        }
        key_replace( header, "Event", event_val );
        key_replace( header, "Site",  site_val );
        key_delete( header, "UTCDate");
        key_delete( header, "UTCTime");
        key_delete( header, "Annotator");
        key_delete( header, "Variant");
        key_delete( header, "WhiteTitle");
        key_delete( header, "BlackTitle");
        std::string name;
        std::string colour     = "White";
        std::string colour_elo = "WhiteElo";
        for( int i=0; i<2; i++ )
        {
            bool ok = key_find( header, colour, name );
            PLAYER r;
            ok = ok && find_player(name,ratings,r);
            ok = ok && key_replace( header, colour, r.name );
            if( ok )
                key_update(header,colour_elo,"",r.rating);
            colour     = "Black";
            colour_elo = "BlackElo";
        }
        std::string normal;
        lichess_moves_to_normal_pgn( header, moves, normal );
        util::putline(out,header+normal);
    }
    return 0;
}

//#define TRIGGER "2014-06-29 9th Wroclaw Open 2014, Wroclaw POL # 2014-06-29 001.026 Dzikowski-Dadello"
static int pluck( std::ifstream &in1, std::ifstream &in2, std::ofstream &out, bool reorder )
{

    // Read the input
    std::map<std::string,std::vector<GAME>> input;
    std::vector<std::string> vin;
    std::vector< std::pair<double,std::string> > vout;  // bulk line_nbr and line
    unsigned long line_nbr=0;
    bool utf8_bom = false;
    printf( "Reading input\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in2, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
            utf8_bom = true;
        }

        vin.push_back(line);
        std::pair<double,std::string> pr(0,"");
        vout.push_back(pr);
        size_t offset = line.find("@H");
        if( offset != std::string::npos )
        {
            std::string prefix = line.substr(0,offset);
            GAME g;
            g.replaced = false;
            g.line_nbr = line_nbr;
            std::vector<std::string> main_line;
            get_main_line( line, main_line, g.moves_txt );
            convert_moves( main_line, g.moves );
            auto it = input.find(prefix);
            bool trigger = false;
#ifdef TRIGGER
            trigger = (prefix==TRIGGER);
#endif
            if( trigger )
                printf( "Triggered %lu: (%s) reading input: %s\n", line_nbr+1, it==input.end()?"not found":"found", g.moves_txt.c_str() );
            if( it != input.end() )
                it->second.push_back(g);
            else
            {
                std::vector<GAME> v;
                v.push_back(g);
                input[prefix] = v;
            }
        }
        line_nbr++;
    }

    // Read the bulk file searching for lines to be plucked
    printf( "Reading bulk input seeking matches to %lu lines\n", line_nbr );
    line_nbr=0;
    int nbr_plucked = 0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in1, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);

        size_t offset = line.find("@H");
        if( offset != std::string::npos )
        {
            std::string prefix = line.substr(0,offset);
            auto it = input.find(prefix);
            bool trigger = false;
#ifdef TRIGGER
            trigger = (prefix==TRIGGER);
#endif
            if( trigger )
                printf( "Triggered (%s) reading bulk\n", it==input.end()?"not found":"found" );
            if( it != input.end() )
            {
                std::vector<std::string> main_line;
                std::string moves_txt;
                get_main_line( line, main_line, moves_txt );
                if( trigger )
                    printf( "moves : %s\n", moves_txt.c_str() );
                for( GAME &g: it->second )
                {
                    if( trigger )
                        printf( "%s: %s\n", g.replaced?"true":"false", g.moves_txt.c_str() );
                    if( !g.replaced && g.moves.size()==main_line.size() )
                    {
                        std::vector<thc::Move> moves;
                        convert_moves( main_line, moves );
                        if( g.moves == moves )
                        {
                            std::pair<double,std::string> pr(line_nbr,line);
                            vout[g.line_nbr] = pr;
                            g.replaced = true;
                            nbr_plucked++;
                            if( trigger )
                                printf( "Woo hoo: %lu\n", g.line_nbr+1 );
                        }
                        break;
                    }
                }
            }
        }
        line_nbr++;
        if( (line_nbr % 100000) == 0 )
            printf( "%lu lines read, %d lines plucked\n", line_nbr, nbr_plucked );
    }

    // Find and report on unreplaced lines
    line_nbr=0;
    int nbr_not_found=0;
    for( std::pair<double,std::string> &pr : vout )
    {
        if( pr.second == "" )
        {
            pr.second = vin[line_nbr];
            if( nbr_not_found < 1 )
                printf( "line %lu not found: %s\n", line_nbr+1, pr.second.c_str() );
            nbr_not_found++;
        }
        line_nbr++;
    }
    printf( "%d lines were not replaced\n", nbr_not_found );

    // Re-order the output, to match locations in original bulk file
    if( reorder )
    {
        printf("Before reorder\n" );
        for( int i=0; i<20; i++ )
        {
            std::pair<double,std::string> &pr = vout[i];
            printf( "%.1f: %s...\n", pr.first, pr.second.substr(0,65).c_str() );
        }

        // Assumes input is pairs, and we still want pairs to be adjacent after sort
        bool even = false;
        std::pair<double,std::string> *previous = NULL;
        for( std::pair<double,std::string> &pr : vout )
        {
            if( !even )
                previous = &pr;
            else
            {
                if( pr.first < previous->first )
                    previous->first = pr.first + 0.1;
                else
                    pr.first = previous->first + 0.1;
            }
            even = !even;
        }
        printf("After pair adjust\n" );
        for( int i=0; i<20; i++ )
        {
            std::pair<double,std::string> &pr = vout[i];
            printf( "%.1f: %s...\n", pr.first, pr.second.substr(0,65).c_str() );
        }
        std::sort( vout.begin(), vout.end() );
        printf("After reorder\n" );
        for( int i=0; i<20; i++ )
        {
            std::pair<double,std::string> &pr = vout[i];
            printf( "%.1f: %s...\n", pr.first, pr.second.substr(0,65).c_str() );
        }
    }

    // Write the output
    printf( "Writing output, including %d plucked lines\n", nbr_plucked );
    if( utf8_bom )
        out.write( "\xef\xbb\xbf", 3 );
    for( std::pair<double,std::string> pr : vout )
    {
        std::string line = pr.second;
        util::putline(out,line);
    }
    return 0;
}


static int do_refine_dups( std::ifstream &in, std::ofstream &out )
{
    unsigned long line_nbr=0;
    bool utf8_bom = false;
    std::string date="@H[Date \"";    //  "@H[Date \""
    size_t date_len = date.length();
    std::string l1, l2, d1, d2, wb1, wb2;
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
            out.write( "\xef\xbb\xbf", 3 );
        }

        // Check whether dates, white_black are same for line pairs
        size_t offset = line.find(date);
        std::string extracted_date="";
        if( offset != std::string::npos )
        {
            offset += date_len;
            size_t offset2 = line.find("\"",offset);
            if( offset2 != std::string::npos )
                extracted_date = line.substr(offset,offset2-offset);
        }
        std::string white_black="";
        offset = line.find("@H");
        if( offset != std::string::npos )
        {
            std::string prefix = line.substr(0,offset);
            size_t offset2 = prefix.find_last_of(" ");
            if( offset2 != std::string::npos )
                white_black = line.substr(offset2,offset-offset2);
        }
        if( line_nbr%2==0 )
        {
            d1 = extracted_date;
            wb1 = white_black;
            l1 = line;
        }
        else
        {
            d2 = extracted_date;
            wb2 = white_black;
            l2 = line;
            std::vector<std::string> main_line;
            std::string moves_txt;
            get_main_line( line, main_line, moves_txt );
            std::string hash(wb1==wb2?"":"#");
            std::string asterisk(d1==d2?"":"*");
            std::string lessthan( main_line.size() < 20 ? // 10 whole moves
                "<" : "");
            std::string prefix = hash + asterisk + lessthan;
            if( prefix != "" )
                prefix += " ";
            line = prefix;
            line += l1;
            util::putline(out,line);
            line = prefix;
            line += l2;
            util::putline(out,line);
        }
        line_nbr++;
    }
    return 0;
}

static int do_remove_exact_pairs( std::ifstream &in, std::ofstream &out )
{
    unsigned long line_nbr=0;
    bool utf8_bom = false;
    bool even=false;
    std::string previous;
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
            out.write( "\xef\xbb\xbf", 3 );
        }

        if( even )
        {
            if( line != previous )
            {
                util::putline(out,previous);
                util::putline(out,line);
            }
        }
        else
            previous = line;
        even = !even;
    } 
    return 0;
}

