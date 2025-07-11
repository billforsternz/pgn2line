// supplementary.cpp : Do something special to LPGN file
//

/*

Some recent stuff
    z = collect_fide_id
        fin1, fin2, fout
        fin1 = list of nzl fide ids
        fin2 = input .lpgn
        fout = output .lpgn NZL player games with names modified with " #NZ#"
    lbi = lichess broadcast improve
        fin, fout
        fin  = input .lpgn
        fout = output .lpgn Improvements, eg create Event and Date fields if absent
    gf = get_name_fide_id
        fin, fout
        fin = input .lpgn
        fout = ordered list of all FIDE id, name pairs from games file
    pf = put_name_fide_id
        fin1, fin2, fout
        fin1 = list of all FIDE id, name pairs
        fin2 = input .lpgn
        fout = output .lpgn If FIDE id found in list, replace player name with name from list (with special Gino patch)
    nzcf_game_id
        fin, fout
        fin = input .lpgn
        fout = output .lpgn all games assigned a unique, incrementing NzcfGameId 
    improve (a bit weird this one)
        fin1, fin2, fout
        fin1 = input .lpgn changed / improved games only
        fin2 = input .lpgn
        fout = output .lpgn input .lpgn with original games replaced by improved games (based on matching "Round" tag - so all games in one tournament right?
*/



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

static int func_collect_fide_id( std::ifstream &in1, std::ifstream &in2, std::ofstream &out );
static int func_tabiya( std::ifstream &in, std::ofstream &out );
static int func_put_name_fide_id( std::ifstream &in1, std::ifstream &in2, std::ofstream &out );
static int func_get_name_fide_id( std::ifstream &in, std::ofstream &out );
static int func_lichess_broadcast_improve( std::ifstream &in, std::ofstream &out );
static int pluck( std::ifstream &in1, std::ifstream &in2, std::ofstream &out, bool reorder );
static int func_justify( std::ifstream &in1, std::ofstream &out );
static int func_nzcf_game_id( std::ifstream &in, std::ofstream &out );
static int func_remove_auto_commentary( std::ifstream &in1, std::ofstream &out );
static int func_add_ratings( std::ifstream &in1, std::ifstream &in2, std::ofstream &out );
static int func_bulk_out_skeleton( std::ifstream &in_bulk, std::ifstream &in_skeleton, std::ofstream &out );
static int func_improve( std::ifstream &in_bulk, std::ifstream &in_improved, std::ofstream &out );
static int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt, int &nbr_comments );
#define GET_MAIN_LINE_MASK_HAS_VARIATIONS 1
#define GET_MAIN_LINE_MASK_HAS_COMMENTS 2
#define GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO 4
static void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out );
static int do_refine_dups( std::ifstream &in, std::ofstream &out );
static int do_remove_exact_pairs( std::ifstream &in, std::ofstream &out );
static int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, int &nbr_comments );
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
#if 0
    const char *args[] =
    {
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/supp.exe",
        "s",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/bobwade/all-games.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/bobwade/skel-2.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/bobwade/challengers-2.lpgn"
    };
#endif
#if 0
    const char *args[] =
    {
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/supp.exe",
        "r",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/anzac-2023.txt",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/anzac-2023.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/out/anzac-2023.lpgn"
    };
#endif
#if 0
    const char *args[] =
    {
        "C:/Users/Bill/Documents/ChessDatabase/twicbase/pgn/supp.exe",
        "k",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/combined/combined4.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/combined/trimmed.lpgn"
    };
#endif
#if 0
    const char *args[] =
    {
        "dont-care.exe",
        "j",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/refine-2022/old1.lpgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/refine-2022/old1-rejustified.lpgn"
    };
#endif
#if 0
    const char *args[] =
    {
        "dont-care.exe",
        "r",
        "C:/Users/Bill/Documents/Chess/congress/nzchamps.txt",
        "C:/Users/Bill/Documents/Chess/congress/boards-11-24.lpgn",
        "C:/Users/Bill/Documents/Chess/congress/boards-11-24-out2.lpgn"
    };
