// supplementary.cpp : Do something special to LPGN file
//  cmd.cpp, breakout the individual commands

#ifndef CMD_H_INCLUDED
#define CMD_H_INCLUDED
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

// Basic form: fin, fout
int cmd_fide_id_report( std::ifstream &in, std::ofstream &out );
int cmd_remove_auto_commentary( std::ifstream &in, std::ofstream &out );
int cmd_justify( std::ifstream &in, std::ofstream &out );
int cmd_nzcf_game_id( std::ifstream &in, std::ofstream &out );
int cmd_refine_dups( std::ifstream &in, std::ofstream &out );
int cmd_remove_exact_pairs( std::ifstream &in, std::ofstream &out );
int cmd_get_name_fide_id( std::ifstream &in, std::ofstream &out );
int cmd_lichess_broadcast_improve( std::ifstream &in, std::ofstream &out );
int cmd_hardwired( std::ifstream &in, std::ofstream &out );
int cmd_time( std::ifstream &in, std::ofstream &out );

// Complete applications
int cmd_golden( std::ifstream &in, std::ofstream &out );
int cmd_tabiya( std::ifstream &in, std::ofstream &out );

// Aux input file: fin_aux, fin, fout
int cmd_pluck( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out, bool reorder );
int cmd_bulk_out_skeleton( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_improve( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_add_ratings( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_collect_fide_id( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_put_name_fide_id( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_get_known_fide_id_games_plus( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );
int cmd_fide_id_to_name( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out );

// Misc
int cmd_event( std::ifstream &in, std::ofstream &out, std::string &replace_event );
int cmd_teams( std::ifstream &in, std::ofstream &out, std::string &name_tsv, std::string &teams_csv );
int cmd_temp( std::ofstream &out );
int cmd_propogate( std::ifstream &in_aux_manual, std::ifstream &in_aux_loc, std::ifstream &in_aux_fide,
                   std::ifstream &in, std::ofstream &out,
                   std::ofstream &out_report );
int cmd_normalise( std::ifstream &in, std::ofstream &out );

#endif //CMD_H_INCLUDED
