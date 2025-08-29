// supplementary.cpp : Do something special to LPGN file
//  lichess-utils.cpp, Misc lichess file related stuff

#include "..\util.h"
#include "..\thc.h"
#include "key.h"
#include "baby.h"
#include "lichess_utils.h"

// Do some baby_clk stuff
static int baby_nbr_tests_passed;
static size_t baby_total_moves;
static size_t baby_total_length;
static int baby_nbr_tests_failed;
static int baby_nbr_tests_aborted;

std::string lichess_moves_to_normal_pgn_extra_stats()
{
    std::string s = util::sprintf(
        "baby_nbr_tests_passed=%d\n"
        "baby_total_moves=%zu\n"
        "baby_total_length=%zu\n"
        "avg move length=%.5f\n"
        "avg string length=%.2f\n"
        "baby_nbr_tests_failed=%d\n"
        "baby_nbr_tests_aborted=%d\n",
        baby_nbr_tests_passed,
        baby_total_moves,
        baby_total_length,
        (1.0*baby_total_length) / (1.0*(baby_total_moves?baby_total_moves:1)),
        (1.0*baby_total_length) / (1.0*(baby_nbr_tests_passed?baby_nbr_tests_passed:1)),
        baby_nbr_tests_failed,
        baby_nbr_tests_aborted );
    s += baby_stats_extra();
    return s;
}

int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, std::string &encoded_clk_times, int &nbr_comments )
{
    std::vector<std::string> main_line;
    std::vector<int> clk_times;
    std::string moves_txt;
    int mask = get_main_line( moves, main_line, clk_times, moves_txt, nbr_comments );
    if( main_line.size() == clk_times.size() )
    {
        baby_total_moves += main_line.size();
        baby_clk_encode( clk_times, encoded_clk_times );
        std::vector<int> temp;
        baby_clk_decode( encoded_clk_times, temp );
        if( clk_times == temp )
        {
            // static int count = 20;
            // if( count > 0 )
            // {
            //     count--;
            //     printf( "%d: %s\n", 20-count, encoded_clk_times.c_str() );
            // }
            baby_nbr_tests_passed++;
            baby_total_length += encoded_clk_times.length();
        }
        else
        {
            baby_nbr_tests_failed++;
            int len = (int)clk_times.size();
            int len2 = (int)temp.size();
            printf( "BabyClk encoding test fails, whoops: %d %d %s\n", len, len2, encoded_clk_times.c_str() );
            printf( "lpgn line;\n%s%s\n", header.c_str(), moves.c_str() );
            for( int i=0; i<len && i<len2; i++ )
            {
                printf( "0x%06x, 0x%06x %s\n", clk_times[i], temp[i], clk_times[i]==temp[i] ? "pass" : "fail");
            }
            exit(-1);
        }
    }
    else
    {
        if( clk_times.size() > 0 )
        {
            baby_nbr_tests_aborted++;
            printf( "Unexpectedly cannot do BabyClk encoding %zd %zd\n",
                        main_line.size(), clk_times.size() );
            if( main_line.size()==23 && clk_times.size()==22 )
                printf( "lpgn line;\n%s%s\n", header.c_str(), moves.c_str() );
        }
    }

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

int get_main_line( const std::string &s, std::vector<std::string> &main_line, std::vector<int> &clk_times, std::string &moves_txt, int &nbr_comments )
{
    std::string comment;
    nbr_comments = 0;
    int mask=0;
    main_line.clear();
    clk_times.clear();
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
    bool clk_comments = false;  // true if there are clock comments
    int  clk_count = -1;        // add a clock value after every move
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
                comment.push_back(c);
                if( c == '%' )
                    auto_comment = true;
                else if( c == '}' )
                {
                    state = save_state;
                    bool lichess_inaccuracy = util::prefix(comment, " Inaccuracy");
                    bool lichess_mistake    = util::prefix(comment, " Mistake");
                    bool lichess_blunder    = util::prefix(comment, " Blunder");
                    bool lichess_checkmate  = util::prefix(comment, " Checkmate is");
                    bool lichess_checkmate2 = util::prefix(comment, " Lost forced checkmate sequence");
                    bool lichess_result1    = util::prefix(comment, " 1-0 White wins");
                    bool lichess_result2    = util::prefix(comment, " 0-1 Black wins");
                    bool lichess_result3    = util::prefix(comment, " 1/2-1/2 Draw");
                    bool lichess_auto = lichess_inaccuracy ||
                                        lichess_mistake    ||
                                        lichess_blunder    ||
                                        lichess_checkmate  ||
                                        lichess_checkmate2 ||
                                        lichess_result1    ||
                                        lichess_result2    ||
                                        lichess_result3;
                    auto_comment = (auto_comment || lichess_auto);
                    if( !auto_comment )
                    {
                        only_auto_comments = false;
                    }
                    else
                    {
                        size_t offset = comment.find("%clk");
                        if( offset != std::string::npos )
                        {
                            int hhmmss = 0;
                            std::string s;
                            bool err = false;
                            for( int i=0; i<3; i++ )
                            {
                                offset = comment.find_first_of("0123456789",offset);
                                if( offset == std::string::npos )
                                    err = true;
                                else
                                {
                                    size_t offset2 = comment.find(i==2?']':':',offset);
                                    if( offset == std::string::npos )
                                        err = true;
                                    else
                                    {
                                        s = comment.substr(offset,offset2-offset);
                                        offset = offset2;
                                    }
                                }
                                if( !err )
                                {
                                    int hh_or_mm_or_ss = atoi(s.c_str());
                                    switch(i)
                                    {
                                        case 0: hhmmss  = ((hh_or_mm_or_ss<<16) & 0xff0000);  break;
                                        case 1: hhmmss |= ((hh_or_mm_or_ss<<8) & 0x00ff00);   break;
                                        case 2: hhmmss |= (hh_or_mm_or_ss & 0x0000ff);        break;
                                    }
                                }
                            }
                            clk_times.push_back( err? -1 :hhmmss );
                            clk_count++;
                            clk_comments = true;
                        }
                    }
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
                comment.clear();
            }
            else if( old_state == in_move )
            {
                if( clk_count == 0 )
                    clk_times.push_back( -1 );  // if we haven't pushed a clk time since last move
                main_line.push_back(move);
                moves_txt += " ";
                moves_txt += move;
                clk_count = 0;
            }
        }
    }
    if( !clk_comments )
        clk_times.clear();
    else if( clk_count == 0 && main_line.size() == clk_times.size()+1 )
        clk_times.push_back( -1 );  // if we haven't pushed a clk time since last move
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

// Simplest possible txt -> chess moves conversion
void convert_moves( const std::vector<std::string> &in, std::vector<thc::Move> &out )
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