#endif
#if 0
    const char *args[] =
    {
        "dont-care.exe",
        "z",
        "C:/Users/Bill/Documents/Chess/twic/fide-ids-sorted.txt",
        "C:/Users/Bill/Documents/Chess/twic/twic-400-1542.lpgn",
        "C:/Users/Bill/Documents/Chess/twic/nz.lpgn"
    };
#endif

#if 0
    const char *args[] =
    {
        "dont-care.exe",
        "y",
        "C:/Users/Bill/Documents/Chess/twic/twic-400-1542.lpgn",
        "C:/Users/Bill/Documents/Chess/twic/tabiyas.lpgn"
    };
#endif

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


    enum {time,event,teams,hardwired,pluck_games,pluck_games_reorder,refine_dups,
          remove_exact_pairs,golden,get_name_fide_id,lichess_broadcast_improve,put_name_fide_id,tabiya,collect_fide_id,
          add_ratings,bulk_out_skeleton,remove_auto_commentary,nzcf_game_id,
          justify,improve} purpose=time;
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
        else if( s == "y" )
        {
            ok = (argc==4);
            purpose = tabiya;
        } 
        else if( s == "z" )
        {
            ok = (argc==5);
            purpose = collect_fide_id;
        } 
        else if( s == "gf" )
        {
            ok = (argc==4);
            purpose = get_name_fide_id;
        }
        else if( s == "lbi" )
        {
            ok = (argc==4);
            purpose = lichess_broadcast_improve;
        }
        else if( s == "pf" )
        {
            ok = (argc==5);
            purpose = put_name_fide_id;
        }
        else if( s == "s" )
        {
            ok = (argc==5);
            purpose = bulk_out_skeleton;
        }
        else if( s == "k" )
        {
            ok = (argc==4);
            purpose = remove_auto_commentary;
        }
        else if( s == "j" )
        {
            ok = (argc==4);
            purpose = justify;
        }
        else if( s == "n" )
        {
            ok = (argc==4);
            purpose = nzcf_game_id;
        }
        else if( s == "i" )
        {
            ok = (argc==5);
            purpose = improve;
        }
    }
    if( !ok )
    {
        printf( 
            "Usage:\n"
            "n in.lpgn out.lpgn                     ;Add nzcf game ids\n"
            "z nz-fide-ids.txt in.lpgn out.lpgn     ;Collect games with NZ players\n"
            "r ratings.txt in.lpgn out.lpgn         ;Fix Lichess names and add ratings\n"
            "y in.lpgn out.lpgn                     ;Find Tabiyas in file\n"
            "gf in.lpgn id-players.txt              ;Get Player names from FIDE-ids from file\n"
            "pf id-players.txt in.lpgn out.lpgn     ;Put Player names from FIDE-ids to file\n"
            "s bulk.lpgn skeleton.lpgn out.lpgn     ;Find games from bulk for skeleton\n"
            "i bulk.lpgn improve.lpgn out.lpgn      ;Replace bulk games with improved game\n"
            "k in.lpgn out.lpgn                     ;Remove auto generated comments\n"
            "j in.lpgn out.lpgn                     ;Justify move section\n"
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
    std::string fout(argv[(purpose==pluck_games||purpose==pluck_games_reorder||purpose==add_ratings||purpose==put_name_fide_id||purpose==collect_fide_id||purpose==bulk_out_skeleton||purpose==improve)?4:3]);
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
    if( purpose == remove_auto_commentary )
        return func_remove_auto_commentary( in, out );
    else if( purpose == justify )
        return func_justify( in, out );
    else if( purpose == nzcf_game_id )
        return func_nzcf_game_id( in, out );
    else if( purpose == refine_dups )
        return do_refine_dups( in, out );
    else if( purpose == remove_exact_pairs )
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
    else if( purpose==bulk_out_skeleton )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return func_bulk_out_skeleton( in, in2,  out );
    }
    else if( purpose==improve )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return func_improve( in, in2,  out );
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
    else if( purpose == collect_fide_id )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return func_collect_fide_id( in, in2,  out );
    }
    else if( purpose == put_name_fide_id )
    {
        std::string fin2(argv[3]);
        std::ifstream in2(fin2.c_str());
        if( !in2 )
        {
            printf( "Error; Cannot open file %s for reading\n", fin2.c_str() );
            return -1;
        }
        return func_put_name_fide_id( in, in2,  out );
    }
    else if( purpose == get_name_fide_id )
    {
        return func_get_name_fide_id( in, out );
    }
    else if( purpose == lichess_broadcast_improve )
    {
        return func_lichess_broadcast_improve( in, out );
    }
    else if( purpose == tabiya )
    {
        return func_tabiya( in, out );
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
                    size_t len = line.length();
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
        cr.PlayMove(m);
    }
}

