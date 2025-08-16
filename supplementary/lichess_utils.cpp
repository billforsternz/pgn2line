// supplementary.cpp : Do something special to LPGN file
//  lichess-utils.cpp, Misc lichess file related stuff

#include "..\util.h"
#include "..\thc.h"
#include "key.h"
#include "lichess_utils.h"

/*
 * 
 *  BabyClk PGN tag
 *  ===============
 * 

The problem; Lichess (and other computer chess systems) often litter PGN chess
documents with clock times as comments, for example {[%clk 1:29:30]}.

These are fantastically ugly and generally unappealing, but there's no doubt
they hold useful information.

What to do?

Let's store the equivalent information safely away as a series of PGN tags that
we don't have to look at.

I have constructed a system that generates a string with some nice properties
(only ASCII, no spaces, only well behaved punctuation characters that won't
cause any grief [no slashes forward or back, no quotes single or double, etc.]).
The string represents any number of clock times, about 1.6 characters are needed
for each clock time (it's subject to a certain amount of statistical variation,
as we will see). This is not fantastic efficiency, but it's decent and the whole
system is hopefully easy to understand and program.

Good names are hard to find, but I'm happy with my choice of BabyClk1, BabyClk2
etc. for the PGN tags. That's right, unlike the barbarians at Lichess we are
going respect the 80 column max specification of PGN and so the string will be
broken up into chunks and recombined as required. The average game will need two
or three lines in the header section.

Why BabyClk? It's distinctive and short. Short is important because we want to
keep that average number of lines down. Baby stands for Babylonian (base 60)
maths, very much on point when it comes to our minutes and seconds (as we will
see). Also Baby means our strings are small, get it?

Now the pesky details. We have one clock comment for each main line ply. Almost.
Sadly Lichess just omits the comment occasionally. So we have a list of moves
(the main line of the game), and a clock time for each, with a sprinkling of
omissions occasionally. Each clock time is of the form hh:mm:ss. I support
hh in the range [0-9], and both mm and ss in the range [0-59] (obviously).

We can model this as follows;

A list of clock times, exactly one per move;

[ clk0, clk1, clk2 ... clk(N-1) ]

Each element is an integer value hhmmss, or -1 if the clock time for that move
is absent. The notation hhmmss indicates hex numbers like 0x010909 for 01:09:09.
Or 0x013b3b for 01:59:59 (extra for hex experts). We could just have well used
simply the number of seconds 1*3600 + 59*60 + 59, it's easy to go backwards and
forwards between number of seconds and the hh, mm, ss components (obviously).

By convention the times clk0, clk2, clk4 etc. are White times. And clk1, clk3,
clk5 etc. are Black times.

The White times and the Black times independently count down, but very importantly
they are not strictly monotonically decreasing, because increment means that
players can gain time.

Our basic plan is to use two characters to encode each clock time. An important
enhancement is that we often can encode two consecutive clock times in three
characters. In fact we can usually do that which is why the average efficiency
at 1.6 characters per clock time is closer to 1.5 (half of three) than 2.

Conveniently, there are 62 ASCII alphanumeric characters. We use 60 of them to
represent a minute or second value, and we reserve Y and Z for other purposes.
I call the [0-59] encoding of the 60 alphanumerals "baby coding".

So the basic plan is simply to encode each clock time as two baby codes, one
for the mm component and one for the ss component. An additional mechanism using
letter 'Y' is used to handle occasional changes of hour. A special one character
code 'Z' represents a missing (-1) clk time.

Now consider the list [clk0, clk1, clk2.... ]. Two adjacent elements are always
a white and black time, although they might be ordered as either (white,black)
or (black,white). In either case if both times are present (not -1) and represent
deltas in the range of 0-5 minutes from the previous time, we encode them with
a special three character sequence.

The three character delta encoding comprises one of 25 lead in punctuation
characters, representing all combinations of [0-5) minutes for both white and black
then two baby codes, one for white and one for black respectively, representing the
seconds component of the white and black delta.

That's it really, it only remains to provide some reference information, which I
will present as a TLDR (sadly the TLDR is now longer than the main explanation).

TLDR (really, the details);

The BabyClk encoding protocol comprises the following elements;

Baby codes use all alphanumeric codes except 'Y' and 'Z' to represent numbers in the
range [0-59]. Ordering is ['0'-'9','a'-'z','A'-'X']

Duration codes are in the range [0-25) ordered according to the following string;
"!#$%&()*+,-.:;<=>?^_`{|}~"

An absolute clock time is encoded as two baby codes representing mm ss respectively.

A delta pair is encoded as a duration code followed by two baby codes representing
the seconds components of the white and black deltas respectively. The minute
components follows from the duration code itself, with values from 0 => 0,0 through
24 => 4,4 in the most natural ordering.

Important subtlety: The delta pair is used for two deltas in the range [-30,4:30)
rather than [0,5) minutes. This captures the small increments resulting from blitzed
out moves which are very statistically common. We add 30 seconds to an in range
delta before encoding using the easier range [0,5) minutes, and then subtract the 30
seconds back after decoding.

Example: white delta 2:31, black delta -20. Add 30 seconds (2:31,-20) => (3:01,0:10),
To encode 3 minutes and 0 minutes respectively we choose the duration code 15, since
3*5 + 0*5 = 15. From the reference string this indexes to '='. The two baby codes are
'1' for ss=01 and 'a' for ss=10. Final encoding is the three character string "=1a".

The final elements of the protocol are the use of 'Y' and 'Z' codes. Z codes are 1
character codes (simply 'Z') representing missing clock times.

Y codes are two character codes used as a prefix to an absolute code to change the
current hour for the current colour only. The current time for each side is tracked.
We could use a set initial value, but we try to do better to avoid two early
Y codes and also to increase our chance of going straight into delta encoding. The
starting time is set by the first character of the BabyClk string. The special
character is encoded using this table.

start_time_minutes[] = {90,3,5,10,15,25,30,60,75,120};

The values from the table are encoded as 'A','B'...'J'. 'A' is the default and is
omitted unless such omission would result in the first character of the BabyClk string
happening to unhelpfully fall into this 'A'-'J' range.

A subtlety worth stating explicitly - if a delta code crosses an hour boundary, it
eliminates the need for a Y code - both encoder and decoder already agree on the new
current value of the hour state variable.

Just one more thing: Experience shows that '!' representing a delta pair in the range
-30 to +30 seconds is extremely common. Strings of such pairs would appear both at
the start and end of many games and throughout blitz games. So we now have an
optimisation - '!' persists, successive '!' codes are implied and omitted, we call
this "blitzing mode". Blitzing mode persists until a different punctuation code or a
Y or Z code appears in the character stream. To end blitzing mode to transition to an
absolute time we have to do something special, "wasting" a character. We insert a
terminating '!'. This makes no sense as a duration code, since we are already in
blitzing mode it's not needed in that role, so we use it for this special job.

Just one more one more thing: We how have a useful little optimisation for the end
of the BabyClk string - if we have just one more clock time to encode and we are in
blitzing mode and the final clock time can be represented as a -30 to 30 second
duration: Them we stay in blitzing mode and represent that clock time with just one
character instead of three ('!' to leave blitzing mode plus mm and ss baby codes).

*/

