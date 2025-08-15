// supplementary.cpp : Do something special to LPGN file
//  cmd.cpp, breakout the individual commands

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
#include "cmd.h"

struct MicroGame
{
    std::string white;
    std::string black;
    int result;     // +1, -1 or 0
};

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

static bool find_player( std::string name, const std::vector<PLAYER> &ratings, PLAYER &rating )
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

//
//  The individual command handlers
//

int cmd_remove_auto_commentary( std::ifstream &in, std::ofstream &out )
{
    int line_nbr=0;
    int fixed_lines=0, total_lines=0;
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
        std::string normal;
        std::string encoded_clk_times;
        int nbr_comments;
        int mask = lichess_moves_to_normal_pgn( header, moves, normal, encoded_clk_times, nbr_comments );
        bool has_variations = (mask & GET_MAIN_LINE_MASK_HAS_VARIATIONS) != 0;
        bool has_comments   = (mask & GET_MAIN_LINE_MASK_HAS_COMMENTS) != 0;
        bool all_comments_auto = (mask & GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO) != 0;
        total_lines++;
        size_t len = encoded_clk_times.length();
        size_t remaining = len;
        offset=0;
        //std::string prev = "Result";
        for( int i=0; remaining>0; i++ )
        {
            size_t chunk = remaining<65 ? remaining : 65;
            std::string tag = util::sprintf("BabyClk%d",i+1);
            key_update(header,tag,"",encoded_clk_times.substr(offset,chunk) );
            //prev = tag;
            offset += chunk;
            remaining -= chunk;
        }
        if( has_comments && all_comments_auto && nbr_comments>=1 && !has_variations )
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
    std::string lichess_moves_to_normal_pgn_extra_stats();
    std::string s = lichess_moves_to_normal_pgn_extra_stats();
    printf( "%s\n", s.c_str() );
    return 0;
}