static int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, int &nbr_comments )
{
    std::vector<std::string> main_line;
    std::string moves_txt;
    int mask = get_main_line( moves, main_line, moves_txt, nbr_comments );

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

    // Step 2, word wrap, lines should have less than 80 characters, so 79 max
    int idx=0, nbr_characters_in_line=0;
    int last_space_idx=-1;
    for( char c: step1 )
    {
        nbr_characters_in_line++;
        if( c == ' ' )
            last_space_idx = idx;
        if( nbr_characters_in_line >= 80 && last_space_idx > 0 )
        {
            step1[last_space_idx] = '\n';
            nbr_characters_in_line = idx - last_space_idx;
            last_space_idx = -1;
        }
        idx++;
    }

    // Step 3, Add @M new line move headers
    normal = "@M";
    for( char c: step1 )
    {
        if( c == '\n' )
            normal += "@M";
        else
            normal += c;
    }
    return mask;
}

static int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt, int &nbr_comments )
{
    nbr_comments = 0;
    int mask=0;
    main_line.clear();
    moves_txt.clear();
	size_t offset = s.find("@M");
	if( offset == std::string::npos )
        return mask;
    offset += 2;
    int nest_depth = 0;
    std::string move;
    size_t len = s.length();
    enum {in_main_line,in_move,in_variation,in_comment} state=in_main_line, old_state=in_main_line, save_state=in_main_line;
    bool auto_comment  = false;
    bool has_variations= false;
    bool has_comments  = false;
    bool only_auto_comments=true;
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
                    has_variations=true;
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
                    has_variations=true;
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
                if( c == '%' )
                    auto_comment = true;
                else if( c == '}' )
                {
                    if( !auto_comment )
                        only_auto_comments = false;
                    state = save_state;
                }
                break;
            }
        }
        if( state != old_state )
        {
            if( state == in_comment )
            {
                nbr_comments++;
                has_comments = true;
                auto_comment = false;
                save_state = old_state;
            }
            else if( old_state == in_move )
            {
                main_line.push_back(move);
                moves_txt += " ";
                moves_txt += move;
            }
        }
    }
    if( has_variations )
        mask |= GET_MAIN_LINE_MASK_HAS_VARIATIONS;
    if( has_comments )
    {
        mask |= GET_MAIN_LINE_MASK_HAS_COMMENTS;
        if( only_auto_comments )
            mask |= GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO;
    }
    return mask;
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
    std::string lower_case_name;
    std::string surname;
    std::string forenames;
    std::string forename;
    std::string rating;
};