// Some BabyClk options
// #define BABY_DEBUG
#define DURATION_OFFSET 30  // defines our 5 minute delta duration window as [-30,4:30) the short
                            //  negative deltas are very useful for blitzed out moves. If two
                            //  consecutive clk values are BOTH in the golden range we can encode
                            //  both in 3 bytes.

// Convert number from 0-59 to alphanumeric character
//  I'm calling this Babylonian codes, baby codes for short (especially as the idea
//  is to make short strings:)
static char baby_encode( int val )
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
static int baby_decode( char baby )
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

// ASCII printables complete -> !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
//
// 25 inoffensive punctuation codes in a PGN context -> !#$%&()*+,-.:;<=>?@^_{|}~
//  Avoids annoying symbols like each of -> "\/'`[]  (actually now @ out ` in to avoid @h, @M problems)
//  Of course '[' and ']' are only annoying in a PGN context
//  We did have a more complicated 31 character scheme but the 25 character scheme is
//  simpler, and more future proof

// Simplified, 25 duration codes, allows us to specify 5x5 = 25 scenarios, all combinations of
//  both players durations when they both fit in the same 5 minute window
static const char *punc = "!#$%&()*+,-.:;<=>?^_`{|}~";  // one benefit; avoids /\"'[] and Z

// Punctuation code -> idx in range 0-24
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
static bool punc_encode( int duration_w, int duration_b, char &punc_code, char &baby_w, char &baby_b )
{
    duration_w += DURATION_OFFSET;
    duration_b += DURATION_OFFSET;
    bool ok = (0<=duration_w && duration_w<5*60 && 0<=duration_b && duration_b<5*60);
    if( ok )
    {
        int min_w = duration_w/60;
        int sec_w = duration_w%60;
        int min_b = duration_b/60;
        int sec_b = duration_b%60;
        int idx  = 5*min_w + min_b; // 0;0 => 0, 0;1 => 1 .... 1;0 => 5 .... 4;4 => 24
        punc_code = punc[idx];
        baby_w = baby_encode(sec_w);
        baby_b = baby_encode(sec_b);
    }
    return ok;
}