int cmd_justify( std::ifstream &in, std::ofstream &out )
{
    int line_nbr=0;
    int fixed_lines=0, total_lines=0;
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

int cmd_add_ratings( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out )
{
    // Read the input and fix it
    std::string line;
    if( !std::getline(in_aux, line) )
        return(-1);

    // Strip out UTF8 BOM mark (hex value: EF BB BF)
    if( line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
        line = line.substr(3);
    std::string event_val = line;
    if( !std::getline(in_aux, line) )
        return(-1);
    std::string site_val = line;

    // Read the ratings
    std::vector<PLAYER> ratings;
    for(;;)
    {
        std::string line;
        if( !std::getline(in_aux, line) )
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
        std::string encoded_clk_times;
        int nbr_comments;
        int mask = lichess_moves_to_normal_pgn( header, moves, normal, encoded_clk_times, nbr_comments );
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
    

 int cmd_bulk_out_skeleton( std::ifstream &in_bulk, std::ifstream &in_skeleton, std::ofstream &out )
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

 int cmd_nzcf_game_id( std::ifstream &in, std::ofstream &out )
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

 int cmd_improve( std::ifstream &in_bulk, std::ifstream &in_improve, std::ofstream &out )
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
 int cmd_pluck( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out, bool reorder )
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
        if( !std::getline(in, line) )
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
            std::vector<int> clk_times;
            int nbr_comments;
            get_main_line( line, main_line, clk_times, g.moves_txt, nbr_comments );
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
        if( !std::getline(in_aux, line) )
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
                std::vector<int> clk_times;
                std::string moves_txt;
                int nbr_comments;
                get_main_line( line, main_line, clk_times, moves_txt, nbr_comments );
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


 int cmd_refine_dups( std::ifstream &in, std::ofstream &out )
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
            std::vector<int> clk_times;
            std::string moves_txt;
            int nbr_comments;
            get_main_line( line, main_line, clk_times, moves_txt, nbr_comments );
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

 int cmd_remove_exact_pairs( std::ifstream &in, std::ofstream &out )
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
 int cmd_collect_fide_id( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out )
{
    std::set<long> nz_fide_ids;
    std::string line;
    int line_nbr=0;
    unsigned int nbr_games_found=0;

    // Read the NZ fide ids
    for(;;)
    {
        if( !std::getline(in_aux, line) )
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
        if( !std::getline(in, line) )
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

//
//  Find NZ Players and NZ Tournaments
//

 int cmd_get_known_fide_id_games_plus( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out )
{
    std::set<long> nz_fide_ids;
    std::map<std::string,std::pair<int,int>> events;
    std::string line;
    int line_nbr=0;
    unsigned int nbr_games_found=0;

    // Read the NZ fide ids
    for(;;)
    {
        if( !std::getline(in_aux, line) )
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

    // Pass 1 find Events with NZ players
    line_nbr=0;
    for(;;)
    {
        if( !std::getline(in, line) )
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
        bool nz_game = false;
        bool ok  = key_find( header, "WhiteFideId", white_fide_id );
        if( ok )
        {
            long id = atol(white_fide_id.c_str());
            auto it = nz_fide_ids.find(id);
            if( it != nz_fide_ids.end() )
                nz_game = true;
        }
        if( !nz_game )
        {
            ok  = key_find( header, "BlackFideId", black_fide_id );
            if( ok )
            {
                long id = atol(black_fide_id.c_str());
                auto it = nz_fide_ids.find(id);
                if( it != nz_fide_ids.end() )
                    nz_game = true;
            }
        }
        if( nz_game )
        {
            std::string event;
            bool ok = key_find( header, "Event", event );
            if( ok )
            {
                auto it = events.find(event);
                if( it == events.end() )
                {
                    std::pair<int,int> zero = {0,0};
                    events[event] = zero;
                }
            }
        }
    }
    printf( "%zu Events with NZ players found\n", events.size() );

    // Pass 2 Count total games and games with NZ players in events
    line_nbr=0;
    in.clear();
    in.seekg(0);
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
        bool ok = key_find( header, "Event", event );
        if( ok )
        {
            auto it = events.find(event);
            if( it != events.end() )
            {
                it->second.first++;     // total games
                std::string white_fide_id;
                std::string black_fide_id;
                bool nz_game = false;
                bool ok  = key_find( header, "WhiteFideId", white_fide_id );
                if( ok )
                {
                    long id = atol(white_fide_id.c_str());
                    auto it = nz_fide_ids.find(id);
                    if( it != nz_fide_ids.end() )
                        nz_game = true;
                }
                if( !nz_game )
                {
                    ok  = key_find( header, "BlackFideId", black_fide_id );
                    if( ok )
                    {
                        long id = atol(black_fide_id.c_str());
                        auto it = nz_fide_ids.find(id);
                        if( it != nz_fide_ids.end() )
                            nz_game = true;
                    }
                }
                if( nz_game )
                    it->second.second++;      // total NZ games
            }
        }
    }

    // Pass 3 Output game if NZ player or NZ event
    line_nbr=0;
    in.clear();
    in.seekg(0);
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
        bool ok = key_find( header, "Event", event );
        if( ok )
        {
            auto it = events.find(event);
            if( it != events.end() )
            {
                // More than 50% NZ games ?
                bool nz_tournament = (it->second.second*2 > it->second.first);
                // printf( "Event %s (%d,%d) %s\n", it->first.c_str(), it->second.first, it->second.second, nz_tournament?"true":"false" );
                bool nz_game = false;
                if( !nz_tournament )
                {
                    std::string white_fide_id;
                    std::string black_fide_id;
                    bool ok  = key_find( header, "WhiteFideId", white_fide_id );
                    if( ok )
                    {
                        long id = atol(white_fide_id.c_str());
                        auto it = nz_fide_ids.find(id);
                        if( it != nz_fide_ids.end() )
                            nz_game = true;
                    }
                    if( !nz_game )
                    {
                        ok  = key_find( header, "BlackFideId", black_fide_id );
                        if( ok )
                        {
                            long id = atol(black_fide_id.c_str());
                            auto it = nz_fide_ids.find(id);
                            if( it != nz_fide_ids.end() )
                                nz_game = true;
                        }
                    }
                }
                if( nz_tournament || nz_game )
                {
                    util::putline(out,header+moves);
                    nbr_games_found++;
                }
            }
        }
    }

    printf( "%u games found\n", nbr_games_found );
    return 0;
}

/*
    If Event starts with "Round " copy BroadcastName -> Event
    If Date absent, create Date after Site using UTCDate
*/
int cmd_lichess_broadcast_improve( std::ifstream &in, std::ofstream &out )
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
        if( ok && ok2 && (util::prefix(event,"Round ") || util::prefix(event,"Game ")) )
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

static bool predicate_lt( const std::pair<long,std::string> &left,  const std::pair<long,std::string> &right )
{
    return left.first < right.first;
}

int cmd_get_name_fide_id( std::ifstream &in, std::ofstream &out )
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

int cmd_put_name_fide_id( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out )
{
    std::map<long,std::string> id_names;
    std::string line;
    int line_nbr=0;

    // Read id -> player file
    for(;;)
    {
        if( !std::getline(in_aux, line) )
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

int cmd_event( std::ifstream &in, std::ofstream &out, std::string &replace_event )
{
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
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
        util::putline(out,line);
    }
    return 0;
}

int cmd_hardwired( std::ifstream &in, std::ofstream &out )
{
    std::map<std::string,std::string> elo_lookup;
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
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
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
        util::putline(out,line);
    }
    return 0;
}

int cmd_teams( std::ifstream &in, std::ofstream &out, std::string &name_tsv, std::string &teams_csv )
{
    std::map<std::string,std::string> handle_team;
    std::map<std::string,std::string> handle_real_name;
    std::ifstream in_team(teams_csv.c_str());
    if( !in_team )
    {
        printf( "Error; Cannot open file %s for reading\n", teams_csv.c_str() );
        return -1;
    }
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
    std::ifstream in_pgn_name(name_tsv.c_str());
    if( !in_pgn_name )
    {
        printf( "Error; Cannot open file %s for reading\n", name_tsv.c_str() );
        return -1;
    }
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
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
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
        util::putline(out,line);
    }
    return 0;
}

int cmd_time( std::ifstream &in, std::ofstream &out )
{
    std::vector<MicroGame> mgs;
    printf( "Reading games\n" );
    for(;;)
    {
        std::string line;
        if( !std::getline(in, line) )
            break;
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
        util::putline(out,line);
    }
    return 0;
} 

int cmd_fide_id_report( std::ifstream &in, std::ofstream &out )
{
    int line_nbr=0;
    std::map<long,std::set<std::string>> m;
    std::string line;
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

        // Eval name and fideid tags
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( ok && name!="BYE" )
            {
                std::string fide_id;
                bool ok2  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
                long id = atol(fide_id.c_str());
                ok2 = ok2 && id!=0;
                if( ok2 )
                {
                    auto it = m.find(id);
                    if( it == m.end() )
                    {
                        std::set<std::string> st;
                        st.insert(name);
                        m[id] = st;
                    }
                    else
                    {
                        std::set<std::string> &st = it->second;
                        st.insert(name);
                    }
                }
            }
        }
    }
    std::vector<std::pair<long,std::set<std::string>>> v;
    for( auto it=m.begin(); it!=m.end(); it++ )
    {
        std::pair<long,std::set<std::string>> pr(it->first,it->second);
        v.push_back(pr);
    }
    std::sort(v.begin(),v.end());
    for( int i=0; i<2; i++ )
    {
        util::putline(out,i==0?"Collisions;":"Regular matches;");
        for( std::pair<long,std::set<std::string>> &pr: v )
        {
            std::set<std::string> &st = pr.second;
            int n = (int)st.size();
            bool show = true;
            if( i==0 )
                show = (n>1);       // collisions first time through
            else
                show = (n==1);      // regular matches second time through
            if( show )
            {
                std::string t = util::sprintf( "%9d: ", pr.first );
                bool first = true;
                for( std::string s: pr.second )
                {
                    if( n > 1 )
                        t += first?"[":"; ";
                    if( n>2 && first )
                        t += util::sprintf( "(%d)", n );
                    t += util::sprintf( "%s", s.c_str() );
                    first = false;
                }
                t += util::sprintf( "%s", n==1?"":"]" );
                util::putline(out,t);
            }
        }
    }
    return 0;
}

struct NameYear
{
    NameYear(const std::string &n, int y) {name=n; year=y;}
    std::string name;
    int year;
};

#define BASE_YEAR   1980       // 1980-2049
#define END_YEAR    2050
static int counts[END_YEAR-BASE_YEAR][END_YEAR-BASE_YEAR];    
static void collision_resolve( long id, const std::string &name, int year, NameYear &ny )
{
    if( name == ny.name )
        return;
    if( util::prefix(name,ny.name) )
        return;
    if( util::prefix(ny.name,name) )
    {
        if( util::suffix(ny.name," (2)" ) )
            ny.name = ny.name.substr(0,ny.name.length()-5);
        return;
    }
    counts[year-BASE_YEAR][ny.year-BASE_YEAR]++;
    printf( "Collision, id=%ld => discard %s(%d), keep %s(%d)\n", id, name.c_str(), year, ny.name.c_str(), ny.year );
}

// Combine fide-id files into one mega fide id file
int cmd_temp( std::ofstream &out )
{
    std::map<long,NameYear>  map_combined;
    for( int i=0; i<4; i++ )
    {
        int year = 2025;
        const char *s;
        switch(i)
        {
            default:
            case 0: s = "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2025.txt";
                        year = 2025;
                        break;
            case 1: s = "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2018.txt";
                        year = 2018;
                        break;
            case 2: s = "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-2000.txt";
                        year = 2000;
                        break;
            case 3: s = "c:/users/bill/documents/chess/nzl/2025/fide-ids-all-1992.txt";
                        year = 1992;
                        break;
        }
        std::ifstream in_fide_ids(s);
        if( !in_fide_ids )
        {
            printf( "Error; Cannot open file %s for reading\n", s );
            return -1;
        }
        printf( "Processing input file %s\n", s );
        int line_nbr=0;
        for(;;)
        {
            std::string line;
            if( !std::getline( in_fide_ids, line) )
                break;

            // Strip out UTF8 BOM mark (hex value: EF BB BF)
            if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                line = line.substr(3);
            line_nbr++;
            util::rtrim(line);
            if( line.length() > 15 )
            {
                std::string ids  = line.substr(0,15);
                std::string name = line.substr(15);
                util::trim(ids);
                long id = atol(ids.c_str());
                if( id > 0 )
                {
                    auto it = map_combined.find(id);
                    if( it != map_combined.end() )
                        collision_resolve( id,name, year, it->second );
                    else
                    {
                        NameYear ny(name,year);
                        std::pair<long,NameYear> pr(id,ny);
                        map_combined.insert( pr );
                    }
                }
            }
        }
    }
    std::vector< std::pair<long,std::string>> v;
    for( const std::pair<long,NameYear> &pr: map_combined )
    {
        v.push_back( std::pair<long,std::string>(pr.first,pr.second.name) );
    }
    printf( "Sort begin\n" );
    std::sort(v.begin(),v.end());
    printf( "Sort end\n" );
    printf( "Writing output\n" );
    std::string previous;
    for(  std::pair<long,std::string> &pr : v )
    {
        std::string s = util::sprintf( "%-9ld %s", pr.first, pr.second.c_str() );
        util::putline(out,s);
    }
    printf( "Collision report\n" );
    for( int i=BASE_YEAR; i<END_YEAR; i++ )
    {
        for( int j=BASE_YEAR; j<END_YEAR; j++ )
        {
            int count = counts[i-BASE_YEAR][j-BASE_YEAR];
            if( count > 0 )
                printf( "Rejected %d names in %d list due to collision with %d list\n", count, i, j );
        }
    }
    return 0;
}

// Adapted (just a little) from http://www.talkativeman.com/levenshtein-distance-algorithm-string-comparison/
static int lev_distance( const std::string source, const std::string target )
{
    // Step 1
    const int n = (int)source.length();
    const int m = (int)target.length();
    if( n == 0 )
    {
        return m;
    }
    if( m == 0 )
    {
        return n;
    }

    // Good form to declare a TYPEDEF
    typedef std::vector< std::vector<int> > matrix_t;
    matrix_t matrix(n+1);

    // Size the vectors in the 2nd dimension. Unfortunately C++ doesn't
    // allow for allocation on declaration of 2nd dimension of vec of vec
    for( int i=0; i<=n; i++ )
    {
        matrix[i].resize(m+1);
    }

    // Step 2
    for( int i=0; i<=n; i++ )
    {
        matrix[i][0]=i;
    }
    for( int j=0; j<=m; j++)
    {
        matrix[0][j]=j;
    }

    // Step 3
    for( int i=1; i<=n; i++ )
    {
        const char s_i = source[i-1];

        // Step 4
        for( int j=1; j<=m; j++ )
        {
            const char t_j = target[j-1];

            // Step 5
            int cost;
            if( s_i == t_j )
            {
                cost = 0;
            }
            else
            {
                cost = 1;
            }

            // Step 6
            const int above = matrix[i-1][j];
            const int left = matrix[i][j-1];
            const int diag = matrix[i-1][j-1];
            #define MIN(a,b) ((a)<=(b) ? (a) : (b))
            int temp = MIN( left+1, diag+cost );
            int cell = MIN( above+1, temp );

            // Step 6A: Cover transposition, in addition to deletion,
            // insertion and substitution. This step is taken from:
            // Berghel, Hal ; Roach, David : "An Extension of Ukkonen's
            // Enhanced Dynamic Programming ASM Algorithm"
            // (http://www.acm.org/~hlb/publications/asm/asm.html)
            if( i>2 && j>2 )
            {
                int trans = matrix[i-2][j-2]+1;
                if( source[i-2] != t_j )
                    trans++;
                if( s_i != target[j-2] )
                    trans++;
                if( cell>trans )
                    cell=trans;
            }
            matrix[i][j]=cell;
        }
    }

    // Step 7
    return matrix[n][m];
}

int cmd_fide_id_to_name( std::ifstream &in_aux_fide, std::ifstream &in_aux_keep, std::ifstream &in_aux_custom,
                         std::ifstream &in, std::ofstream &out )
{
    // Stage 1:
    // 
    // Read the set of fide_ids we need from game file
    //
    std::set<long> fide_ids;
    int line_nbr=0;
    std::string line;

    // Find fide-ids line by line
    line_nbr=0;
    for(;;)
    {
        if( !std::getline(in,line) )
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
        for( int i=0; i<2; i++ )
        {
            std::string fide_id;
            bool ok  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
            long id = atol(fide_id.c_str());
            ok = ok && id>0;
            if( ok )
                fide_ids.insert(id);
        }
    }

    // Stage 2:
    // 
    // Create map of fide-id to name(s) for each fide id we need
    // Also maps of sacrosanct pairs and custom replacements
    //
    std::map<long,std::string> fide_id_names;
    std::map<long,std::string> fide_id_names_to_keep;
    std::map<long,std::string> fide_id_names_custom;
    for( int fide_file=0; fide_file<3; fide_file++ )
    {
        std::map<long,std::string> &m =
                      (fide_file==0 ? fide_id_names
                                    : (fide_file==1?fide_id_names_to_keep:fide_id_names_custom)
                      );
        std::ifstream &in_fide =
                      (fide_file==0 ? in_aux_fide
                                    : (fide_file==1?in_aux_keep:in_aux_custom)
                      );
        line_nbr=0;
        for(;;)
        {
            if( !std::getline(in_fide,line) )
                break;

            // Strip out UTF8 BOM mark (hex value: EF BB BF)
            if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                line = line.substr(3);
            line_nbr++;
            util::rtrim(line);
            util::ltrim(line);
            long id = atol(line.c_str());
            auto it = fide_ids.find(id);
            if( id>0 && it!=fide_ids.end() )
            {
                size_t offset = line.find(' ');
                if( offset != std::string::npos )
                {
                    offset = line.find_first_not_of(' ',offset);
                    if( offset != std::string::npos )
                    {
                        std::string name = line.substr(offset);
                        util::rtrim(name);
                        m[id] = name;
                    }
                }
            }
        }
    }

    //
    // Stage 3: Output file with updated names
    //
    in.clear();
    in.seekg(0);
    line_nbr=0;
    std::set<std::string> warnings;
    std::vector<std::string> errors;
    std::vector<std::string> customs;
    std::vector<std::string> skips;
    std::vector<std::string> unknowns;
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

        // Eval name and fideid tags
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( !ok || name=="BYE" )
                continue;
            std::string fide_id;
            ok  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
            if( !ok )
                continue;
            long id = atol(fide_id.c_str());
            if( id <= 0 )
                continue;
            auto it2 = fide_id_names_to_keep.find(id);
            if( it2 != fide_id_names_to_keep.end() && it2->first==id && it2->second==name )
            {
                std::string s = util::sprintf( "SKIPPING: Player (%s; %ld), not updated\n", name.c_str(), id );
                auto iit = warnings.find(s);
                if( iit == warnings.end() )
                {
                    warnings.insert(s);
                    skips.push_back(s);
                }
                continue;
            }
            it2 = fide_id_names_custom.find(id);
            if( it2 != fide_id_names_custom.end() && it2->first==id )
            {
                std::string replacement = it2->second;
                key_update( header, i==0?"White":"Black", "Result", replacement );
                if( name != replacement )
                {
                    key_update2( header, i==0?"WhiteOriginalName":"BlackOriginalName", i==0?"Black":"WhiteOriginalName", "Black", replacement );
                }
                std::string s = util::sprintf( "CUSTOM NAME, %ld replacing %s by %s\n", id, name.c_str(), replacement.c_str() );
                auto iit = warnings.find(s);
                if( iit == warnings.end() )
                {
                    warnings.insert(s);
                    customs.push_back(s);
                }
                continue;
            }
            auto it = fide_id_names.find(id);
            bool found = (it != fide_id_names.end());
            if( !found )
            {
                std::string s = util::sprintf( "UNKNOWNS: Player (%s; %ld), id not found in fide file\n", name.c_str(), id );
                auto iit = warnings.find(s);
                if( iit == warnings.end() )
                {
                    warnings.insert(s);
                    unknowns.push_back(s);
                }
                continue;
            }    
            std::string replacement = it->second;
            key_update( header, i==0?"White":"Black", "Result", replacement );
            int distance = lev_distance( name, replacement );
            if( distance > 2 )
            {
                std::string s = util::sprintf( "WARNING, %ld replacing %s by %s (distance %d)\n", id, name.c_str(), replacement.c_str(), distance );
                auto iit = warnings.find(s);
                if( iit == warnings.end() )
                {
                    printf("%s",s.c_str() );
                    warnings.insert(s);
                    std::string lhs = util::tolower(name);
                    if( lhs.length() > 4 )
                        lhs = lhs.substr( 0, 4 );
                    std::string rhs = util::tolower(replacement);
                    if( rhs.length() > 4 )
                        rhs = rhs.substr( 0, 4 );
                    if( distance>8 && lhs!=rhs )
                    {
                        std::string t = util::sprintf( "ERROR (suspected), %ld replacing %s by %s (distance %d)\n", id, name.c_str(), replacement.c_str(), distance );
                        errors.push_back(t);
                    }
                }
            }
            if( name != replacement )
            {
                key_update2( header, i==0?"WhiteOriginalName":"BlackOriginalName", i==0?"Black":"WhiteOriginalName", "Black", replacement );
            }
        }

        // Write updated output
        util::putline(out,header+moves);       
    }
    for( std::string &s: unknowns )
        printf( "%s", s.c_str() );
    for( std::string &s: errors )
        printf( "%s", s.c_str() );
    for( std::string &s: skips )
        printf( "%s", s.c_str() );
    for( std::string &s: customs )
        printf( "%s", s.c_str() );
    printf( "%zd unique warnings including %zd unknown ids and %zd suspected errors and %zd skips and %zd custom names from %zd unique replacements\n", warnings.size(), unknowns.size(), errors.size(), skips.size(), customs.size(), fide_ids.size() );
    return 0;
}

int cmd_normalise( std::ifstream &in, std::ofstream &out )
{
    int line_nbr=0;
    std::string line;

    // Find names line by line
    line_nbr=0;
    std::vector<std::pair<long,std::string>> v;
    for(;;)
    {
        if( !std::getline(in,line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;

        // Get id and name
        util::rtrim(line);
        util::ltrim(line);
        long id = atol(line.c_str());
        auto offset = line.find_first_of(" \t");
        if( offset == std::string::npos )
            continue;
        offset = line.find_first_not_of(" \t",offset);
        if( id==0 || offset == std::string::npos )
            continue;
        std::string name = line.substr(offset);
        std::pair<long,std::string> pr(id,name);
        v.push_back(pr);
    }
    std::sort( v.begin(), v.end() );
    for( std::pair<long,std::string> &pr: v )
    {
        std::string s = util::sprintf( "%-9ld  %s", pr.first, pr.second.c_str() );
        util::putline( out, s );
    }
    return 0;
}
