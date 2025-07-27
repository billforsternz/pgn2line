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

struct PlayerDetails
{
    std::string name;
    long id  = 0;
    int  nbr_games=0;
    int  nbr_games_with_id=0;
    int  nbr_games_with_other_ids=0;
    int  first_year=0;
    int  last_year=0;
    char match_tier= ' ';   // '0' = not matched, 'A'=best, 'B'=next best etc
    std::set<long> other_ids;
    bool operator < (const PlayerDetails &lhs ) { return nbr_games < lhs.nbr_games; }
};

struct NameSplit
{
    std::string name;
    std::string lower;
    std::string lower_no_period;
    std::string surname_forename_initial;
    std::string surname_forename;
    std::string surname_initial;
    std::string surname;
};

struct Triple
{
    std::string name;
    int id;
    int count = 1;
};

void split_name( const std::string &name, NameSplit &out )
{                   
    out.name = name;
    std::string s = util::tolower(name);
    size_t len = s.length();
    out.lower = s;
    out.lower_no_period = (len>0 && s[len-1] == '.')
        ? s.substr(0,len-1)
        : s;
    s = out.lower_no_period;
    out.surname_forename_initial.clear();
    out.surname_forename.clear();
    out.surname_initial.clear();
    out.surname.clear();
    size_t offset = s.find_first_of(" ,");
    if( offset != std::string::npos )
    {
        out.surname = s.substr(0,offset);
        if( (s[offset]==' ') && (out.surname=="van" || out.surname=="mc" || out.surname=="du" || out.surname=="de") )
        {
            offset = s.find_first_not_of(' ',offset);
            if( offset != std::string::npos )
            {
                offset = s.find_first_of(" ,",offset);
                if( offset != std::string::npos )
                {
                    out.surname = s.substr(0,offset);
                    if( s[offset]==' ' && out.surname=="van der" )
                    {
                        offset = s.find_first_not_of(' ',offset);
                        if( offset != std::string::npos )
                        {
                            offset = s.find_first_of(" ,",offset);
                            if( offset != std::string::npos )
                                out.surname = s.substr(0,offset);
                        }
                    }
                }
            }
        }
    }
    if( offset != std::string::npos )
    {
        offset = s.find_first_not_of(" ,",offset);
        if( offset != std::string::npos )
        {
            out.surname_initial = out.surname + ", " + s[offset];
            out.surname_forename = out.surname + ", " + s.substr(offset);
            size_t offset2 = s.find_first_of(' ',offset);
            if( offset2 != std::string::npos )
            {
                std::string forename = s.substr(offset,offset2-offset);
                out.surname_forename = out.surname + ", " + forename;
                size_t offset3 = s.find_first_not_of(' ',offset2);
                if( offset3 != std::string::npos )
                    out.surname_forename_initial  = out.surname_forename + " " + s[offset3];
            }
        }
    }
}    