// Decode duration coding
static bool punc_decode( char punc_code, char baby_w, char baby_b, int &duration_w, int &duration_b )
{
    int idx = -1;
    bool ok = punc_idx( punc_code, idx );
    if( !ok ) return false;
    int seconds_w = baby_decode(baby_w);
    int seconds_b = baby_decode(baby_b);
    int min_w = idx/5;
    int min_b = idx%5;
    duration_w = min_w*60 + seconds_w;
    duration_b = min_b*60 + seconds_b;
    duration_w -= DURATION_OFFSET;
    duration_b -= DURATION_OFFSET;
    return ok;
}

// Split hhmmss into components and calculate duration
static bool clk_times_calc_duration( int time, int hhmmss, int &hh, int &mm, int &ss, int &duration )
{
    if( hhmmss == -1 )  // filler ?
        return true;
    hh = (hhmmss>>16) & 0xff;
    mm = (hhmmss>>8) & 0xff;
    ss = hhmmss & 0xff;
    if( hh>9 )  hh = 9;
    if( mm>59 ) mm = 59;
    if( ss>59 ) ss = 59;

    // Calculate duration, positive values represent declining time
    int t2 = hh*3600 + mm*60 + ss;
    duration = time-t2;
    return false;
}

// Encode absolute time hhmmss
static void clk_times_encode_ply( bool filler, int time, int hh, int mm, int ss, std::string &out )
{
    out.clear();
    if( filler )
    {
        out = "Z";   // filler, sometimes no clk time is recorded for a move, just move on
        return;
    }

    // Encode hour changes, this is an exception, it doesn't happen often
    if( hh != (time/3600) )
        out += util::sprintf("Y%c", '0'+hh );

    // Encode minutes
    char baby = baby_encode(mm);
    out += baby;

    // Encode seconds
    baby = baby_encode(ss);
    out += baby;
}

