// lichess-fixup.cpp
//
//    One of the supplementary functions, made standalone, for PGN rather than LPGN
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

bool core_pgn2line( std::string fin, std::vector<std::string> &vout );
bool core_line2pgn( std::vector<std::string> &vin, std::string fout );
static int func_add_ratings( std::ifstream &in1, std::vector<std::string> &in2, std::vector<std::string> &out );
static int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt, int &nbr_comments );
#define GET_MAIN_LINE_MASK_HAS_VARIATIONS 1
#define GET_MAIN_LINE_MASK_HAS_COMMENTS 2
#define GET_MAIN_LINE_MASK_ALL_COMMENTS_AUTO 4
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

int main( int argc, const char **argv )
{
#ifdef _DEBUG
    const char *args[] =
    {
        "dontcare.exe",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/anzac-2023.txt",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/anzac-2023.pgn",
        "C:/Users/Bill/Documents/ChessDatabase/nz-database/work-2023/lichess/anzac-2023-out.pgn"
    };
    argc = sizeof(args)/sizeof(args[0]);
    argv = args;
#endif
    bool ok = (argc==4);
    if( !ok )
    {
        printf( 
            "Fix Lichess names and add ratings\n"
            "Usage:\n"
            "lichess-fixup ratings.txt in.pgn out.pgn\n"
            "ratings.txt format example, (starts with Event, Site)\n"
            "Wellington Open\n"
            "Wellington\n"
            "Dive, Russell   2250\n"
            "Forster, Bill   1900\n"
            "...\n"
        );
        return -1;
    }
    std::string fin_ratings(argv[1]);
    std::string fin(argv[2]);
    std::string fout(argv[3]);
    std::ifstream in_ratings(fin_ratings.c_str());
    if( !in_ratings )
    {
        printf( "Error; Cannot open file %s for reading\n", fin_ratings.c_str() );
        return -1;
    }
    std::vector<std::string> lpgn_games_in;
    std::vector<std::string> lpgn_games_out;
    ok = core_pgn2line( fin, lpgn_games_in );
    if( !ok )
        return -1;
    func_add_ratings( in_ratings, lpgn_games_in, lpgn_games_out );
    core_line2pgn( lpgn_games_out, fout );
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

static int func_add_ratings( std::ifstream &in1, std::vector<std::string> &in2, std::vector<std::string> &out )
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
    for( std::string line: in2 )
    {
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
        out.push_back(header+normal);
    }
    return 0;
}