int cmd_get_name_fide_id_plus( std::ifstream &in_aux, std::ifstream &in, std::ofstream &out )
{
    std::map<std::string,Triple> map_name;
    std::map<std::string,Triple> map_lower;
    std::map<std::string,Triple> map_lower_no_period;
    std::map<std::string,Triple> map_surname_forename_initial;
    std::map<std::string,Triple> map_surname_forename;
    std::map<std::string,Triple> map_surname_initial;

    // Read the NZ fide ids
    int nbr_nz_fide_ids = 0;
    std::string line;
    int line_nbr=0;
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
        {
            size_t offset = line.find(' ');
            if( offset != std::string::npos )
            {
                offset = line.find_first_not_of(' ',offset);
                if( offset != std::string::npos )
                {
                    nbr_nz_fide_ids++;
                    std::string name = line.substr(offset);
                    util::rtrim(name);
                    Triple triple;
                    triple.name = name;
                    triple.id   = id;
                    NameSplit ns;
                    split_name(name,ns);
                    #if 0
                    static int count=10;
                    if( count>0 )
                    {
                        count--;
                        printf( "input=%s\n"
                            " name=%s\n"
                            " lower=%s\n"
                            " lower_no_period=%s\n"
                            " surname_forename_initial=%s\n"
                            " surname_forename=%s\n"
                            " surname_initial=%s\n"
                            " surname=%s\n",
                            name.c_str(),
                            ns.name.c_str(),
                            ns.lower.c_str(),
                            ns.lower_no_period.c_str(),
                            ns.surname_forename_initial.c_str(),
                            ns.surname_forename.c_str(),
                            ns.surname_initial.c_str(),
                            ns.surname.c_str()
                        );
                    }
                    #endif
                    for( int i=0; i<6; i++ )
                    {
                        std::map<std::string,Triple> *p = &map_name;                    
                        std::string q;
                        switch(i)
                        {
                            case 0: p = &map_name;
                                    q =  ns.name;
                                    break;
                            case 1: p = &map_lower;
                                    q =  ns.lower;
                                    break;
                            case 2: p = &map_lower_no_period;
                                    q =  ns.lower_no_period;
                                    break;
                            case 3: p = &map_surname_forename_initial;
                                    q =  ns.surname_forename_initial;
                                    break;
                            case 4: p = &map_surname_forename;
                                    q =  ns.surname_forename;
                                    break;
                            case 5: p = &map_surname_initial;
                                    q =  ns.surname_initial;
                                    break;
                        }
                        if( q.length() > 0 )
                        {
                            auto it = p->find(q);
                            if( it == p->end() )
                                (*p)[q] = triple;
                            else
                                it->second.count++;
                        }
                    }
                }
            }
        }
    }
    printf( "%d NZ fide ids read from file\n", nbr_nz_fide_ids );

    std::map<std::string,PlayerDetails> players;
    line_nbr=0;

    // Find names line by line
    line_nbr=0;
    unsigned int nbr_names_replaced = 0;
    int total_names=0, total_names_with_fide_ids = 0;
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
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( ok && name!="BYE" )
            {
                total_names++;
                auto it = players.find(name);
                if( it == players.end() )
                {
                    PlayerDetails pd;
                    players[name] = pd;
                    it = players.find(name);
                }    
                it->second.name = name;
                it->second.nbr_games++;
                std::string date;
                bool ok2 = key_find(header,"Date",date);
                if( ok2 )
                {
                    int year = atoi( date.c_str() );
                    if( year > 1000 )
                    {
                        if( it->second.first_year == 0 || year < it->second.first_year )
                            it->second.first_year = year;
                        if( it->second.last_year == 0 || year > it->second.last_year)
                            it->second.last_year = year;
                    }
                }
                std::string fide_id;
                ok2  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
                long id = atol(fide_id.c_str());
                ok2 = ok2 && id!=0;
                if( ok2 )
                {
                    total_names_with_fide_ids++;
                    if( it->second.id == 0 )
                    {
                        it->second.id = id;                   
                        it->second.nbr_games_with_id = 1;
                    }
                    else
                    {
                        if( it->second.id == id )                    
                        {
                            it->second.nbr_games_with_id++;
                        }
                        else
                        {
                            printf( "Multiple FIDE ids for name=%s, fide_id=%s, id=%ld\n", name.c_str(), fide_id.c_str(), id );
                            it->second.nbr_games_with_other_ids++;
                            auto it2 = it->second.other_ids.find(id);
                            if( it2 == it->second.other_ids.end() )
                                it->second.other_ids.insert(id);
                        }    
                    }
                }
            }
        }
    }
    printf( "%d names, %d with fide_ids %.1f%%\n",
        total_names, total_names_with_fide_ids, (total_names_with_fide_ids*100.0) / ((total_names?total_names:1)*1.0) );
    printf( "%zu players, writing details to file\n", players.size() );
    std::vector<PlayerDetails> v;
    for( std::pair<std::string,PlayerDetails> pr: players )
        v.push_back(pr.second);
    std::sort( v.rbegin(), v.rend() );

    int total_names_ext=0, total_names_with_fide_ids_ext=0;
    for( const PlayerDetails &pd: v )
    {
        total_names_ext++;
        if( pd.nbr_games_with_id!=0 && pd.nbr_games_with_other_ids==0 )
            total_names_with_fide_ids_ext++;
    }
    printf( "After back-porting inline fide ids, %d names, %d with fide_ids %.1f%%\n",
        total_names, total_names_with_fide_ids_ext, (total_names_with_fide_ids_ext*100.0) / ((total_names_ext?total_names_ext:1)*1.0) );
    for( PlayerDetails &pd2: v )
    {
        auto jt = players.find(pd2.name);
        if( jt == players.end() )
        {
            printf( "WARNING, unexpected miss\n");
            continue;
        }
        PlayerDetails &pd = jt->second;
        pd.match_tier = ' ';
        int infer = 0;
        long infer_id;
        std::string infer_string;
        std::string infer_name;
        int         infer_count;
        std::string line = util::sprintf("%-30s", pd.name.c_str() );
        if( pd.nbr_games_with_id!=0 && pd.nbr_games_with_other_ids==0 )
        {
            line += util::sprintf( "%9ld", pd.id );
            pd.match_tier = 'A';
        }
        else
        {
            NameSplit ns;
            split_name( pd.name, ns );
            for( int i=0; i<6; i++ )
            {
                std::map<std::string,Triple> *p = &map_name;                    
                std::string q;
                switch(i)
                {
                    case 0: p = &map_name;
                            q =  ns.name;
                            infer_string = "complete name match";
                            break;
                    case 1: p = &map_lower;
                            q =  ns.lower;
                            infer_string = "case insensitive match";
                            break;
                    case 2: p = &map_lower_no_period;
                            q =  ns.lower_no_period;
                            infer_string = "case insensitive match after removing period";
                            break;
                    case 3: p = &map_surname_forename_initial;
                            q =  ns.surname_forename_initial;
                            infer_string = "surname plus forename plus initial match";
                            break;
                    case 4: p = &map_surname_forename;
                            q =  ns.surname_forename;
                            infer_string = "surname plus forename match";
                            break;
                    case 5: p = &map_surname_initial;
                            q =  ns.surname_initial;
                            infer_string = "surname plus initial match";
                            break;
                }
                if( q.length() > 0 )
                {
                    auto it = p->find(q);
                    if( it != p->end() )
                    {
                        infer = i+1;
                        infer_id     = it->second.id;
                        infer_name   = it->second.name;
                        infer_count  = it->second.count;
                        if( i== 5 ) // surname_initial
                        {
                            // Important exception,
                            //  "Smith, G" should (weakly) match "Smith, Garry"  BUT
                            //  "Smith, Gordon" should not match "Smith, Garry" through surname initial
                            NameSplit potential;
                            split_name( infer_name, potential );
                            if( ns.surname_forename!="" || potential.surname_forename!="")
                                infer = 0;  // Kill "Smith, Gordon" and "Smith, Garry"
                        }
                        break;
                    }
                }
            }
            if( !infer )
                line += util::sprintf("%9s"," " );
            else
            {
                pd.match_tier = 'A' + infer;  // inferred match tiers start at 'B'
                line += util::sprintf( "%9ld", infer_id );
            }
        }
        line += util::sprintf("  %d game%s",
            pd.nbr_games,
            pd.nbr_games==1 ? "" : "s"
        );
        line += pd.first_year == pd.last_year
                ? util::sprintf( ", (%d)", pd.first_year )
                : util::sprintf( ", (%d-%d)",
                    pd.first_year,
                    pd.last_year );
        if( pd.nbr_games_with_id != 0 )
        {
            line += util::sprintf( " %d game%s with id %ld",
                pd.nbr_games_with_id,
                pd.nbr_games_with_id==1 ? "" : "s",
                pd.id
            );
            if( pd.nbr_games_with_other_ids != 0 )
            {
                line += util::sprintf( " %d game%s with %zu other ids",
                    pd.nbr_games_with_other_ids,
                    pd.nbr_games_with_other_ids==1 ? "" : "s",
                    pd.other_ids.size()
                );
                bool first = true;
                for( long id: pd.other_ids )
                {
                    line += util::sprintf( "%s%ld", first?"(":",", id );
                    first = false;
                }
                line += util::sprintf( ")" );
            }
        }
        if( infer )
        {
            std::string confidence = "No clashes";
            if( infer_count > 1 )
                confidence = util::sprintf( "%d clashes", infer_count );
            line += util::sprintf(" %s %s %s",
                        infer_string.c_str(),
                        infer_name.c_str(),
                        confidence.c_str()
                    );
        }
        util::putline(out,line);
    }

    // Calculate number of games with FIDE-ids, before and after
    in.clear();
    in.seekg(0);
    line_nbr=0;
    int nbr_games = 0;

    // Number of games with 0, 1 or 2 fide-ids natively included
    int nbr_games_0_native = 0; 
    int nbr_games_1_native = 0;
    int nbr_games_2_native = 0; 

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'a' 
    int nbr_games_0_a = 0; 
    int nbr_games_1_a = 0;
    int nbr_games_2_a = 0; 

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'b' or better
    int nbr_games_0_b = 0;
    int nbr_games_1_b = 0;
    int nbr_games_2_b = 0;

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'c' or better
    int nbr_games_0_c = 0;
    int nbr_games_1_c = 0;
    int nbr_games_2_c = 0;

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'd' or better
    int nbr_games_0_d = 0;
    int nbr_games_1_d = 0;
    int nbr_games_2_d = 0;

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'e' or better
    int nbr_games_0_e = 0;
    int nbr_games_1_e = 0;
    int nbr_games_2_e = 0;

    // Number of games with 0, 1 or 2 fide-ids including infered ids, level 'f' or better
    int nbr_games_0_f = 0;
    int nbr_games_1_f = 0;
    int nbr_games_2_f = 0;
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
        int nbr_fide_ids_in_game_native = 0;
        int nbr_fide_ids_in_game_a = 0;
        int nbr_fide_ids_in_game_b = 0;
        int nbr_fide_ids_in_game_c = 0;
        int nbr_fide_ids_in_game_d = 0;
        int nbr_fide_ids_in_game_e = 0;
        int nbr_fide_ids_in_game_f = 0;

        // Eval name and fideid tags
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( ok && name!="BYE" )
            {
                char match_tier = ' ';
                std::string fide_id;
                bool ok2  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
                long id = atol(fide_id.c_str());
                ok2 = ok2 && id!=0;
                if( ok2 )
                    match_tier = 'N';   // native fide-id, not so much a match as an actual fide id
                else
                {
                    auto it = players.find(name);
                    if( it == players.end() )
                    {
                        printf( "WARNING: Player name %s, not found second time through\n", name.c_str() );
                        continue;
                    }    
                    match_tier = it->second.match_tier; // can be ' ' = not matched
                    if( match_tier == 'B' )
                    {
                        static int count = 4;
                        if( count > 0 )
                        {
                            count--;
                            printf( "player=%s, id=%ld, header=%s\n",
                                it->second.name.c_str(), 
                                it->second.id,
                                header.c_str() );
                        }
                    }
                }
                switch( match_tier )
                {
                    case 'N':   nbr_fide_ids_in_game_native++;  // fall through
                    case 'A':   nbr_fide_ids_in_game_a++;       //  ...
                    case 'B':   nbr_fide_ids_in_game_b++;       //
                    case 'C':   nbr_fide_ids_in_game_c++;       //  (eg if 'C' criteria met, 'D','E'... also met)
                    case 'D':   nbr_fide_ids_in_game_d++;       //
                    case 'E':   nbr_fide_ids_in_game_e++;       
                    case 'F':   nbr_fide_ids_in_game_f++;       
                    default:    ;
                }
            }
        }

        // Update stats
        nbr_games++;
        nbr_games_0_native += (nbr_fide_ids_in_game_native==0 ? 1 : 0); 
        nbr_games_1_native += (nbr_fide_ids_in_game_native==1 ? 1 : 0);
        nbr_games_2_native += (nbr_fide_ids_in_game_native==2 ? 1 : 0); 

        nbr_games_0_a += (nbr_fide_ids_in_game_a==0 ? 1 : 0); 
        nbr_games_1_a += (nbr_fide_ids_in_game_a==1 ? 1 : 0);
        nbr_games_2_a += (nbr_fide_ids_in_game_a==2 ? 1 : 0); 

        nbr_games_0_b += (nbr_fide_ids_in_game_b==0 ? 1 : 0);
        nbr_games_1_b += (nbr_fide_ids_in_game_b==1 ? 1 : 0);
        nbr_games_2_b += (nbr_fide_ids_in_game_b==2 ? 1 : 0);

        nbr_games_0_c += (nbr_fide_ids_in_game_c==0 ? 1 : 0);
        nbr_games_1_c += (nbr_fide_ids_in_game_c==1 ? 1 : 0);
        nbr_games_2_c += (nbr_fide_ids_in_game_c==2 ? 1 : 0);

        nbr_games_0_d += (nbr_fide_ids_in_game_d==0 ? 1 : 0);
        nbr_games_1_d += (nbr_fide_ids_in_game_d==1 ? 1 : 0);
        nbr_games_2_d += (nbr_fide_ids_in_game_d==2 ? 1 : 0);

        nbr_games_0_e += (nbr_fide_ids_in_game_e==0 ? 1 : 0);
        nbr_games_1_e += (nbr_fide_ids_in_game_e==1 ? 1 : 0);
        nbr_games_2_e += (nbr_fide_ids_in_game_e==2 ? 1 : 0);

        nbr_games_0_f += (nbr_fide_ids_in_game_f==0 ? 1 : 0);
        nbr_games_1_f += (nbr_fide_ids_in_game_f==1 ? 1 : 0);
        nbr_games_2_f += (nbr_fide_ids_in_game_f==2 ? 1 : 0);
    }

    // Number of games with 0, 1 or 2 fide-ids natively included
    printf( "Number of games with 0, 1 or 2 fide-ids natively included\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_native,
                                     (nbr_games_0_native*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_native,
                                     (nbr_games_1_native*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_native,
                                     (nbr_games_2_native*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_native*2 + nbr_games_1_native)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        

    // Number of games with 0, 1 or 2 fide-ids if we include tier A matches
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier A matches\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_a,
                                     (nbr_games_0_a*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_a,
                                     (nbr_games_1_a*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_a,
                                     (nbr_games_2_a*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_a*2 + nbr_games_1_a)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        

    // Number of games with 0, 1 or 2 fide-ids if we include tier B matches or better
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier B matches or better\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_b,
                                     (nbr_games_0_b*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_b,
                                     (nbr_games_1_b*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_b,
                                     (nbr_games_2_b*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_b*2 + nbr_games_1_b)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        

    // Number of games with 0, 1 or 2 fide-ids if we include tier C matches or better
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier C matches or better\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_c,
                                     (nbr_games_0_c*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_c,
                                     (nbr_games_1_c*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_c,
                                     (nbr_games_2_c*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_c*2 + nbr_games_1_c)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        


    // Number of games with 0, 1 or 2 fide-ids if we include tier D matches or better
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier D matches or better\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_d,
                                     (nbr_games_0_d*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_d,
                                     (nbr_games_1_d*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_d,
                                     (nbr_games_2_d*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_d*2 + nbr_games_1_d)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        

    // Number of games with 0, 1 or 2 fide-ids if we include tier E matches or better
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier E matches or better\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_e,
                                     (nbr_games_0_e*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_e,
                                     (nbr_games_1_e*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_e,
                                     (nbr_games_2_e*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_e*2 + nbr_games_1_e)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        

    // Number of games with 0, 1 or 2 fide-ids if we include tier F matches or better
    printf( "Number of games with 0, 1 or 2 fide-ids if we include tier F matches or better\n" );
    printf( " 0: %d, %.1f percent\n", nbr_games_0_f,
                                     (nbr_games_0_f*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 1: %d, %.1f percent\n", nbr_games_1_f,
                                     (nbr_games_1_f*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( " 2: %d, %.1f percent\n", nbr_games_2_f,
                                     (nbr_games_2_f*100.0) / ((nbr_games?nbr_games:1)*1.0) );
    printf( "Penetration = %.1f%%\n", (nbr_games_2_f*2 + nbr_games_1_f)*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        
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

// Turn players-list-foa.txt into fide id + player name only
int cmd_temp( std::ifstream &in, std::ofstream &out )
{
    std::string line;
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
        if( line.length() > 70 )
            line = line.substr(0,70);
        util::rtrim(line);
        util::putline(out,line);
    }
    return 0;
}