// Start time encoding is an optimisation, use a single letter at the start of the BabyClk string
static int start_time_codes[] = {90,3,5,10,15,25,30,60,75,120};
#define START_TIME_CODE_MIN 'A'     // 'A' is minimum and also default
#define START_TIME_CODE_MAX ('A' + sizeof(start_time_codes)/sizeof(start_time_codes[0]) - 1)
#define START_TIME_CODE_DEFAULT 'A'

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times 
static void clk_times_encode( const std::vector<int> &clk_times, std::string &encoded_clk_times )
{
    #ifdef BABY_DEBUG
    printf("\nclk_times_encode()\n" );
    #endif
    encoded_clk_times.clear();
    bool blitzing_mode=false;

    // Set a good, hopefully excellent start time, using the first time from our array
    char start_time_code = START_TIME_CODE_DEFAULT; // fall back to this
    int best_so_far = INT_MAX;
    int len = (int) clk_times.size();
    int clk_time = (len>0 ? clk_times[0] : -1);
    if( clk_time != -1 )    // if not absent or filler
    {
        int s = ((clk_time>>16) & 0xff) * 3600 +
                ((clk_time>>8) & 0xff) * 60 +
                ( clk_time & 0xff);
        for( int i=0; i<sizeof(start_time_codes)/sizeof(start_time_codes[0]); i++ )
        {
            int trial_s = 60 * start_time_codes[i];
            int diff = trial_s - s;
            if( diff < 0 )
                diff = 0-diff;
            if( diff < best_so_far )
            {
                best_so_far = diff;
                start_time_code = START_TIME_CODE_MIN+i;
            }
        }
    }

    // Look for if( i == 0 ) sections below to actually omit the start_time_code
    //  (we don't do it now because we can often omit it)
    int start_time_s = 60 * start_time_codes[start_time_code-START_TIME_CODE_MIN];

    // Set the initial time, for encoding and decoding
    int time_w = start_time_s;  // Start time for both sides
    int time_b = start_time_s;

    // Loop taking one element (for absolute coding) or two for (delta coding) at a time
    for( int i=0; i<len; i++ )
    {
        bool more    = i+1<len;
        bool white   = ((i&1) == 0);
        int  hhmmss1 = clk_times[i];
        std::string emit_abs;
        std::string emit_delta;
        int duration_w=0, duration_b=0;
        int hh=0,mm=0,ss=0;

        // Encode first half
        int &time     = white ? time_w     : time_b;
        int &duration = white ? duration_w : duration_b;
        bool filler1 = clk_times_calc_duration( time, hhmmss1, hh, mm, ss, duration );
        clk_times_encode_ply( filler1, time, hh, mm, ss, emit_abs );

        // If we don't have a second half, write this one out, we're done
        if( !more )
        {
            if( !blitzing_mode )
            {

                // Check the sad corner case of the last character is also the first character
                if( i==0 )
                {

                    // If we want a start time other than default, prepend it
                    if( start_time_code != START_TIME_CODE_DEFAULT )
                        encoded_clk_times += start_time_code;

                    // If we use default, we still need to prepend it if we start with something
                    //  that looks like a start time code
                    else if( START_TIME_CODE_MIN<=emit_abs[0] && emit_abs[0]<=START_TIME_CODE_MAX )
                        encoded_clk_times += START_TIME_CODE_DEFAULT;
                }
                encoded_clk_times += emit_abs;
                #ifdef BABY_DEBUG
                printf( "ragged %06x %s\n", hhmmss1, emit_abs.c_str() );
                #endif
            }

            // Now support a single character (half duration) if blitzing mode
            //  can continue into the last clk time
            else
            {
                char punc_code, baby_w, baby_b;
                bool duration_coding = !filler1 && punc_encode( duration_w, duration_b, punc_code, baby_w, baby_b );
                if( duration_coding && punc_code=='!' )
                {
                    encoded_clk_times += (white ? baby_w : baby_b);     // this is the only time baby_b can be
                                                                        //  the first duration code
                    #ifdef BABY_DEBUG
                    printf( "ragged half duration %06x %s\n", hhmmss1, emit_abs.c_str() );
                    #endif
                }
                else
                {
                    encoded_clk_times += '!';   // blitzing mode -> absolute mode
                    encoded_clk_times += emit_abs;
                    #ifdef BABY_DEBUG
                    printf( "ragged %06x %s\n", hhmmss1, emit_abs.c_str() );
                    #endif
                }
            }
            continue;
        }

        // Calculate second half duration
        int hhmmss2 = clk_times[i+1];
        bool filler2 = white ? clk_times_calc_duration( time_b, hhmmss2, hh, mm, ss, duration_b )
                             : clk_times_calc_duration( time_w, hhmmss2, hh, mm, ss, duration_w );

        // Do duration coding if possible
        char punc_code, baby_w, baby_b;
        bool duration_coding = !filler1 && !filler2 && punc_encode( duration_w, duration_b, punc_code, baby_w, baby_b );

        // If duration coding , consume both elements
        if( duration_coding )
        {
            // Note that duration encoding may mean no hour updates are necessary when the
            //  hour changes

            // If we want a start time other than default, prepend it
            if( i==0 && start_time_code != START_TIME_CODE_DEFAULT )
                encoded_clk_times += start_time_code;

            // Consume both
            i++;
            time_w -= duration_w;
            time_b -= duration_b;

            // New optimization, blitzing_mode, '!' punctuation persists
            if( !blitzing_mode || punc_code != '!' )
                encoded_clk_times += punc_code;
            blitzing_mode = (punc_code == '!');
            encoded_clk_times += baby_w;    // baby_w always goes first, except now with our half duration situation above
            encoded_clk_times += baby_b;
            #ifdef BABY_DEBUG
            printf( "duration %06x %06x %d %d %c%c%c\n", hhmmss1, hhmmss2, duration_w, duration_b, punc_code, baby_w, baby_b );
            #endif
        }

        // Else if absolute coding , consume just one element
        else
        {
            time -= duration;
            if( blitzing_mode )
            {
                blitzing_mode = false;
                encoded_clk_times += '!';   // need something to terminate blitzing_mode
                // '!' makes no sense as a duration code in blitzing_mode, so use it to indicate
                // blitzing_mode -> absolute instead.
            }
            if( i==0 )
            {
                // If we want a start time other than default, prepend it
                if( start_time_code != START_TIME_CODE_DEFAULT )
                    encoded_clk_times += start_time_code;

                // If we use default, we still need to prepend it if we start with something
                //  that looks like a start time code
                else if( START_TIME_CODE_MIN<=emit_abs[0] && emit_abs[0]<=START_TIME_CODE_MAX )
                    encoded_clk_times += START_TIME_CODE_DEFAULT;
            }
            encoded_clk_times += emit_abs;
            #ifdef BABY_DEBUG
            printf( "time_%c %06x %s\n", white?'w':'b', hhmmss1, emit_abs.c_str() );
            #endif
        }
    }
}

