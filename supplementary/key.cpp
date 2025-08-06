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
// #define USE_Z_FOR_FILLER

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

// Encode hhmmss against the context of the current time being time seconds
bool clk_times_encode_half( int hhmmss, int &time, std::string &out, int &duration )
{
    bool absent = true;
    out.clear();
    if( hhmmss == -1 )  // absent clock time ?
    {
        duration = 0;   // don't change duration, or time
        #ifdef USE_Z_FOR_FILLER
        out = "Z";      // Z = filler, represents absent clock time
        #else
        out = "YY";     // YY = filler, represents absent clock time
        #endif
        return absent;
    }
    if( hhmmss < 0 )
    {
        printf( "Debug assert clk_times_encode_half()\n" );
        return absent;
    }
    int hh = (hhmmss>>16) & 0xff;
    int mm = (hhmmss>>8) & 0xff;
    int ss = hhmmss & 0xff;
    if( hh>9 )  hh = 9;
    if( mm>59 ) mm = 59;
    if( ss>59 ) ss = 59;

    // Calculate duration, positive values represent declining time
    int t2 = hh*3600 + mm*60 + ss;
    duration = time-t2;

    // Encode hour changes, this is an exception doesn't happen often
    if( hh != (time/3600) )
        out += util::sprintf("Y%c", 'A'+hh );

    // Encode minutes
    char baby = baby_encode(mm);
    out += baby;

    // Encode seconds
    baby = baby_encode(ss);
    out += baby;

    // Update time
    time = t2;
    absent = false;
    return absent;
}

// ASCII printables complete -> !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
// 31 inoffensive punctuation codes in a PGN context -> !#$%&()*+,-./:;<=>?@[\Z^_`{|}~
//  Don't really want ] (because PGN) so use Z for that
//
// 31 duration codes, allows us to specify 5x5 + 2x3 = 31 scenarios
// OR simpler system 25 duration codes, allows us to specify 5x5 = 25 scenarios
#define USE_25_CODES
#define DURATION_OFFSET 30
#ifdef USE_25_CODES
static const char *punc = "!#$%&()*+,-.:;<=>?@^_{|}~";  // one benefit; avoids /\"'`[] and Z
#else
static const char *punc = "!#$%&'()*+,-./:;<=>?@[\\Z^_`{|}~";
#endif

// Punctuation code -> idx in range 0-30
static bool punc_idx( char in, int &idx )
{
    idx = -1;
    for( int i=0; punc[i]; i++ )
    {
        if( punc[i] == in )
        {
            idx = i;
            break;
        }
    }
    return idx >= 0;
}

// If possible, encode durations as three characters a punctuation lead in code, then two baby codes
static bool punc_encode( int duration1, int duration2, char &punc_code, char &baby1, char &baby2 )
{
#ifdef USE_25_CODES
    duration1 += DURATION_OFFSET;
    duration2 += DURATION_OFFSET;
    bool ok = (0<=duration1 && duration1<5*60 && 0<=duration2 && duration2<5*60);
    if( ok )
    {
        int min1 = duration1/60;
        int sec1 = duration1%60;
        int min2 = duration2/60;
        int sec2 = duration2%60;
        int idx  = 5*min1 + min2; // 0;0 => 0, 0;1 => 1 .... 1;0 => 5 .... 4;4 => 25
        punc_code = punc[idx];
        baby1 = baby_encode(sec1);
        baby2 = baby_encode(sec2);
    }
    return ok;
#else
    bool ok = false;
    int base = 0;
    int idx = 0;
    int min1 = duration1/60;
    int sec1 = duration1%60;
    int min2 = duration2/60;
    int sec2 = duration2%60;

    // First 3 codes; duration1 is negative and duration2 is negative or 0 minutes or 1 minutes
    if( duration1 < 0 )
    {
        base = 0;
        idx  = duration2<0 ? 0 : min2+1;    // 0,1 or 2
        ok = -60 < duration1 && -60 < duration2 && duration2 < 120;
        if( ok )
        {
            sec1 = 0-duration1; // +ve number 0-59, don't mess with % operator on negative numbers
            if( duration2 < 0 )
                sec2 = 0-duration2; // +ve number 0-59, don't mess with % operator on negative numbers
        }
    }

    // Next 3 codes; duration2 is negative and duration1 is negative or 0 minutes or 1 minutes
    else if( duration2 < 0 )
    {
        base = 3;
        idx  = duration1<0 ? 0 : min1+1;    // 0,1 or 2
        ok = -60 < duration2 && -60 < duration1 && duration1 < 120;
        if( ok )
        {
            sec2 = 0-duration2; // +ve number 0-59, don't mess with % operator on negative numbers
            if( duration1 < 0 )
                sec1 = 0-duration1; // +ve number 0-59, don't mess with % operator on negative numbers
        }
    }

    // Final 25 codes duration1 and duration2 both positive and less than 5 minutes
    else
    {
        base = 6;
        idx  = 5*min1 + min2; // 0;0 => 0, 0;1 => 1 .... 1;0 => 5 .... 4;4 =>25
        ok = min1<5 && min2<5;
    }
    if( ok )
    {
        punc_code = punc[base+idx];
        baby1 = baby_encode(sec1);
        baby2 = baby_encode(sec2);
    }
    return ok;
#endif
}