bool find_player( std::string name, const std::vector<PLAYER> &ratings, PLAYER &rating )
{
    //if( name == "Leonard McLaren" )
    //    printf( "Debug!\n" );
    bool found = false;
    std::string orig_name = name;
    util::rtrim(name);
    name = util::tolower(name); // do all matching in lower case

    // For those times when the player names areb't being changed, just the ratings
    for( PLAYER r: ratings )
    {
        if( r.lower_case_name == name )
        {
            rating = r;
            return true;
        }
    }

    auto offset = name.find_first_of(',');
    std::string surname = name;
    std::string forename;
    std::string forenames;
    if( offset != std::string::npos )
    {
        surname = name.substr( 0, offset );
        util::rtrim(surname);
        forenames = name.substr( offset+1 );
        util::ltrim(forenames);
    }
    else
    {
        offset = name.find_last_of(' ');
        if( offset != std::string::npos )
        {
            surname = name.substr( offset+1 );
            forenames = name.substr( 0, offset );
        }
    }
    util::rtrim(forenames);
    std::vector<std::string> name_vec_temp;
    util::split( forenames, name_vec_temp );
    if( name_vec_temp.size() > 0 && name_vec_temp[0].length() > 0 )
        forename = name_vec_temp[0];
    int count = 0;
    for( PLAYER r: ratings )
    {
        if( r.surname == surname )
        {
            if( count == 0 )
                rating = r;
            count++;
        }
    }
    if( count == 1 )
        found = true;
    else if( count == 0 )
    {
        std::string space_surname = std::string(" ") + surname;
        bool match=false;
        for( PLAYER r: ratings )
        {
            auto offset = r.lower_case_name.find( space_surname );
            match = (offset != std::string::npos);
            if( match )
            {
                if( count == 0 )
                    rating = r;
                count++;
                break;
            }
        }
        if( count > 0 )
            found = true;
        if( count > 1 )
        {
            printf( "%s -> %s\n", orig_name.c_str(), rating.name.c_str() );
            printf( "***** Warning, multiple one token surname only matches, picking first!\n");
        }
    }
    else
    {
        count = 0;
        for( PLAYER r: ratings )
        {
            if( r.surname == surname && r.forename == forename )
            {
                if( count == 0 )
                    rating = r;
                count++;
            }
        }
        if( count > 0 )
            found = true;
        if( count > 1 )
        {
            printf( "%s -> %s\n", orig_name.c_str(), rating.name.c_str() );
            printf( "***** Warning, multiple surname plus forename matches, picking first!\n");
        }
        else if( count == 0 )
        {
            count = 0;
            for( PLAYER r: ratings )
            {
                if( r.surname == surname  && r.forename.length()>0 && forename.length()>0 && r.forename[0] == forename[0] )
                {
                    if( count == 0 )
                        rating = r;
                    count++;
                }
            }
            if( count > 0 )
                found = true;
            if( count > 1 )
            {
                printf( "%s -> %s\n", orig_name.c_str(), rating.name.c_str() );
                printf( "***** Warning, multiple plus first name initial matches, picking first!\n");
            }
        }
    }
    if( !found )
    {
        printf( "%s\n", orig_name.c_str() );
        printf( "***** Not found!\n");
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
            size_t offset2 = line.find_first_of( "0123456789", offset );
            if( offset2 != std::string::npos )
            {
                PLAYER r;
                r.rating = line.substr(offset2);
                r.name   = line.substr(0,offset2);
                line = util::tolower(line);     // Do all matching in lower case
                util::rtrim(r.name);
                r.lower_case_name = util::tolower(r.name); 
                r.surname = line.substr(0,offset);
                r.forenames = line.substr(offset+1, offset2-(offset+1) );
                util::ltrim(r.forenames);
                util::rtrim(r.forenames);
                std::vector<std::string> name_vec_temp;
                util::split( r.forenames, name_vec_temp );
                if( name_vec_temp.size() > 0 )
                    r.forename = name_vec_temp[0];
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
            //if( name == "Burns, Christopher J" )
            //    printf( "Debug2!\n");
            ok = ok && find_player(name,ratings,r);
            ok = ok && key_replace( header, colour, r.name );
            if( ok )
                key_update(header,colour_elo,"",r.rating);
            colour     = "Black";
            colour_elo = "BlackElo";
        }
        std::string normal;
        int nbr_comments;
        int mask = lichess_moves_to_normal_pgn( header, moves, normal, nbr_comments );
        if( (mask & GET_MAIN_LINE_MASK_HAS_COMMENTS) && !(mask & GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO) )
        {
            printf( "Warning: removing one or more not automated comments\n");
        }
        util::putline(out,header+normal);
    }
    return 0;
}

struct GAME2
{
    std::string header;
    std::string moves;
};
    