//  For now the reverse procedure is a test only
static void clk_times_decode( const std::string &encoded_clk_times, std::vector<int> &clk_times )
{
    enum {init,hour_w,hour_b,minute_w,second_w,minute_b,second_b,duration_w,duration_b} state=init;
    bool blitzing_mode = false;
    bool fall_through_half_duration_at_end = false;
    char punc_code, baby_w, baby_b;
    bool white_save;

    // Keep time in seconds, and its hh,mm,ss components in sync as state variables
    int time_w=0, time_b=0;
    int hh_w=0,   hh_b=0;
    int mm_w=0,   mm_b=0;
    int ss_w=0,   ss_b=0;
    #ifdef BABY_DEBUG
    printf("\nclk_times_decode()\n" );
    #endif
    int len = (int)encoded_clk_times.length();
    for( int i=0; i<len; i++ )
    {
        bool more = (i+1<len);
        char c = encoded_clk_times[i];
        #ifdef BABY_DEBUG
        printf("%c", c );
        #endif
        switch(state)
        {
            case init:
            {
                // Set up our time state variables, even if the first code isn't
                //  a starting time code
                state = minute_w;   // next state, whether falling through or not
                bool is_time_code = (START_TIME_CODE_MIN<=c && c<=START_TIME_CODE_MAX);
                char start_time_code = START_TIME_CODE_DEFAULT;
                if( is_time_code )
                    start_time_code = c;
                time_w = time_b = 60 * start_time_codes[start_time_code-START_TIME_CODE_MIN];
                hh_w = hh_b = time_w/3600;
                mm_w = mm_b = (time_w - hh_w*3600) / 60;
                ss_w = ss_b = time_w%60;

                // Fall through if it wasn't a time code;
                if( is_time_code ) break;
            }

            // Normal start point, expect absolute encoding 1st baby code = minute, but also delta
            //  encoding, filler, and Y codes
            default:
            case minute_w:
            case minute_b:
            {
                bool white = (state==minute_w);
                int idx;
                if( c == 'Z' )
                {
                    #ifdef BABY_DEBUG
                    printf( " add filler\n" );
                    #endif
                    clk_times.push_back(-1);    // Z = filler for absent clock time
                    state = white?minute_b:minute_w;
                    blitzing_mode = false;
                }
                else if( c == 'Y' )
                {
                    state = (white?hour_w:hour_b);
                    blitzing_mode = false;
                }
                else if( punc_idx(c,idx) )
                {
                    punc_code = c;
                    if( c == '!' && blitzing_mode )
                    {
                        blitzing_mode = false;  // have to waste a character ending blitzing mode sometimes
                        // '!' makes no sense as a duration code in blitzing_mode, so use it to indicate
                        // blitzing_mode -> absolute instead. Stay in same state, mm ss are coming next
                    }
                    else
                    {
                        blitzing_mode = (c == '!');
                        state = duration_w;
                        white_save = white; // save whether duration represents a (w,b) or (b,w) pair
                                            // Note that this quite correctly is unchanged throughout
                                            // blitzing mode
                    }
                }
                else if( blitzing_mode )
                {
                    // We have effectively jumped straight to duration_w state
                    baby_w = c;
                    state = duration_b;
                    if( !more )
                        fall_through_half_duration_at_end = true;   // baby_b will correctly get the same value as baby_w
                }
                else
                {
                    int mm = baby_decode(c);
                    if( white )
                        mm_w = mm;
                    else
                        mm_b = mm;
                    state = (white?second_w:second_b);
                }
                if( !fall_through_half_duration_at_end ) break;
            }

            // 1st baby code in delta code is always white delta
            case duration_w:
            {
                if( state == duration_b )
                    ;   // allow fall through to duration_b
                else
                {
                    baby_w = c;
                    state = duration_b;
                    break;
                }
            }

            // 2nd and final baby code in delta code is always black delta
            case duration_b:
            {
                baby_b = c;
                state = white_save?minute_w:minute_b;
                int dur_w, dur_b;
                punc_decode(punc_code,baby_w,baby_b,dur_w,dur_b);
                #ifdef BABY_DEBUG
                printf( " %d %d", dur_w, dur_b );
                #endif

                // Calculate updated hhmmss for white and black
                int hhmmss_w=0;
                int hhmmss_b=0;
                for( int i=0; i<2; i++ )
                {
                    bool white = (i==0);
                    int &time     = white ? time_w : time_b;
                    int &duration = white ? dur_w : dur_b;
                    int &hh       = white ? hh_w : hh_b;
                    int &mm       = white ? mm_w : mm_b;
                    int &ss       = white ? ss_w : ss_b;
                    int &hhmmss   = white ? hhmmss_w : hhmmss_b;
                    time -= duration;
                    if( time < 0 )
                        time = 0;
                    hh = time / 3600;
                    mm = (time - 3600*hh) / 60;
                    ss = time % 60;
                    hhmmss =  ( ss      &     0xff);
                    hhmmss |= ((mm<<8)  &   0xff00);
                    hhmmss |= ((hh<<16) & 0xff0000);
                }

                // We can now do delta encoding of (w,b) pair or (b,w) pair
                //  so if the latter make sure to save them in that order
                int &val1 = white_save ? hhmmss_w : hhmmss_b;
                int &val2 = white_save ? hhmmss_b : hhmmss_w;
                #ifdef BABY_DEBUG
                printf( " %06x ",  val1 );
                printf( " %06x\n", val2 );
                #endif
                clk_times.push_back(val1);
                if( !fall_through_half_duration_at_end )
                    clk_times.push_back(val2);
                break;
            }

            // Get the 2nd character in absolute mode, representing seconds
            case second_w:
            case second_b:
            {
                bool white = (state==second_w);
                int &hh = white ? hh_w : hh_b;
                int &mm = white ? mm_w : mm_b;
                int &ss = white ? ss_w : ss_b;
                int &time = white ? time_w : time_b;
                ss = baby_decode(c);
                state = white ? minute_b : minute_w;
                int hhmmss=0;
                hhmmss =  ( ss      &     0xff);
                hhmmss |= ((mm<<8)  &   0xff00);
                hhmmss |= ((hh<<16) & 0xff0000);
                #ifdef BABY_DEBUG
                printf( " %06x\n", hhmmss );
                #endif
                clk_times.push_back(hhmmss);
                time = hh*3600 + mm*60 + ss;
                break;
            }
            
            // Y code character changes the hour
            case hour_w:
            case hour_b:
            {
                bool white = (state==hour_w);
                int &hh = white ? hh_w : hh_b;
                hh = c-'0';
                #ifdef BABY_DEBUG
                printf( " hour_%c %d ", white?'w':'b' );
                #endif
                state = white?minute_w:minute_b;
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