// Decode duration coding
static bool punc_decode( char punc_code, char baby1, char baby2, int &duration1, int &duration2 )
{
#ifdef USE_25_CODES
    int idx = -1;
    bool ok = punc_idx( punc_code, idx );
    if( !ok ) return false;
    int seconds1 = baby_decode(baby1);
    int seconds2 = baby_decode(baby2);
    int min1 = idx/5;
    int min2 = idx%5;
    duration1 = min1*60 + seconds1;
    duration2 = min2*60 + seconds2;
    duration1 -= DURATION_OFFSET;
    duration2 -= DURATION_OFFSET;
    return ok;
 #else
    int idx = -1;
    bool ok = punc_idx( punc_code, idx );
    if( !ok ) return false;
    int seconds1 = baby_decode(baby1);
    int seconds2 = baby_decode(baby2);

    // First 3 codes; duration1 is negative and duration2 is negative or 0 or 1 minutes
    if( idx < 3 )
    {
        duration1 = 0-seconds1;
        if( idx == 0 )
            duration2 = 0-seconds2;
        else
            duration2 = (idx-1)*60 + seconds2;
    }

    // Next 3 codes; duration2 is negative and duration1 is negative or 0 or 1 minutes
    else if( idx < 6 )
    {
        idx -= 3;
        duration2 = 0-seconds2;
        if( idx == 0 )
            duration1 = 0-seconds1;
        else
            duration1 = (idx-1)*60 + seconds1;
    }

    // Final 25 codes duration1 and duration2 both positive and less than 5 minutes
    else
    {
        idx -= 6;
        int min1 = idx/5;
        int min2 = idx%5;
        duration1 = min1*60 + seconds1;
        duration2 = min2*60 + seconds2;
    }
    return ok;
#endif
}

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times
//
// #define BABY_DEBUG
// For debugging #define BABY_DEBUG, copy log to an editor and use column editing to line up
// the encoder to the decoder, it's pretty cool
//
void clk_times_encode( const std::vector<int> &clk_times, std::string &encoded_clk_times )
{
    #ifdef BABY_DEBUG
    printf("\nclk_times_encode()\n" );
    #endif
    encoded_clk_times.clear();
    int time1 = 5400;   // start times is 1:30:00, 90 minutes, 5400 seconds, by my decreed convention
    int time2 = 5400;

    int len = (int) clk_times.size();
    for( int i=0; i<len; i++ )
    {
        int hhmmss = clk_times[i];
        std::string emit1;
        std::string emit2;
        std::string emit_duration;
        int duration1, duration2;

        // Encode first half
        bool absent1 = clk_times_encode_half( hhmmss, time1, emit1, duration1 );

        // If we don't have a second half, write this one out
        if( i+1 >= len )
        {
            encoded_clk_times += emit1;
            #ifdef BABY_DEBUG
            printf( "ragged %06x %s\n", hhmmss, emit1.c_str() );
            #endif
            continue;
        }

        // Encode a second half
        int hhmmss2 = clk_times[++i];    // ++i to do two array values per iteration of the loop
        bool absent2 = clk_times_encode_half( hhmmss2, time2, emit2, duration2 );

        // Do duration coding if possible
        char punc_code, baby1, baby2;
        bool duration_coding = !absent1 && !absent2 && punc_encode( duration1, duration2, punc_code, baby1, baby2 );
        if( duration_coding )
        {

            // Note that duration encoding may mean no hour updates are necessary when the
            //  hour changes
            encoded_clk_times += punc_code;
            encoded_clk_times += baby1;
            encoded_clk_times += baby2;
            #ifdef BABY_DEBUG
            printf( "duration %06x %06x %d %d %c%c%c\n", hhmmss, hhmmss2, duration1, duration2, punc_code, baby1, baby2 );
            #endif
            continue;
        }

        // Changing both hours at once routinely happens at the start of a
        //  rapid game say (change both hours from 1 -> 0), so it's worth
        //  a little optimisation
        if( emit1[0]=='Y' && emit2[0]=='Y' && emit1[1]==emit2[1] && emit1[1]!='Y' )
        {

            // change matching "YA" "YA" (say) for both sides to "Y0" for both
            emit1[1] = emit1[1] - 'A' + '0';     // z coding remains possible
            emit2 = emit2.substr(2);
        }
        encoded_clk_times += emit1;
        #ifdef BABY_DEBUG
        printf( "player1 %06x %s\n", hhmmss, emit1.c_str() );
        #endif
        encoded_clk_times += emit2;
        #ifdef BABY_DEBUG
        printf( "player2 %06x %s\n", hhmmss2, emit2.c_str() );
        #endif
    }
}

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times 
//  For now the reverse procedure is a test only
void clk_times_decode( const std::string &encoded_clk_times, std::vector<int> &clk_times )
{
    enum {hour1,hour2,minute1,second1,minute2,second2,duration1,duration2} state=minute1;
    int time1 = 5400;   // start times is 1:30:00, 90 minutes, 5400 seconds, by my decreed convention
    int time2 = 5400;
    int hh1   = time1 / 3600;
    int mm1   = (time1 - 3600*hh1) / 60;
    int ss1   = time1 % 60;
    int hh2   = time2 / 3600;
    int mm2   = (time2 - 3600*hh2) / 60;
    int ss2   = time2 % 60;
    char punc_code, baby1, baby2;
    #ifdef BABY_DEBUG
    printf("\nclk_times_decode()\n" );
    #endif
    for( char c: encoded_clk_times )
    {
        #ifdef BABY_DEBUG
        printf("%c", c );
        #endif
        switch(state)
        {
            default:
            case minute1:
            case minute2:
            {
                bool first = (state==minute1);
                int idx;
                if( c == 'Z' )
                {
                    #ifdef BABY_DEBUG
                    printf( " add filler\n" );
                    #endif
                    clk_times.push_back(-1);    // Z = filler for absent clock time
                    state = first?minute2:minute1;
                }
                else if( c == 'Y' )
                {
                    state = (first?hour1:hour2);
                }
                else if( first && punc_idx(c,idx) )
                {
                    punc_code = c;
                    state = duration1;
                }
                else
                {
                    int mm = baby_decode(c);
                    if( first )
                        mm1 = mm;
                    else
                        mm2 = mm;
                    state = (first?second1:second2);
                }
                break;
            }
            case second1:
            {
                ss1 = baby_decode(c);
                state = minute2;
                int hhmmss=0;
                hhmmss =  ( ss1      &     0xff);
                hhmmss |= ((mm1<<8)  &   0xff00);
                hhmmss |= ((hh1<<16) & 0xff0000);
                #ifdef BABY_DEBUG
                printf( " %06x\n", hhmmss );
                #endif
                clk_times.push_back(hhmmss);
                time1 = hh1*3600 + mm1*60 + ss1;
                break;
            }
            case second2:
            {
                ss2 = baby_decode(c);
                state = minute1;
                int hhmmss=0;
                hhmmss =  ( ss2      &     0xff);
                hhmmss |= ((mm2<<8)  &   0xff00);
                hhmmss |= ((hh2<<16) & 0xff0000);
                #ifdef BABY_DEBUG
                printf( " %06x\n", hhmmss );
                #endif
                clk_times.push_back(hhmmss);
                time2 = hh2*3600 + mm2*60 + ss2;
                break;
            }
            case hour1:
            case hour2:
            {
                bool first = (state==hour1);
                if( c == 'Y' )
                {
                    #ifdef BABY_DEBUG
                    printf( " add filler\n" );
                    #endif
                    clk_times.push_back(-1);    // YY = filler for absent clock time
                    state = first?minute2:minute1;
                }
                else if( '0'<= c && c<='9' )
                {
                    hh1 = hh2 = c-'0';
                    #ifdef BABY_DEBUG
                    printf( " hour (both) %d ", hh1 );
                    #endif
                    state = first?minute1:minute2;
                }
                else
                {
                    if( first )
                    {
                        hh1 = c-'A';
                        #ifdef BABY_DEBUG
                        printf( " hour1 %d ", hh1 );
                        #endif
                    }
                    else
                    {
                        hh2 = c-'A';
                        #ifdef BABY_DEBUG
                        printf( " hour2 %d ", hh2 );
                        #endif
                    }
                    state = first?minute1:minute2;
                }
                break;
            }
            case duration1:
            {
                baby1 = c;
                state = duration2;
                break;
            }
            case duration2:
            {
                baby2 = c;
                state = minute1;
                int duration1, duration2;
                punc_decode(punc_code,baby1,baby2,duration1,duration2);
                #ifdef BABY_DEBUG
                printf( " %d %d", duration1, duration2 );
                #endif
                time1 -= duration1;
                if( time1 < 0 )
                    time1 = 0;
                hh1 = time1 / 3600;
                mm1 = (time1 - 3600*hh1) / 60;
                ss1 = time1 % 60;
                int hhmmss=0;
                hhmmss =  ( ss1      &     0xff);
                hhmmss |= ((mm1<<8)  &   0xff00);
                hhmmss |= ((hh1<<16) & 0xff0000);
                #ifdef BABY_DEBUG
                printf( " %06x", hhmmss );
                #endif
                clk_times.push_back(hhmmss);
                time2 -= duration2;
                if( time2 < 0 )
                    time2 = 0;
                hh2 = time2 / 3600;
                mm2 = (time2 - 3600*hh2) / 60;
                ss2 = time2 % 60;
                hhmmss=0;
                hhmmss =  ( ss2      &     0xff);
                hhmmss |= ((mm2<<8)  &   0xff00);
                hhmmss |= ((hh2<<16) & 0xff0000);
                #ifdef BABY_DEBUG
                printf( " %06x\n", hhmmss );
                #endif
                clk_times.push_back(hhmmss);
                break;
            }
        }
    }
}

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
        clk_times_encode( clk_times, encoded_clk_times );
        std::vector<int> temp;
        clk_times_decode( encoded_clk_times, temp );
        if( clk_times == temp )
        {
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