static int func_bulk_out_skeleton( std::ifstream &in_bulk, std::ifstream &in_skeleton, std::ofstream &out )
{
    std::vector<GAME2> bulk_v;
    std::vector<GAME2> skeleton_v;
    std::vector<std::string> skeleton_headers;
    unsigned long line_nbr=0;
    bool utf8_bom = false;

    // Read bulk
    for(;;)
    {
        std::string line;
        if( !std::getline(in_bulk, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
        }

        auto offset = line.find( "@M" );
        if( offset != std::string::npos )
        {
            GAME2 g;
            g.header = line.substr(0,offset);
            g.moves  = line.substr(offset);
            bulk_v.push_back(g);
        }
    }

    // Read skeleton
    line_nbr=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in_skeleton, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
            utf8_bom = true;
        }

        auto offset = line.find( "@M" );
        if( offset != std::string::npos )
        {
            GAME2 g;
            g.header = line.substr(0,offset);
            g.moves  = line.substr(offset);
            skeleton_v.push_back(g);
        }
    }

    // Clunky O(N^2) algo
    int successes = 0;
    for( GAME2 &skeleton: skeleton_v )
    {
        std::string white, black, bulk_white, bulk_black;
        key_find(skeleton.header,"White",white);
        std::string white_orig = white;
        white = util::tolower(white);
        auto offset = white.find(',');
        if( offset != std::string::npos )
            white = white.substr(0,offset);
        util::rtrim(white);
        key_find(skeleton.header,"Black",black);
        std::string black_orig = black;
        black = util::tolower(black);
        offset = black.find(',');
        if( offset != std::string::npos )
            black = black.substr(0,offset);
        util::rtrim(black);
        int idx=0, count=0, found_idx;

        // Look through bulk games
        for( GAME2 &bulk: bulk_v )
        {
            key_find(bulk.header,"White",bulk_white);
            bulk_white = util::tolower(bulk_white);
            key_find(bulk.header,"Black",bulk_black);
            bulk_black = util::tolower(bulk_black);
            auto offset = bulk_white.find(white);
            bool found1 = (offset != std::string::npos);
            offset = bulk_black.find(black);
            bool found2 = (offset != std::string::npos);
            if( found1 && found2 )
            {
                count++;
                found_idx = idx;
            }
            idx++;
        }
        std::string round;
        key_find(skeleton.header,"Round",round);

        if( count != 1 )
            printf( "%s %s-%s ", round.c_str(), white_orig.c_str(), black_orig.c_str() );
        if( count == 0 )
            printf( " not found\n" );
        if( count > 1 )
            printf( " multiple hits so effectively not found\n" );
        if( count == 1 )
        {
            successes++;
            GAME2 &bulk = bulk_v[found_idx];
            key_find(bulk.header,"White",bulk_white);
            key_find(bulk.header,"Black",bulk_black);
            // printf( " found as %s-%s\n", bulk_white.c_str(), bulk_black.c_str() );
            util::putline( out, skeleton.header+bulk.moves );
        }
    }
    printf( "%u successes from %zu attempts\n", successes, skeleton_v.size() );
    return 0;
}

static int func_remove_auto_commentary( std::ifstream &in1, std::ofstream &out )
{
    int line_nbr=0;
    int fixed_lines=0, total_lines=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in1, line) )
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
        std::string normal;
        int nbr_comments;
        int mask = lichess_moves_to_normal_pgn( header, moves, normal, nbr_comments );
        bool has_variations = (mask & GET_MAIN_LINE_MASK_HAS_VARIATIONS) != 0;
        bool has_comments   = (mask & GET_MAIN_LINE_MASK_HAS_COMMENTS) != 0;
        bool all_comments_auto = (mask & GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO) != 0;
        total_lines++;
        if( has_comments && all_comments_auto && nbr_comments>1 && !has_variations )
        {
            util::putline(out,header+normal);       
            fixed_lines++;
        }
        else
        {
            util::putline(out,header+moves);       
        }
    }
    printf( "Normalised %u games (apparently with only automated comments) from %u total games\n", fixed_lines, total_lines );
    return 0;
}

