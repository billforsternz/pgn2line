//  baby.cpp, Convert clock times to a string suitable for a PGN tag

#include "..\util.h"
#include "baby.h"

/*
 * 
 *  BabyClk PGN tag
 *  ===============
 * 

(These notes need to be updated with a new concept - we now try more than
5:5 = 25 delta codes, we also try 6:6=36, 7:7=49, 8:8=64 and 9:9=81 with
progressively more overlap between delta codes and baby codes which is
great when delta is possible and we don't have to use an escape/sentinel
too often to resolve the overlap. We try all possibilities, pick a winner
and prepend an options code to the string to indicate who won) 

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

// 
// Some BabyClk options
// 

//#define BABY_DEBUG
#define DURATION_OFFSET 30  // defines our 5 minute delta duration window as [-30,4:30) the short
                            //  negative deltas are very useful for blitzed out moves. If two
                            //  consecutive clk values are BOTH in the golden range we can encode
                            //  both in 3 bytes.

//
//  Local data
//
static int start_times[] = {90,3,15,25,60,75};
#define NBR_START_TIMES (sizeof(start_times)/sizeof(start_times[0]))
#define NBR_DELTA_TABLES 10
static int start_time_counts[NBR_START_TIMES];
static int delta_range_counts[NBR_DELTA_TABLES];
static int delta_range_count;
static int start_times_count;
static bool has_overlaps;
static int  nbr_deltas;

const char *delta_0 =
    "1111111100"
    "1111100000"
    "1111000000"
    "1110000000"
    "1100000000"
    "1000000000"
    "1000000000"
    "1000000000"
    "0000000000"
    "0000000000";

const char *delta_1 =
    "1111111111"
    "1111000000"
    "1110000000"
    "1100000000"
    "1000000000"
    "1000000000"
    "1000000000"
    "1000000000"
    "1000000000"
    "1000000000";


const char *delta_2 =
    "1111111110"
    "1111110000"
    "1111100000"
    "1111000000"
    "1110000000"
    "1100000000"
    "1000000000"
    "1000000000"
    "1000000000"
    "0000000000";

const char *delta_3 =
    "1111111100"
    "1111111100"
    "1111111100"
    "1000000000"
    "0000000000"
    "0000000000"
    "0000000000"
    "0000000000"
    "0000000000"
    "0000000000";

const char *delta_4 =
    "1111000000"
    "1110000000"
    "1110000000"
    "1110000000"
    "1110000000"
    "1110000000"
    "1110000000"
    "1110000000"
    "0000000000"
    "0000000000";
static bool is_delta_combo[10][10];
static int delta_value[10][10];
static int delta_x[100];
static int delta_y[100];

static void build_delta_tables( int delta_idx )
{
    const char *src=NULL;
    int square=5;
    switch(delta_idx)
    {
        default:
        case 0: src = delta_0;  break;
        case 1: src = delta_1;  break;
        case 2: src = delta_2;  break;
        case 3: src = delta_3;  break;
        case 4: src = delta_4;  break;
        case 5: square=5;   break;
        case 6: square=6;   break;
        case 7: square=7;   break;
        case 8: square=8;   break;
        case 9: square=9;   break;
    }
    if( src )
    {
        for( int i=0; i<10; i++ )
        {
            for( int j=0; j<10; j++ )
            {
                is_delta_combo[i][j] = (*src++ == '1');    
            }
        }
    }
    else
    {
        for( int i=0; i<10; i++ )
        {
            for( int j=0; j<10; j++ )
            {
                is_delta_combo[i][j] = (i<square && j<square);    
            }
        }
    }
    int val=0;
    for( int i=0; i<10; i++ )
    {
        for( int j=0; j<10; j++ )
        {
            if( is_delta_combo[i][j] )
                delta_value[i][j] = val++;
            else
                delta_value[i][j] = -1;
        }
    }
    nbr_deltas = val;
    has_overlaps = nbr_deltas>25;
    for( int i=0; i<100; i++ )
    {
        delta_x[i] = -1;
        delta_y[i] = -1;
    }
    for( int i=0; i<10; i++ )
    {
        for( int j=0; j<10; j++ )
        {
            int val = delta_value[i][j];
            if( val != -1 )
            {
                delta_x[val] = i;
                delta_y[val] = j;
            }
        }
    }
}


//
//  Local prototypes
//

// Encode the clock times with one set of configurable options
static void clk_times_encode_inner( const std::vector<int> &clk_times,
               std::string &encoded_clk_times, int delta_idx );

// Convert number from 0-59 to alphanumeric character
//  I'm calling this Babylonian codes, baby codes for short (especially as the idea
//  is to make short strings:)
static char baby_encode( int val );
static int baby_decode( char baby );
static bool is_baby( char c );

// Convert number from 0-24 to punctuation character
static char punc_encode( int val );
static int  punc_decode( char punc );
static bool is_punc( char c );

// Delta encoding, combine punc codes and baby codes to give up to 81 (9x9) characters
static char delta_encode( int val );
static int  delta_decode( char delta );
static bool is_delta( char c, int delta_idx );

// If possible, encode durations as three characters a punctuation lead in code, then two baby codes
static bool duration_encode( int duration_w, int duration_b, char &delta_code, char &baby_w, char &baby_b, int delta_idx );

// Decode duration coding
static bool duration_decode( char delta_code, char baby_w, char baby_b, int &duration_w, int &duration_b, int delta_idx );

// Split hhmmss into components and calculate duration
static bool clk_times_calc_duration( int time, int hhmmss, int &hh, int &mm, int &ss, int &duration );

// Encode absolute time hhmmss
static void clk_times_encode_abs( bool filler, int time, int hh, int mm, int ss, std::string &out, int delta_idx );

// Combine x [0-6), y [0-5), z[0-2) into a single value [0-60)
//  and baby code it
static char options_combine( int x, int y );
static void options_split( char baby, int &x, int &y );

//
//  Exported functions
//

// Encode the clock times efficiently as an alphanumeric string, we call this BabyClk, short for
//  Babylonian (base-60) clock times. This outer shell just calls an inner function, searching
//  for the best options.
void baby_clk_encode( const std::vector<int> &clk_times, std::string &encoded_clk_times )
{
    int best_so_far = INT_MAX;
    int delta_idx = 0;
    int  delta_range_winner = delta_idx;
    for( delta_idx=0; delta_idx<NBR_DELTA_TABLES; delta_idx++ )
    {
        build_delta_tables( delta_idx );
        clk_times_encode_inner( clk_times, encoded_clk_times, delta_idx );
        if( encoded_clk_times.length() < (size_t)best_so_far )
        {
            delta_range_winner = delta_idx;
            best_so_far = (int)encoded_clk_times.size();
        }
    }
    build_delta_tables( delta_range_winner );
    clk_times_encode_inner( clk_times, encoded_clk_times, delta_range_winner );
}

//  Decode from BabyClk string -> array of clock times
void baby_clk_decode( const std::string &encoded_clk_times, std::vector<int> &clk_times )
{
    enum {init,delta_w,delta_b,hour_w,hour_b,minute_w,second_w,minute_b,second_b,duration_w,duration_b,blitzing} state=init;
    bool fall_through_half_duration_at_end = false;
    char punc_code, baby_w, baby_b;
    bool white_save;
    int  nbr_ys_in_a_row;
    int  delta_idx = 0;

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
        // if( c=='i' && more && encoded_clk_times[i+1] =='v' )
        //    printf( "DEBUG\n" );
        #ifdef BABY_DEBUG
        printf("%c", c );
        #endif
        switch(state)
        {
            case init:
            {
                state = delta_w;
                char start_time_plus_delta_code = c;
                int x, y;
                options_split( c, x, y );
                int start_time_idx  = x;
                delta_idx = y;
                build_delta_tables(delta_idx);
                start_time_counts[start_time_idx]++;
                delta_range_counts[delta_idx]++;
                time_w = time_b = 60 * start_times[start_time_idx];
                hh_w = hh_b = time_w/3600;
                mm_w = mm_b = (time_w - hh_w*3600) / 60;
                ss_w = ss_b = time_w%60;
                break;
            }

            // Normal start point, expect start of a delta coded triplet by default
            default:
            case delta_w:
            case delta_b:
            {
                bool white = (state==delta_w);
                if( c == 'Z' )
                {
                    #ifdef BABY_DEBUG
                    printf( " add filler\n" );
                    #endif
                    clk_times.push_back(-1);    // Z = filler for absent clock time
                    state = white?delta_b:delta_w;
                }
                else if( c == 'Y' )
                {
                    // Path 1 to absolute encoding
                    state = (white?hour_w:hour_b);
                    nbr_ys_in_a_row = 1;
                }
                else if( has_overlaps && c=='X' )
                {
                    // Path 2 to absolute encoding
                    state = (white?minute_w:minute_b);
                }
                else if( is_delta(c,delta_idx) )
                {
                    punc_code = c;
                    state = duration_w;
                    white_save = white; // save whether duration represents a (w,b) or (b,w) pair
                                        // Note that this quite correctly is unchanged throughout
                                        // blitzing mode
                }
                else // if( is_baby(c) )    // only baby codes left
                {
                    // Path 3 to absolute encoding
                    int mm = baby_decode(c);
                    if( white )
                        mm_w = mm;
                    else
                        mm_b = mm;
                    state = (white?second_w:second_b);
                }
                break;
            }

            // Y code character changes the hour
            case hour_w:
            case hour_b:
            {
                bool white = (state==hour_w);
                bool fall_through = (c!='Y');   // fall through after run of 'Y' codes
                if( c == 'Y' )
                    nbr_ys_in_a_row++;
                else
                {
                    int &hh = white ? hh_w : hh_b;
                    hh = nbr_ys_in_a_row-1;
                    #ifdef BABY_DEBUG
                    printf( " hour_%c %d ", white?'w':'b', hh );
                    #endif
                    state = white?minute_w:minute_b;
                    // fall through to minute_w and minute_b
                }
                if( !fall_through ) break;
            }

            // Absolute encoding
            case minute_w:
            case minute_b:
            {
                bool white = (state==minute_w);
                int mm = baby_decode(c);
                if( white )
                    mm_w = mm;
                else
                    mm_b = mm;
                state = (white?second_w:second_b);
                break;
            }

            // Blitzing is implied '!' -> duration_w, so like duration_w but other things
            //  can happen
            case blitzing:
            {
                int idx = 0;
                bool fall_through = false;
                // Y and Z codes exit blitzing
                if( c == 'Z' )
                {
                    #ifdef BABY_DEBUG
                    printf( " add filler\n" );
                    #endif
                    clk_times.push_back(-1);    // Z = filler for absent clock time
                    state = white_save ? delta_b : delta_w;
                }
                else if( c == 'Y' )
                {
                    state = (white_save ? hour_w : hour_b);
                    nbr_ys_in_a_row = 1;
                }

                // '!' exits blitzing (protocol trick, '!' is not needed as a duration
                //   code during blitzing, so use it to exit blitzing
                else if( c == '!' )
                {
                    state = white_save ? delta_w : delta_b;   // just exit blitzing
                }
                
                // Other punc codes stay in duration coding, but not blitzing
                else if( is_punc(c) )
                {
                    punc_code = c;
                    state = duration_w;
                }

                // Else must be a baby code, fall through to duration_w
                else
                {
                    fall_through = true;
                }
                if( !fall_through ) break;
            }

            // 1st delta baby code
            case duration_w:
            {
                baby_w = c;
                state = duration_b;
                if( !more )
                    fall_through_half_duration_at_end = true;   // baby_b will correctly get the same value as baby_w
                if( !fall_through_half_duration_at_end ) break;
            }

            // 2nd and final baby code in delta code is always black delta
            case duration_b:
            {
                baby_b = c;
                state = (punc_code=='!' ? blitzing : (white_save?delta_w:delta_b));
                int dur_w, dur_b;
                duration_decode(punc_code,baby_w,baby_b,dur_w,dur_b,delta_idx);
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
                state = white ? delta_b : delta_w;
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
        }
    }
}

//  Generate extra stats output for diagnostics
std::string baby_stats_extra()
{
    std::string s = "Delta range popularity: ";
    for( int i=0; i<NBR_DELTA_TABLES; i++ )
    {
        s += util::sprintf("%s%d:%d%s", i==0?"[":",",
                i, delta_range_counts[i],
                i==NBR_DELTA_TABLES-1?"]\n":"");
    }
    s += "Start time popularity: ";
    for( int i=0; i<NBR_START_TIMES; i++ )
    {
        s += util::sprintf("%s%d:%d%s", i==0?"[":",",
                start_times[i], start_time_counts[i],
                i==NBR_START_TIMES-1?"]\n":"");
    }
    return s;
 }

//
//  Local functions
//

// Inner encoder, with one set of options. baby_clk_encode() tries every combination and uses the best
static void clk_times_encode_inner( const std::vector<int> &clk_times,
                                std::string &encoded_clk_times, int delta_idx )
{
    #ifdef BABY_DEBUG
    printf("\nclk_times_encode()\n" );
    #endif
    encoded_clk_times.clear();
    bool blitzing_mode=false;

    // Set a good, hopefully excellent nominal start time, using the first time from our array
    int start_time_idx = 0;  // fall back to this
    int best_so_far = INT_MAX;
    int clk_time = -1;
    for( int val: clk_times )
    {
        if( val != -1 )
        {
            clk_time = val;
            break;
        }
    }
    if( clk_time != -1 )    // if not absent or filler
    {
        int s = ((clk_time>>16) & 0xff) * 3600 +
                ((clk_time>>8) & 0xff) * 60 +
                ( clk_time & 0xff);
        for( int i=0; i<NBR_START_TIMES; i++ )
        {
            int trial_s = 60 * start_times[i];
            int diff = trial_s - s;
            if( diff < 0 )
                diff = 0-diff;
            if( diff < best_so_far )
            {
                best_so_far = diff;
                start_time_idx = i;
            }
        }
    }

    // Set the initial time, for encoding and decoding
    int start_time_s = 60 * start_times[start_time_idx];
    int time_w = start_time_s;  // Start time for both sides
    int time_b = start_time_s;

    // Combine start_time and delta_idx into options code
    char options_code = options_combine(start_time_idx,delta_idx);
    encoded_clk_times += options_code;

    // Loop taking one element (for absolute coding) or two for (delta coding) at a time
    int len = (int) clk_times.size();
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
        int &time1     = white ? time_w     : time_b;
        int &duration1 = white ? duration_w : duration_b;
        bool filler1 = clk_times_calc_duration( time1, hhmmss1, hh, mm, ss, duration1 );
        clk_times_encode_abs( filler1, time1, hh, mm, ss, emit_abs, delta_idx );

        // Calculate second half duration (fake it if !more)
        int hhmmss2 = more ? clk_times[i+1] : 0;
        int &time2     = white ? time_b     : time_w;
        int &duration2 = white ? duration_b : duration_w;
        bool filler2 = clk_times_calc_duration( time2, hhmmss2, hh, mm, ss, duration2 );

        // Do duration coding if possible
        char delta_code, baby_w, baby_b;
        if( !more )
            duration2 = duration1;  // set both durations to the first duration if !more
        bool duration_coding = !filler1 && !filler2 && duration_encode( duration_w, duration_b, delta_code, baby_w, baby_b, delta_idx );

        // If duration coding , consume both elements
        if( duration_coding )
        {
            // Note that duration encoding may mean no hour updates are necessary when the
            //  hour changes

            // Consume both
            i++;
            time_w -= duration_w;
            time_b -= duration_b;

            // New optimization, blitzing_mode, '!' punctuation persists
            if( blitzing_mode && !is_punc(delta_code) )
                encoded_clk_times += '!';   // transition from blitzing to non punc delta code needs something
                                            //  to indicate delta code is not baby coded mm in blitzing mode
            if( !blitzing_mode || delta_code != '!' )
                encoded_clk_times += delta_code;
            blitzing_mode = (delta_code == '!');
            encoded_clk_times += baby_w;    // baby_w always goes first, unless !more
            if( more )
                encoded_clk_times += baby_b;    // if !more baby_b and baby_w are the same value
            #ifdef BABY_DEBUG
            printf( "duration %06x %06x %d %d %c%c%c\n", hhmmss1, hhmmss2, duration_w, duration_b, delta_code, baby_w, more?baby_b:' ' );
            #endif
        }

        // Else if absolute coding, consume just one element
        else
        {
            time1 -= duration1;
            if( blitzing_mode )
            {
                blitzing_mode = false;
                if( emit_abs[0] != 'Y' )
                    encoded_clk_times += '!';   // need something to terminate blitzing_mode
                // '!' makes no sense as a duration code in blitzing_mode, so use it to indicate
                // blitzing_mode -> absolute instead.
            }
            encoded_clk_times += emit_abs;
            #ifdef BABY_DEBUG
            printf( "time_%c %06x %s\n", white?'w':'b', hhmmss1, emit_abs.c_str() );
            #endif
        }
    }

    // If start time and delta range index only, no need to keep it
    if( encoded_clk_times.length() == 1 )
        encoded_clk_times.clear();

    // A little optimisation, if we use default start time, we omit it unless
    // such omission would result in the first character of the BabyClk string
    // happening to unhelpfully fall into the start time range
#if 0
    if( encoded_clk_times.length() < 2 )
        encoded_clk_times.clear();  // start_time_code only, omit
    else
    {
        bool in_range = (START_TIME_CODE_MIN<=encoded_clk_times[1] && encoded_clk_times[1]<=START_TIME_CODE_MAX);
        if( start_time_code==START_TIME_CODE_DEFAULT && !in_range )
            encoded_clk_times = encoded_clk_times.substr(1);    // omit
    }
#endif
}

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
//  Avoids annoying symbols like each of -> "\/'`[]  (actually now @ out ` in to avoid @H, @M problems)
//  Of course '[' and ']' are only annoying in a PGN context

// Simplified, 25 duration codes, allows us to specify 5x5 = 25 scenarios, all combinations of
//  both players durations when they both fit in the same 5 minute window
static const char *delta_encode_lookup = "!#$%&()*+,-.:;<=>?^_`{|}~WVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543";
static const char *punc_encode_lookup = delta_encode_lookup;
#define NBR_PUNC_CHARACTERS 25      // = DELTA_MIN*DELTA_MIN

static char delta_encode( int val )
{
    return delta_encode_lookup[val];
}

static int delta_decode( char delta )
{
    static int  delta_decode_lookup[128];
    static bool tables_built;
    if( !tables_built )
    {
        tables_built = true;
        for( int i=0; i<sizeof(delta_decode_lookup)/sizeof(delta_decode_lookup[0]); i++ )
            delta_decode_lookup[i] = -1;
        const char *s = delta_encode_lookup;
        for( int i=0; *s; i++ )
        {
            char c = *s++;
            delta_decode_lookup[c] = i;
        }
    }
    int val = delta_decode_lookup[delta];
    return val;
}

static char punc_encode( int val )
{
    return punc_encode_lookup[val];
}

static int punc_decode( char punc )
{
    int val = delta_decode( punc );
    if( val >= NBR_PUNC_CHARACTERS )
        val = -1;
    return val;
}

static bool is_baby( char c )
{
    return ('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='X');
}

static bool is_punc( char c )
{
    int val = delta_decode( c );
    bool in_range = (0<=val && val<NBR_PUNC_CHARACTERS );
    return in_range;
}

static bool is_delta( char c, int delta_idx )
{
    int val = delta_decode( c );
    bool in_range = (0<=val && val<nbr_deltas );
    return in_range;
}



// If possible, encode durations as three characters a punctuation lead in code, then two baby codes
static bool duration_encode( int duration_w, int duration_b, char &delta_code, char &baby_w, char &baby_b, int delta_idx )
{
    duration_w += DURATION_OFFSET;
    duration_b += DURATION_OFFSET;
    int min_w = duration_w/60;
    int sec_w = duration_w%60;
    int min_b = duration_b/60;
    int sec_b = duration_b%60;
    bool ok = (0<=duration_w && min_w<10 && 0<=duration_b && min_b<10 && is_delta_combo[min_w][min_b] );
    if( ok )
    {
        int idx = delta_value[min_w][min_b];
        delta_code = delta_encode(idx);
        baby_w = baby_encode(sec_w);
        baby_b = baby_encode(sec_b);
        // if( delta_code == '!' && baby_w=='g' && baby_b=='z' )
        //     printf( "Debug\n" );
    }
    return ok;
}

// Decode duration coding
static bool duration_decode( char delta_code, char baby_w, char baby_b, int &duration_w, int &duration_b, int delta_idx )
{
    if( !is_delta(delta_code,delta_idx) )
        return false;
    int idx = delta_decode(delta_code);
    int seconds_w = baby_decode(baby_w);
    int seconds_b = baby_decode(baby_b);
    int min_w = delta_x[idx];
    int min_b = delta_y[idx];
    duration_w = min_w*60 + seconds_w;
    duration_b = min_b*60 + seconds_b;
    duration_w -= DURATION_OFFSET;
    duration_b -= DURATION_OFFSET;
    return true;
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
static void clk_times_encode_abs( bool filler, int time, int hh, int mm, int ss, std::string &out, int delta_idx )
{
    out.clear();
    if( filler )
    {
        out = "Z";   // filler, sometimes no clk time is recorded for a move, just move on
        return;
    }

    // Encode minutes
    char baby = baby_encode(mm);

    // Encode hour changes, this is an exception, it doesn't happen often
    if( hh != (time/3600) )
    {
        for( int i=0; i<hh+1; i++ )
            out += 'Y';
    }

    // Absolute codes always follow Y codes, and otherwise we need an 'X' to lead in to
    //  the absolute minute and second baby codes
    else if( has_overlaps && (is_delta(baby,delta_idx) || baby=='X') )  // if the baby code is not unambiguous
        out += 'X'; // sentinel
    out += baby;

    // Encode seconds
    baby = baby_encode(ss);
    out += baby;
}

// Combine x [0-6) and y [0-10) into a single value [0-60)
//  and baby code it
static char options_combine( int x, int y )
{
    int xy = x + y*6;
    char baby = baby_encode(xy);
    return baby;
}

/*     y  0  1  2  3  4  5  6  7  8  9
   x
   0      0  6  12 18 24 30 36 42 48 54
   1      1  7  13 19 25 31 37 43 49 55
   2      2  8  14 20 26 32 38 44 50 56
   3      3  9  15 21 27 33 39 45 51 57
   4      4  10 16 22 28 34 40 46 52 58
   5      5  11 17 23 29 35 41 47 53 59

*/
static void options_split( char baby, int &x, int &y )
{
    int xy = baby_decode(baby);
    x      = xy % 6; 
    y      = xy / 6; 
}

/*
void test_options()
{
    for( int x=0; x<6; x++ )
    {
        for( int y=0; y<5; y++ )
        {
            for( int z=0; z<2; z++ )
            {
                char code = options_combine(x,y,z);
                int X,Y,Z;
                options_split(code,X,Y,Z);
                bool ok = (x==X && y==Y && z==Z);
                if( !ok )
                {
                    printf( "FAIL x=%d, y=%d, z=%d, code=%d, X=%d, Y=%d, Z=%d\n", x,y,z,baby_decode(code), X,Y,Z );
                    return;
                }
            }
        }
    }
    printf( "PASS\n" );
}
*/