// supplementary.cpp : Do something special to LPGN file
//  key.cpp, Key utilities, Tags are key value pairs

//
//  Note that older commands do not use these utilities (but could profitably be recoded to use them)
//

#include "..\util.h"
#include "..\thc.h"
#include "key.h"

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

//
// Some misc utilities that deserve their own home somewhere
//

int get_main_line2( const std::string &s, std::vector<std::string> &main_line, std::string &moves_txt, int &nbr_comments )
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

// Convert number from 0-59 to alphanumeric character
//  I'm calling this Babylonian codes, baby codes for short (especially as the idea
//  is to make short strings:)
char baby_encode( int val )
{

    // Input must already be constrained into 0-59 range
    char baby;
    if( val < 10 )
        baby = '0'+val;          // 0-9 => '0'-'9'
    else if( val < 36 )
        baby = 'a'+(val-10);     // 10-35 => 'a'-'z'
    else
        baby = 'A'+(val-36);     // 36-59 => 'A'-'X'
    return baby;
}

// Decode baby codes
int baby_decode( char baby )
{

    // Input must already be constrained into alphanumeric range
    int val;
    if( '0'<=baby && baby<='9' )
        val = baby-'0';         // '0'-'9' => 0-9
    else if( 'a'<=baby && baby<='z' )
        val = baby-'a'+10;      // 'a'-'z' => 10-35
    else
        val = baby-'A'+36;      // 'A'-'X' => 36-59
    return val;
}

void clk_times_encode_half( int hhmmss, std::string &out, int &hh, int &mm, int &ss, int &duration )
{
    out.clear();
    if( hhmmss < 0 )
    {
        printf( "Debug assert clk_times_encode_half()\n" );
        return;
    }
    int h = (hhmmss>>16) & 0xff;
    int m = (hhmmss>>8) & 0xff;
    int s = hhmmss & 0xff;
    if( h>9 ) h = 9;
    if( m>59 ) m = 59;
    if( s>59 ) s = 59;

    // Encode hour changes only if necessary, eg start of a classical game
    //  or a player thinks for more than an hour
    //  or an arbiter adjusts the clock and we don't detect normal hour rollover
    bool running = hh>0 || mm>0 || ss>0;
    bool hour_rollover = running && m>mm && hh>0 && h==hh-1;
    if( !hour_rollover && h != hh )
        out += util::sprintf("Y%c", 'A'+h );

    // Encode minutes
    char baby = baby_encode(m);
    out += baby;

    // Encode seconds
    baby = baby_encode(s);
    out += baby;

    // Calculate duration
    int t1 = hh*3600 + mm*60 + ss;
    int t2 = h*3600 + m*60 + s;
    duration = t1-t2 > 0 ? t1-t2 : -1;

    // Update to new values
    hh = h;
    mm = m;
    ss = s;
}


// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times 
void clk_times_encode( const std::vector<int> &clk_times, std::string &encoded_clk_times )
{
    encoded_clk_times.clear();
    int whour=0, wmin=0, wsec=0;    
    int bhour=0, bmin=0, bsec=0;

    int len = (int) clk_times.size();
    for( int i=0; i<len; i+=2 )
    {
        int hhmmss = clk_times[i];
        std::string emit1;
        std::string emit2;
        int duration1, duration2;
        clk_times_encode_half( hhmmss, emit1, whour, wmin, wsec, duration1 );

        // If we don't have a second half, write this one out we can't do pair
        //  optimisations at a ragged end
        if( i+1 >= len )
        {
            encoded_clk_times += emit1;
            continue;
        }

        // Encode a second half, initially without optimisations
        hhmmss = clk_times[i+1];
        clk_times_encode_half( hhmmss, emit2, bhour, bmin, bsec, duration2 );

        // Start optimisation phase
        bool zcode_is_possible = true;

        // Changing both hours at once routinely happens at the start of a
        //  classical game (change both hours from 0 -> 1), so it's worth
        //  a little optimisation
        if( emit1[0]=='Y' && emit2[0]=='Y' )        
        {
            if( emit1[1] != emit2[1] )
            {
                // Changing both hours at once to different values ruins all hope
                //  of optimisation (it's rather unlikely, to say the least,
                //  but we still handle it smoothly)
                zcode_is_possible = false;
            }
            else
            {
                // Optimisation 1), change matching "YA" "YA" (say) for both sides to "Y1" for both
                emit1[1] = emit1[1] - 'A' + '1';     // z coding remains possible
                emit2 = emit2.substr(2);
            }
        }

        // Optimisation 2), replace mm,ss,mm,ss quartet with 'Z',dd,dd where dd = delta
        //  in seconds, if both deltas were <60 seconds
        if( zcode_is_possible && 0<=duration1 && duration1<60 && 0<=duration2 && duration2<60 )
        {
            if( emit1[0] == 'Y' )
                encoded_clk_times += emit1.substr(0,2);
            encoded_clk_times += 'Z';
            char baby = baby_encode(duration1);
            encoded_clk_times += baby;
            baby = baby_encode(duration2);
            encoded_clk_times += baby;
        }
        else
        {
            encoded_clk_times += emit1;
            encoded_clk_times += emit2;
        }
    }
}

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times 
//  For now the reverse procedure is a test only
void clk_times_decode( const std::string &encoded_clk_times, std::vector<int> &clk_times )
{
    int whour=0, wmin=0, wsec=0;    
    int bhour=0, bmin=0, bsec=0;
    int state=0, save_state;
    for( char c: encoded_clk_times )
    {
        bool emit=false;
        switch(state)
        {
            default:
            case 0:
            case 1:
            case 2:
            case 3:
            {
                if( c == 'Y' )
                {
                    save_state = state;
                    state = 4;
                }
                else if( c=='Z' )
                {
                    state = 5;
                }
                else
                {
                    int val = baby_decode(c);
                    switch( state )
                    {
                        case 0: if( val > wmin )
                                    whour--;
                                wmin = val;    break;
                        case 1: wsec = val;    break;
                        case 2: if( val > bmin )
                                    bhour--;
                                bmin = val;    break;
                        case 3: bsec = val;    break;
                    }
                    state++;
                    if( state > 3 )
                    {
                        emit = true;
                        state = 0;
                    }
                }
                break;
            }
            case 4:
            {
                if( '0'<= c && c<='9' )
                    whour = bhour = c-'0';
                else
                {
                    if( save_state == 0 )
                        whour = c-'A';
                    else
                        bhour = c-'A';
                }
                state = save_state;
                break;
            }
            case 5:
            case 6:
            {
                int duration = baby_decode(c);
                int &h = (state==5 ? whour : bhour);
                int &m = (state==5 ? wmin  : bmin );
                int &s = (state==5 ? wsec  : bsec );
                int remaining = duration;
                int chunk = (s>=remaining ? remaining : s);
                s -= chunk;
                remaining -= chunk;
                if( remaining > 0 )
                {
                    s = 60-remaining;
                    if( m > 0 )
                        m--;
                    else
                    {
                        if( h > 0 )
                        {
                            h--;
                            m = 59;
                        }
                    }
                }
                state++;
                if( state > 6 )
                {
                    state = 0;
                    emit = true;
                }
                break;
            }
        }
        if( emit )
        {
            if( whour > 1 ) printf( "Assert whour=%d\n", whour );
            int hhmmss=0;
            hhmmss =  ( wsec       &     0xff);
            hhmmss |= ((wmin<<8)   &   0xff00);
            hhmmss |= ((whour<<16) & 0xff0000);
            clk_times.push_back(hhmmss);
            if( bhour > 1 ) printf( "Assert bhour=%d\n", bhour );
            hhmmss =  ( bsec       &     0xff);
            hhmmss |= ((bmin<<8)   &   0xff00);
            hhmmss |= ((bhour<<16) & 0xff0000);
            clk_times.push_back(hhmmss);
        }
    }
    if( state == 2 )    // ragged end?
    {
        int hhmmss=0;
        hhmmss =  ( wsec       &     0xff);
        hhmmss |= ((wmin<<8)   &   0xff00);
        hhmmss |= ((whour<<16) & 0xff0000);
        clk_times.push_back(hhmmss);
    }
}

int lichess_moves_to_normal_pgn( const std::string &header, const std::string &moves, std::string &normal, std::string &encoded_clk_times, int &nbr_comments )
{
    std::vector<std::string> main_line;
    std::vector<int> clk_times;
    std::string moves_txt;
    int mask = get_main_line( moves, main_line, clk_times, moves_txt, nbr_comments );
    if( main_line.size() == clk_times.size() )
    {
        clk_times_encode( clk_times, encoded_clk_times );
        std::vector<int> temp;
        clk_times_decode( encoded_clk_times, temp );
        if( clk_times != temp )
        {
            int len = clk_times.size();
            int len2 = temp.size();
            printf( "BabyClk encoding test fails, whoops: %d %d %s\n", len, len2, encoded_clk_times.c_str() );
            for( int i=0; i<len && i<len2; i++ )
            {
                printf( "0x%06x, 0x%06x %s\n", clk_times[i], temp[i], clk_times[i]==temp[i] ? "pass" : "fail");
            }
        }
    }
    else
    {
        if( clk_times.size() > 0 )
        {
            printf( "Unexpectedly cannot do BabyClk encoding %zd %zd\n",
                        main_line.size(), clk_times.size() );
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
                        }
                    }
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
                comment.clear();
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