static int func_nzcf_game_id( std::ifstream &in, std::ofstream &out )
{
    printf( "Pass 1: Find the maximum existing NzcfGameId\n" );
    int line_nbr=0;
    long max_so_far=0;
    bool at_least_one_missing = false;
    bool at_least_one_found = false;
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
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
        std::string game_id;
        bool found = key_find(header,"NzcfGameId",game_id);
        if( !found )
        {
            at_least_one_missing = true;
        }
        else
        {
            at_least_one_found = true;
            long id = atol(game_id.c_str());
            if( id>0 && id>max_so_far )
                max_so_far = id;
        }
    }
    if( !at_least_one_missing )
    {
        printf( "All games have NzcfGameId tags, no insertions required\n" );
        return 0;
    }
    if( !at_least_one_found )
    {
        printf( "No existing NzcfGameId tags found, all games will be assigned ids starting at 1\n" );
        printf( "Pass 2: Insert incrementing NzcfGameId tags for all games\n" );
    }
    else
    {
        printf( "Highest existing NzcfGameId is %ld, new ids will be assigned starting at %ld\n",
                    max_so_far, max_so_far+1 );
        printf( "Pass 2: Insert incrementing NzcfGameId tags for games that don't have them\n" );
    }
    in.clear();
    in.seekg(0);
    line_nbr=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
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
        std::string game_id;
        bool found = key_find(header,"NzcfGameId",game_id);
        if( !found )
        {
            game_id = util::sprintf("%ld",++max_so_far);
            key_add(header,"NzcfGameId",game_id);
        }
        util::putline(out,header+moves);       
    }
    return 0;
}

static int func_justify( std::ifstream &in1, std::ofstream &out )
{
    int line_nbr=0;
    int fixed_lines=0, total_lines=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in1, line) )
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

        // Unroll the moves
        if( moves.length() <= 2 )
            continue;
        std::string unrolled;
        offset = 2;
        auto offset2 = moves.find("@M",offset);
        while( offset2 != std::string::npos )
        {
            unrolled += moves.substr(offset,offset2-offset);
            unrolled += ' ';
            offset = offset2+2;
            offset2 = moves.find("@M",offset);
        }
        unrolled += moves.substr(offset);

        // Word wrap, lines should have less than 80 characters, so 79 max
        int idx=0, nbr_characters_in_line=0;
        int last_space_idx=-1;
        for( char c: unrolled )
        {
            nbr_characters_in_line++;
            if( c == ' ' )
                last_space_idx = idx;
            if( nbr_characters_in_line >= 80 && last_space_idx > 0 )
            {
                unrolled[last_space_idx] = '\n';
                nbr_characters_in_line = idx - last_space_idx;
                last_space_idx = -1;
            }
            idx++;
        }

        // Roll moves back into a lpgn line
        std::string rolled_up;
        rolled_up = "@M";
        for( char c: unrolled )
        {
            if( c == '\n' )
                rolled_up += "@M";
            else
                rolled_up += c;
        }
        util::putline(out,header+rolled_up);       
    }
    return 0;
}

static int func_improve( std::ifstream &in_bulk, std::ifstream &in_improve, std::ofstream &out )
{
    unsigned long line_nbr=0;
    bool utf8_bom = false;

    // Read improve
    std::map<std::string,GAME2> improved_rounds;
    line_nbr=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in_improve, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
            utf8_bom = true;
        }

        auto offset = line.find( "@M" );
        if( offset != std::string::npos )
        {
            GAME2 g;
            g.header = line.substr(0,offset);
            g.moves  = line.substr(offset);
            std::string round;
            key_find(g.header,"Round",round);
            improved_rounds[round] = g;
        }
    }

    // Read bulk
    line_nbr=0;
    int successes=0, attempts=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in_bulk, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        {
            line = line.substr(3);
        }

        auto offset = line.find( "@M" );
        if( offset != std::string::npos )
        {
            GAME2 g;
            g.header = line.substr(0,offset);
            g.moves  = line.substr(offset);
            std::string round;
            key_find(g.header,"Round",round);
            std::string white, black;
            key_find(g.header,"White",white);
            key_find(g.header,"Black",black);
            printf( "Round %5s: ", round.c_str() );
            auto it = improved_rounds.find(round);
            attempts++;
            if( it != improved_rounds.end() )
            {
                successes++;
                GAME2 &improved = it->second;
                printf( "Improve: %s-%s\n", white.c_str(), black.c_str() );
                util::putline( out, improved.header+improved.moves );
            }
            else
            {
                printf( "Retain: %s-%s\n", white.c_str(), black.c_str() );
                util::putline( out, g.header+g.moves );
            }
        }
    }
    printf( "%u successes from %u attempts\n", successes, attempts );
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
            int nbr_comments;
            get_main_line( line, main_line, g.moves_txt, nbr_comments );
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
                int nbr_comments;
                get_main_line( line, main_line, moves_txt, nbr_comments );
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
            int nbr_comments;
            get_main_line( line, main_line, moves_txt, nbr_comments );
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

