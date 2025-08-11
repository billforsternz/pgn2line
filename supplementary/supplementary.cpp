// supplementary.cpp : Do something special to LPGN file
//

/*

Some recent stuff
    z = collect_fide_id
        fin_aux, fin, fout
        fin_aux = list of nzl fide ids
        fin = input .lpgn
        fout = output .lpgn NZL player games with names modified with " #NZ#"
    zz = collect_fide_id
        fin_aux, fin, fout
        Same as z but include non NZ games from events with >50% NZ players (so NZ events)
    lbi = lichess broadcast improve
        fin, fout
        fin  = input .lpgn
        fout = output .lpgn Improvements, eg create Event and Date fields if absent
    gf = get_name_fide_id
        fin, fout
        fin = input .lpgn
        fout = ordered list of all FIDE id, name pairs from games file
    pf = put_name_fide_id
        fin_aux, fin, fout
        fin_aux = list of all FIDE id, name pairs
        fin = input .lpgn
        fout = output .lpgn If FIDE id found in list, replace player name with name from list (with special Gino patch)
    nzcf_game_id
        fin, fout
        fin = input .lpgn
        fout = output .lpgn all games assigned a unique, incrementing NzcfGameId 
    improve (a bit weird this one)
        fin_aux, fin, fout
        fin_aux = input .lpgn changed / improved games only
        fin = input .lpgn
        fout = output .lpgn input .lpgn with original games replaced by improved games (based on matching "Round" tag - so all games in one tournament right?
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "key.h"
#include "cmd.h"
#include "..\util.h"
#include "..\thc.h"

typedef enum
{
    nzcf_game_id,       
    collect_fide_id,
    get_known_fide_id_games_plus,
    add_ratings,        
    tabiya,             
    get_name_fide_id,
    propogate,
    fide_id_report,
    fide_id_to_name,
    lichess_broadcast_improve,
    put_name_fide_id,   
    bulk_out_skeleton,  
    improve,            
    remove_auto_commentary,
    justify,            
    golden,             
    times,               
    event,              
    teams,              
    hardwired,      
    refine_dups,        
    pluck_games,        
    pluck_games_reorder,
    remove_exact_pairs, 
    temp
} CMD_ENUM;

struct COMMAND
{
    CMD_ENUM    e;
    int         argc;
    const char *help;
};

static COMMAND table[] =
{
    {nzcf_game_id,       4,  "n in.lpgn out.lpgn                     ;Add nzcf game ids"},
    {collect_fide_id,    5,  "z nz-fide-ids.txt in.lpgn out.lpgn     ;Collect games with NZ players"},
    {get_known_fide_id_games_plus,
                         5,  "zz nz-fide-ids.txt in.lpgn out.lpgn    ;Collect games with NZ players and NZ tournaments"},
    {add_ratings,        5,  "r ratings.txt in.lpgn out.lpgn         ;Fix Lichess names and add ratings"},
    {tabiya,             4,  "y in.lpgn out.lpgn                     ;Find Tabiyas in file"},
    {get_name_fide_id,   4,  "gf in.lpgn id-players.txt              ;Get Player names from FIDE-ids from file"},
    {propogate,          8,  "propogate manual-fide-ids.txt nat-fide-ids.txt fide-ids.txt in.lpgn out.lpgn report.txt"},
    {fide_id_report,     4,  "fir in.lpgn report.txt                 ;Report on fide-id name pairs\n" },
    {fide_id_to_name,    8,  "rename fide-ids-1.txt 2.txt 3.txt 4.txt in.lpgn out.lpgn ;Rename players with fide-ids from up to 4 files\n" },
    {lichess_broadcast_improve,4,"lbi in.lpgn out.lpgn                  ;Lichess broadcast improve"},          
    {put_name_fide_id,   5,  "pf id-players.txt in.lpgn out.lpgn     ;Put Player names from FIDE-ids to file"},
    {bulk_out_skeleton,  5,  "s bulk.lpgn skeleton.lpgn out.lpgn     ;Find games from bulk for skeleton"},
    {improve,            5,  "i bulk.lpgn improve.lpgn out.lpgn      ;Replace bulk games with improved game"},
    {remove_auto_commentary,4,"rac in.lpgn out.lpgn                  ;Remove auto generated comments"},
    {justify,            4,  "justify in.lpgn out.lpgn               ;Justify move section"},
    {golden,             4,  "g in.lpgn out.lpgn                     ;golden age of chess program"},
    {times,              4,  "1 in.lpgn out.lpgn                     ;adds 'Lost on time' comment"},
    {event,              5,  "2 in.lpgn out.lpgn replacement-event   ;change event name"},
    {teams,              6,  "3 in.lpgn out.lpgn name-tsv teams-csv  ;fixup team event (somehow:)"},
    {hardwired,          4,  "4 in.lpgn out.lpgn                     ;hardwired Elo fix"},
    {refine_dups,        4,  "5 in.lpgn out.lpgn                     ;refine candidate dup pairs"},
    {pluck_games,        5,  "6 bulk.lpgn in.lpgn out.lpgn           ;replace games in input with close matches from bulk"},
    {pluck_games_reorder,5,  "7 bulk.lpgn in.lpgn out.lpgn           ;like 6, but assume pairs and re-order based on bulk location"},
    {remove_exact_pairs, 4,  "8 in.lpgn out.lpgn                     ;remove exact dup pairs"},
    {temp,               3,  "temp out.txt                           ;temp miscellaneous utility sorry!"}
};

int main( int argc, const char **argv )
{
#if 0 // def _DEBUG
    const char *args[] =
    {
        /*"dont-care.exe",
        "rac",
        "c:/users/bill/documents/chess/nzl/2025/lichess-broadcasts/b.lpgn",
        "c:/users/bill/documents/chess/nzl/2025/lichess-broadcasts/c.lpgn" */
        #if 0
        "dont-care.exe",
        "temp",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-combined-2025.txt"  
        #endif
        #if 0
        "dont-care.exe",
        "fir",
        "c:/users/bill/documents/chess/nzl/2025/nzl2024-out.lpgn",
        "c:/users/bill/documents/chess/nzl/2025/fir-report.txt"
        #endif
        #if 0
        "dont-care.exe",
        "propogate",
        "c:/users/bill/documents/chess/nzl/2025/manual-fide-id-adjustments.txt",
        "c:/users/bill/documents/chess/nzl/2025/nz-fide-id-combined.txt",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-combined-2025.txt",
        "c:/users/bill/documents/chess/nzl/2025/nzl2024.lpgn",
        "c:/users/bill/documents/chess/nzl/2025/nzl2024-out.lpgn",
        "c:/users/bill/documents/chess/nzl/2025/player-details-fide-2024.txt"
        #endif
        #if 1
        "dont-care.exe",
        "rename",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2025.txt",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2018.txt",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2000.txt",
        "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-1992.txt",
        "c:/users/bill/documents/chess/nzl/2025/nzl2024-out.lpgn",
        "c:/users/bill/documents/chess/nzl/2025/nzl2024-out2.lpgn"
        #endif
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args;
#endif
    CMD_ENUM purpose;
    bool ok = (argc>=2);
    if( ok )
    {
        ok = false;
        for( int i=0; i<sizeof(table)/sizeof(table[0]); i++ )
        {
            COMMAND *p = &table[i];
            std::string s(p->help);
            size_t offset = s.find(" ");
            if( offset != std::string::npos && std::string(argv[1]) == s.substr(0,offset) )
            {
                purpose = p->e;
                ok = (argc == p->argc);
                break;
            }
        }
    }
    if( !ok )
    {
        printf("Usage:\n");
        for( int i=0; i<sizeof(table)/sizeof(table[0]); i++ )
        {
            COMMAND *p = &table[i];
            printf( "%s\n", p->help );
        }
        printf("Notes: for 1 adds 'Lost on time' comment if \"Time forfeit\" Termination field in Lichess LPGN\n");
        return -1;
    }

    // Run argc == 3 commands here
    if( purpose == temp )
    {
        std::string fout(argv[2]);
        std::ofstream out( fout.c_str() );
        if( !out )
        {
            printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
            return -1;
        }
        return cmd_temp( out );
    }

    // Find arg index of main input and output files
    int argi_input_file = 2;
    switch( purpose )
    {
        case pluck_games:
        case pluck_games_reorder:
        case bulk_out_skeleton:
        case improve:
        case add_ratings:
        case collect_fide_id:
        case get_known_fide_id_games_plus:
        case put_name_fide_id:
            argi_input_file = 3;
            break;
        case propogate:
            argi_input_file = 5;
            break;
        case fide_id_to_name:
            argi_input_file = 6;
    }

    // Open main input and output files
    std::string fin(argv[argi_input_file]);
    std::string fout(argv[argi_input_file+1]);
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

    // Open aux_input file for those commands that require it
    switch( purpose )
    {
        case pluck_games:
        case pluck_games_reorder:
        case bulk_out_skeleton:
        case improve:
        case add_ratings:
        case collect_fide_id:
        case get_known_fide_id_games_plus:
        case put_name_fide_id:
        case propogate:
        case fide_id_to_name:
        {
            std::string fin_aux(argv[2]);
            std::ifstream in_aux(fin_aux.c_str());
            if( !in_aux )
            {
                printf( "Error; Cannot open file %s for reading\n", fin_aux.c_str() );
                return -1;
            }

            // Run aux_input file commands
            switch( purpose )
            {
                case pluck_games:
                case pluck_games_reorder:
                {
                    return cmd_pluck( in_aux, in, out, purpose==pluck_games_reorder );
                    break;
                }
                case bulk_out_skeleton:
                {
                    return cmd_bulk_out_skeleton( in_aux, in,  out );
                    break;
                }
                case improve:
                {
                    return cmd_improve( in_aux, in,  out );
                    break;
                }
                case add_ratings:
                {
                    return cmd_add_ratings( in_aux, in,  out );
                    break;
                }
                case collect_fide_id:
                {
                    return cmd_collect_fide_id( in_aux, in,  out );
                    break;
                }
                case get_known_fide_id_games_plus:
                {
                    return cmd_get_known_fide_id_games_plus( in_aux, in,  out );
                    break;
                }
                case put_name_fide_id:
                {
                    return cmd_put_name_fide_id( in_aux, in,  out );
                    break;
                }
                case fide_id_to_name:
                case propogate:
                {
                    std::string fin_aux2(argv[3]);
                    std::ifstream in_aux2(fin_aux2.c_str());
                    if( !in_aux2 )
                    {
                        printf( "Error; Cannot open file %s for reading\n", fin_aux2.c_str() );
                        return -1;
                    }
                    std::string fin_aux3(argv[4]);
                    std::ifstream in_aux3(fin_aux3.c_str());
                    if( !in_aux3 )
                    {
                        printf( "Error; Cannot open file %s for reading\n", fin_aux3.c_str() );
                        return -1;
                    }
                    switch( purpose )
                    {
                        case fide_id_to_name:
                        {
                            std::string fin_aux4(argv[5]);
                            std::ifstream in_aux4(fin_aux4.c_str());
                            if( !in_aux4 )
                            {
                                printf( "Error; Cannot open file %s for reading\n", fin_aux4.c_str() );
                                return -1;
                            }
                            return cmd_fide_id_to_name( in_aux, in_aux2, in_aux3, in_aux4, in, out );
                        }
                        case propogate:
                        {
                            std::string fout_report(argv[7]);
                            std::ofstream out_report(fout_report.c_str());
                            if( !out_report )
                            {
                                printf( "Error; Cannot open file %s for writing\n", fout_report.c_str() );
                                return -1;
                            }
                            return cmd_propogate( in_aux, in_aux2, in_aux3, in, out, out_report );
                        }
                    }
                    break;
                }
            }
        }
    }

    // Run all other commands
    switch( purpose )
    {
        case remove_auto_commentary:
        {
            return cmd_remove_auto_commentary( in, out );
            break;
        }
        case fide_id_report:
        {
            return cmd_fide_id_report( in, out );
            break;
        }
        case justify:
        {
            return cmd_justify( in, out );
            break;
        }
        case nzcf_game_id:
        {
            return cmd_nzcf_game_id( in, out );
            break;
        }
        case refine_dups:
        {
            return cmd_refine_dups( in, out );
            break;
        }
        case remove_exact_pairs:
        {
            return cmd_remove_exact_pairs( in, out );
            break;
        }
        case get_name_fide_id:
        {
            return cmd_get_name_fide_id( in, out );
            break;
        }
        case lichess_broadcast_improve:
        {
            return cmd_lichess_broadcast_improve( in, out );
            break;
        }
        case tabiya:
        {
            return cmd_tabiya( in, out );
            break;
        }
        case hardwired:
        {
            return cmd_hardwired( in, out );
            break;
        }
        case times:
        {
            return cmd_time( in, out );
            break;
        }
        case golden:
        {
            return cmd_golden( in, out );
            break;
        }
        case event:
        {
            std::string replacement_name( argv[4] );
            return cmd_event( in, out, replacement_name );
            break;
        }
        case teams:
        {
            std::string name_tsv( argv[4] );
            std::string teams_csv( argv[5] );
            return cmd_teams( in, out, name_tsv, teams_csv );
            break;
        }
    }
    return 0;
}