//
//  Find NZ Players
//
static int func_collect_fide_id( std::ifstream &in1, std::ifstream &in2, std::ofstream &out )
{
    std::set<long> nz_fide_ids;
    std::string line;
    int line_nbr=0;
    unsigned int nbr_games_found=0;

    // Read the NZ fide ids
    for(;;)
    {
        if( !std::getline(in1, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        util::rtrim(line);
        long id = atol(line.c_str());
        if( id > 0 )
            nz_fide_ids.insert(id);
    }

    printf( "%zu NZ fide ids read from file\n", nz_fide_ids.size() );

    // Filter line by line
    line_nbr=0;
    for(;;)
    {
        if( !std::getline(in2, line) )
            break;
        int nbr_nz_players = 0;

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
        std::string white_fide_id;
        std::string black_fide_id;
        bool ok  = key_find( header, "WhiteFideId", white_fide_id );
        if( ok )
        {
            long id = atol(white_fide_id.c_str());
            auto it = nz_fide_ids.find(id);
            if( it != nz_fide_ids.end() )
            {
                nbr_nz_players++;
                std::string name;
                ok = key_find( header, "White", name );
                if( ok )
                    key_replace( header, "White", name + " #NZ#"  );
            }
        }
        ok  = key_find( header, "BlackFideId", black_fide_id );
        if( ok )
        {
            long id = atol(black_fide_id.c_str());
            auto it = nz_fide_ids.find(id);
            if( it != nz_fide_ids.end() )
            {
                nbr_nz_players++;
                std::string name;
                ok = key_find( header, "Black", name );
                if( ok )
                    key_replace( header, "Black", name + " #NZ#"  );
            }
        }
        if( nbr_nz_players > 0 )
        {
            nbr_games_found++;
            util::putline(out,header+moves);
        }
    }
    printf( "%u games found\n", nbr_games_found );
    return 0;
}


bool predicate_lt( const std::pair<long,std::string> &left,  const std::pair<long,std::string> &right )
{
    return left.first < right.first;
}


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


//
//  Find some good Tabiyas
//
#define TABIYA_PLY 16
#define CUTOFF 10
#define PLAYER_MIN_GAMES 20
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
static int func_tabiya( std::ifstream &in, std::ofstream &out )
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

bool lt_nbr_hits( const Tabiya &left, const Tabiya &right )
{
    return left.nbr_hits < right.nbr_hits;
}

bool lt_proportion_hits( const Tabiya &left, const Tabiya &right )
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
            int nbr_comments;
            std::string moves_txt;
            get_main_line( line, main_line, moves_txt, nbr_comments );
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
            int nbr_comments;
            std::string moves_txt;
            get_main_line( game, main_line, moves_txt, nbr_comments );
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

/*
    If Event starts with "Round " copy BroadcastName -> Event
    If Date absent, create Date after Site using UTCDate
*/
static int func_lichess_broadcast_improve( std::ifstream &in, std::ofstream &out )
{
    std::string line;
    int line_nbr=0;
    line_nbr=0;
    for(;;)
    {
        if( !std::getline(in, line) )
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
        std::string event;
        std::string name;
        bool ok  = key_find( header, "Event", event );
        bool ok2 = key_find( header, "BroadcastName", name );
        if( ok && ok2 && util::prefix(event,"Round ") )
        {
            key_replace(header,"Event",name);
        }
        std::string date;
        std::string utc_date;
        ok  = key_find( header, "Date", date );
        ok2 = key_find( header, "UTCDate", utc_date );
        if( !ok && ok2 )
        {
            key_update(header,"Date","Site",utc_date);
        }
        util::putline(out,header+moves);
    }
    return 0;
}

static int func_get_name_fide_id( std::ifstream &in, std::ofstream &out )
{
    std::map<long,std::string> id_names;
    std::string line;
    int line_nbr=0;

    // Find names line by line
    line_nbr=0;
    unsigned int nbr_names_replaced = 0;
    for(;;)
    {
        if( !std::getline(in, line) )
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
        std::string white_fide_id;
        std::string black_fide_id;
        bool ok  = key_find( header, "WhiteFideId", white_fide_id );
        std::string name;
        bool ok2 = key_find( header, "White", name );
        if( ok && ok2 )
        {
            long id = atol(white_fide_id.c_str());
            if( id == 4300963 )
                name = "Stark, John";
            if( id == 4314565 )
                name = "van der Hoorn, Thomas";
            if( id != 0 )
            {
                auto it = id_names.find(id);
                if( it == id_names.end() )
                    id_names[id] = name;
                else
                {
                    if( it->second != name )
                        printf( "Warning %ld->%s and %s (keep first)\n", id, it->second.c_str(), name.c_str() );
                }
            }
        }
        ok  = key_find( header, "BlackFideId", black_fide_id );
        ok2 = key_find( header, "Black", name );
        if( ok && ok2 )
        {
            long id = atol(black_fide_id.c_str());
            if( id == 4300963 )
                name = "Stark, John";
            if( id == 4314565 )
                name = "van der Hoorn, Thomas";
            if( id != 0 )
            {
                auto it = id_names.find(id);
                if( it == id_names.end() )
                    id_names[id] = name;
                else
                {
                    if( it->second != name )
                        printf( "Warning %ld->%s and %s (keep first)\n", id, it->second.c_str(), name.c_str() );
                }
            }
        }
    }
    printf( "%zu id name pairs harvested, writing to file\n", id_names.size() );
    std::vector<std::pair<long,std::string>> v;
    for( std::pair<long,std::string> pr: id_names )
        v.push_back(pr);
    std::sort( v.begin(), v.end(), predicate_lt );
    for( std::pair<long,std::string> pr: v )
    {
        std::string line = util::sprintf( "%ld %s", pr.first, pr.second.c_str() );
        util::putline(out,line);
    }
    return 0;
}

static int func_put_name_fide_id( std::ifstream &in1, std::ifstream &in2, std::ofstream &out )
{
    std::map<long,std::string> id_names;
    std::string line;
    int line_nbr=0;

    // Read id -> player file
    for(;;)
    {
        if( !std::getline(in1, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        util::rtrim(line);
        const char *s = line.c_str();
        long id = atol(s);
        if( id > 0 )
        {
            while( '0'<=*s && *s<='9' )
                s++;
            while( ' '==*s || *s=='\t' )
                s++;
            if( *s )
            {
                std::string name(s);
                auto it = id_names.find(id);
                if( it == id_names.end() )
                    id_names[id] = name;
                else
                {
                    if( it->second != name )
                        printf( "Warning %ld->%s and %s (keep first)\n", id, it->second.c_str(), name.c_str() );
                }
            }
        }
    }
    printf( "%zu fide ids, name pairs read from file\n", id_names.size() );

    // Filter line by line
    line_nbr=0;
    unsigned int nbr_names_replaced = 0;
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
        std::string white_fide_id;
        std::string black_fide_id;
        bool ok  = key_find( header, "WhiteFideId", white_fide_id );
        if( ok )
        {
            long id = atol(white_fide_id.c_str());
            auto it = id_names.find(id);
            if( it != id_names.end() )
            {
                std::string name;
                ok = key_find( header, "White", name );
                if( ok )
                {
                    if( id==4300963 && name.length()>=9 && name.substr(0,9)=="Thornton," )
                        printf("Special patch to avoid renaming Gino in old games\n");
                    else
                    {
                        nbr_names_replaced++;
                        key_replace( header, "White", it->second  );
                    }
                }
            }
        }
        ok  = key_find( header, "BlackFideId", black_fide_id );
        if( ok )
        {
            long id = atol(black_fide_id.c_str());
            auto it = id_names.find(id);
            if( it != id_names.end() )
            {
                std::string name;
                ok = key_find( header, "Black", name );
                if( ok )
                {
                    if( id==4300963 && name.length()>=9 && name.substr(0,9)=="Thornton," )
                        printf("Special patch to avoid renaming Gino in old games\n");
                    else
                    {
                        nbr_names_replaced++;
                        key_replace( header, "Black", it->second );
                    }
                }
            }
        }
        util::putline(out,header+moves);
    }
    printf( "%u names replaced (hopefully improved)\n", nbr_names_replaced );
    return 0;
}

