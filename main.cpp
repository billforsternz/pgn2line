/*

    PGN2LINE                                            Bill Forster, October 2018

    A chess PGN file management system based on the idea of converting PGN file(s)
    to a new and highly convenient one game per line text format. The system makes
    it easy to collect massive collections of disparate pgn files into a large,
    neatly ordered and consolidated pgn file (or files). Along the way exact
    duplicate games will be discarded and problematic missing blank lines (between
    headers and moves, and between games) will be fixed. Whitelist, blacklist and
    fixuplist facilities make it easy to filter tournaments and harmonise
    tournament naming.

    Why is a one game per line format convenient? An example should make the concept
    clear. Consider using grep (i.e. search) on a one game per line file searching
    for the text (say) "Forster". The grep output will be a valid one game per line
    file comprising all games featuring that text - which includes all games played
    by anyone named Forster.

    Another example, in a Nakamura-Carlsen game, the format includes the text
    "Nakamura-Carlsen". Search for all lines with that text and you get all such
    games ready for immediate conversion back into PGN.

    Usage:
     pgn2line [-l] [-z] [-d] [-n] [-r] [-p]
              [-y year_before] [+y year_after] [-w whitelist | -b blacklist]
              [-f fixuplist]  input output

     -l indicates input is a text file that lists input pgn files (else input is a pgn file)
     -z indicates don't include zero length games (BYEs are unaffected)
     -Z indicates don't include zero length games, including BYEs
     -d indicates smart game de-duplication (eliminates more dups)
     -n indicates no sorting or de-duping
	 -r specifies smart reverse sort - yields most recent games first, smart because higher
        rounds/boards are adjusted to come first both here and in the conventional sort
        order
     -p indicates create a .pgn from output (filename is ".pgn" appended to output)
     -y discard games unless they are played in year_before or earlier
     +y discard games unless they are played in year_after or later
     -w specifies a whitelist list of tournaments, discard games not from these tournaments
     -b specifies a blacklist list of tournaments, discard games from these tournaments
     -f specifies a list of tournament name fixups
     -2 flag removes games unless both players have been fixed up (useful for
        cleaning online PGNs with a list of some but not all player handles)

    Output is all games found in one game per line format, sorted for maxium utility.

    Program line2pgn (or option -p) converts back to PGN format.

    Whitelist, blacklist and fixuplist are all text files listing tournaments in the 
    following format;

    yyyy Event@Site

    For example;
    
        2018 German Open@Berlin

    Fixuplist must have an even number of lines as it is interpreted as before/after pairs.
    For example

        2018 German Open@Berlin
        2018 23rd German Open@Berlin, Germany

    This will apply the simple transformation Event->"23rd German Open" and Site->"Berlin, Germany"
    For the moment Events and/or Sites with embedded @ characters are not accommodated.
    
    Pairs of before and after player names are now allowed in the fixup file,
    player names are identified as those strings NOT in yyyy Event@Site format.
 
    Also line2pgn, a program to convert back to pgn.

    And tournaments, a program to convert the line format into a tournament list. This list can be
    in the same format as the whitelist/blacklist/fixuplist greatly simplifying preparation of
    such lists.

    Games are represented on a single line as follows;

        Prefix@Hheader1@Hheader2...@Mmoves1@Mmoves2...

    The header1, header2 etc. are the PGN header lines, moves1, moves2 etc. are the PGN
    moves lines

    The prefix to the line serves two purposes
     1) readable summary of the game
     2) allows simple string sort to order games effectively

    Example for Smith v Jones, Event=Acme Open, Site=Gotham, round 3.2.1 on 2001-12-31 prefix is;
      2001-12-28 Acme Open, Gotham # 2001-12-31 003.002.001 000000001 Smith-Jones

    The first date is the tournament date (first day in tournament), the second date is the
    game date.

    Note: I have added a 9 digit tie breaker after the round information. It's a game idx,
    1 for first game read, 2 for second etc. and serves to order the games in the original
    source order if everything else matches. The first release of this suite lacked this
    feature.

    Note 2: I am now updating the program for Release 3.0, and I am going to remove the tie
    breaker as a final stage of pgn2line because it makes the .lpgn files dramatically less useful
    for comparisons - the smallest changes anywhere make for a completely different .lpgn file
    line by line, only because of the presence of this field. In other words, it is still used
    internally for intermediate processing (it's very useful) but discarded in a different stage
    at the end. So externally we are back to the V1.0 format;
      2001-12-28 Acme Open, Gotham # 2001-12-31 003.002.001 Smith-Jones

    The net result is that all games in the tournament are grouped together in date/round
    order. Tournaments are sorted by start date of tournament. This can reasonably be
    considered optimum game ordering.

    When processing game by game we don't actually know the start date of the tournament,
    so we set the tournament date to the game date (so 2001-12-31 here). That allows a
    useful stage one disk sort.
    
    Later on in the "refined sort" stage we group together tournament games that share
    the same Event and Site in any 6 month window. The tournament date is easily determined
    as the first game date encountered for the tournament after the stage 1 sort, allowing
    us to replace the proxy start date with the real start date.
    
    TODO - Events and/or Sites with embedded @ characters are not accommodated by the
    whitelist, blacklist and fixuplist files, extend the tournament list syntax used by
    those files with an appropriate extension to allow that
     eg "@#2008 John@Smith.com Open#Berlin" -> if the first character is a '@' then the 2nd
        character replaces @ as the Event/Site separator.

*/

/*
   Define (only) one of the following options to select which program to build
*/

#define PGN2LINE      // Convert one or more PGN files into a single file in our new format
//#define LINE2PGN      // Convert back to PGN
//#define TOURNAMENTS   // A simple utility for extracting tournament names from line file
//#define PLAYERS       // A utility for extracting player names from line file
//#define DISKSORT      // Just for testing our disksort() 
//#define WORDSEARCH    // A poor man's grep -w

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include "disksort.h"
#include "util.h"

static bool pgn2line( std::string fin, std::string fout, std::string diag_fout,
                        bool &utf8_bom,
                        bool append,
                        bool reverse_order,
                        bool remove_zero_length,
                        bool remove_zero_length_allow_bye,
                        bool remove_unfixed_players_flag,
                        int year_before,
                        int year_after,
                        const std::set<std::string> &whitelist,
                        const std::set<std::string> &blacklist,
                        const std::map<std::string,std::string> &fixups,
                        const std::map<std::string,std::string> &name_fixups );
static void line2pgn( std::string fin, std::string fout );
static void tournaments( std::string fin, std::string fout, bool bare=false );
static void players( std::string fin, std::string fout, bool bare, bool dups_only );
static bool read_tournament_list( std::string fin, std::vector<std::string> &tournaments, std::vector<std::string> *names=NULL  );
static bool test_date_format( const std::string &date, char separator );
static bool parse_date_format( const std::string &date, char separator, int &yyyy, int &mm, int &dd );
static bool refine_sort( std::string fin, std::string fout );
static void word_search( bool case_insignificant, std::string word, std::string fin, std::string fout );
static void remove_tie_breaker_and_dups( std::string fin, std::string fout, bool add_utf8_bom_to_output, std::ofstream *p_smart_uniq, bool no_deduping_at_all=false );
static void postponed_dedup_filter( bool flush, const std::string &line, std::ofstream &out, std::ofstream *p_smart_uniq );

int main( int argc, const char *argv[] )
{
    util::tests();

#ifdef DISKSORT
    bool ok = (argc==3);
    if( !ok )
    {
        printf(
            "Simple text file sort\n"
            "Usage:\n"
            " sort input.txt output.txt\n"
        );
        return -1;
    }
    disksort(argv[1],argv[2]);
    return 0;
#endif

#ifdef WORDSEARCH
    int arg_idx=1;
    bool case_insignificant=false;
    bool ok = true;
    if( argc>1 && std::string(argv[1]) == "-i" )
    {
        case_insignificant = true;
        argc--;
        arg_idx=2;
    }
    if( argc<3 || argc>4 )
        ok = false;
    if( !ok )
    {
        printf(
            "wordsearch V3.02 (from Github.com/billforsternz/pgn2line)\n"
            "Simple text file search for words. Useful for filtering .lpgn files for\n"
            "specific players, events etc.\n"
            "Usage:\n"
            " wordsearch [-i] word input.txt [output.txt]\n"
            "-i flag requests a case insignificant search\n"
        );
        return -1;
    }
    word_search( case_insignificant, argv[arg_idx],argv[arg_idx+1],argc==4?argv[arg_idx+2]:"");
    return 0;
#endif

#ifdef TOURNAMENTS
    int arg_idx=1;
    bool bare=false;
    bool ok = true;
    if( argc>1 && std::string(argv[1]) == "-b" )
    {
        bare = true;
        argc--;
        arg_idx=2;
    }
    if( argc<2 || argc>3 )
        ok = false;
    if( !ok )
    {
        printf(
            "tournaments V3.02 (from Github.com/billforsternz/pgn2line)\n"
            "Extract tournament descriptions from files created by pgn2line\n"
            "Usage:\n"
            " tournaments [-b] input.lpgn [output.txt]\n"
            "\n"
            "-b requests bare format suitable for use as a whitelist/blacklist file\n"
        );
        return -1;
    }
    tournaments( argv[arg_idx], argc==2?"":argv[arg_idx+1], bare );
    return 0;
#endif

#ifdef PLAYERS
    int arg_idx=1;
    bool bare=false;
    bool dups_only=false;
    bool ok = true;

    while( ok && argc>2 )
    {
        if( std::string(argv[arg_idx]) == "-b" )
            bare = true;
        else if( std::string(argv[arg_idx]) == "-d" )
            dups_only = true;
        else
            break;
        argc--;
        arg_idx++;
    }
    if( argc<2 || argc>3 )
        ok = false;
    if( !ok )
    {
        printf(
            "players V3.02 (from Github.com/billforsternz/pgn2line)\n"
            "Extract player names from pgn files (or lpgn files created by pgn2line)\n"
            "Usage:\n"
            " players [-b] [-d] input.pgn|lpgn [output.txt]\n"
            "\n"
            "-b requests bare format suitable for use in a pgn2line fixup file\n"
            "-d requests only names with duplicate surnames present\n"
            "\n"
            "Output is sorted, format is name : count (or just name if -b flag)\n"
            "Names without commas are highlighted by adding a \"@ \" prefix\n"
        );
        return -1;
    }
    players( argv[arg_idx], argc==2?"":argv[arg_idx+1], bare, dups_only );
    return 0;
#endif

#ifdef LINE2PGN
    bool ok = (argc==3);
    if( !ok )
    {
        printf(
            "line2pgn V3.02 (from Github.com/billforsternz/pgn2line)\n"
            "Convert one game per line files generated by companion program pgn2line to pgn\n"
            "Usage:\n"
            " line2pgn input.lpgn output.pgn\n"
        );
        return -1;
    }
    std::string fin(argv[1]);
    std::string fout(argv[2]);
    if( fin == fout )
    {
        printf( "Error: input and output filenames are the same.\n" );
        return -1;
    }
    line2pgn(fin,fout);
    return 0;
#endif

#ifdef PGN2LINE
    // Command line processing
    bool remove_zero_length = false;
    bool remove_zero_length_allow_bye = false;
    bool list_flag = false;
    bool reverse_flag = false;
    bool pgn_create_flag = false;
    bool remove_unfixed_players_flag = false;
    bool whitelist_flag = false;
    bool smart_uniq = false;
    bool no_sort = false;
    std::string whitelist_file;
    bool blacklist_flag = false;
    std::string blacklist_file;
    bool fixup_flag = false;
    std::string fixup_file;
    bool ok = true;

    // Unless one or both of these are set on the command line
    //  yyyy>=year_after && yyyy<=year_before is true
    int year_after=-10000, year_before=10000;
#ifdef _DEBUG   // for debugging / testing
    smart_uniq = true;
    pgn_create_flag = true;
    list_flag = true;
    std::string fin("test-in-plus-test-in2.lst");
    std::string fout("test-out.lpgn");
#else

    int arg_idx=1;
    while( ok && argc>1 )
    {
        if( std::string(argv[arg_idx]) == "-l" )
            list_flag = true;
        else if( std::string(argv[arg_idx]) == "-r" )
            reverse_flag = true;
        else if( std::string(argv[arg_idx]) == "-Z" )
            remove_zero_length = true;
        else if( std::string(argv[arg_idx]) == "-z" )
            remove_zero_length_allow_bye = true;
        else if( std::string(argv[arg_idx]) == "-d" )
            smart_uniq = true;
        else if( std::string(argv[arg_idx]) == "-n" )
            no_sort = true;
        else if( std::string(argv[arg_idx]) == "-p" )
            pgn_create_flag = true;
        else if( std::string(argv[arg_idx]) == "-2" )
            remove_unfixed_players_flag = true;
        else if( util::prefix( std::string(argv[arg_idx]),"-f") )
        {
            fixup_flag = true;
            if( std::string(argv[arg_idx]) == "-f" )
            {
                argc--;
                arg_idx++;
                fixup_file = std::string(argv[arg_idx]);
            }
            else
            {
                fixup_file = std::string(argv[arg_idx]).substr(2);
            }
        }
        else if( util::prefix( std::string(argv[arg_idx]),"-w") )
        {
            whitelist_flag = true;
            if( std::string(argv[arg_idx]) == "-w" )
            {
                argc--;
                arg_idx++;
                whitelist_file = std::string(argv[arg_idx]);
            }
            else
            {
                whitelist_file = std::string(argv[arg_idx]).substr(2);
            }
        }
        else if( util::prefix( std::string(argv[arg_idx]),"-b") )
        {
            blacklist_flag = true;
            if( std::string(argv[arg_idx]) == "-b" )
            {
                argc--;
                arg_idx++;
                blacklist_file = std::string(argv[arg_idx]);
            }
            else
            {
                blacklist_file = std::string(argv[arg_idx]).substr(2);
            }
        }
        else if( util::prefix( std::string(argv[arg_idx]),"-y") )
        {
            if( std::string(argv[arg_idx]) == "-y" )
            {
                argc--;
                arg_idx++;
                year_before = atoi(argv[arg_idx]);
            }
            else
            {
                year_before = atoi(argv[arg_idx]+2);
            }
            if( year_before == 0 )
                ok = false;
        }
        else if( util::prefix( std::string(argv[arg_idx]),"+y") )
        {
            if( std::string(argv[arg_idx]) == "+y" )
            {
                argc--;
                arg_idx++;
                year_after = atoi(argv[arg_idx]);
            }
            else
            {
                year_after = atoi(argv[arg_idx]+2);
            }
            if( year_after == 0 )
                ok = false;
        }
        else
            break;
        argc--;
        arg_idx++;
    }
    if( argc != 3 )
        ok = false;
    if( !ok || (whitelist_flag&&blacklist_flag) )
    {
/*
     pgn2line [-l] [-z] [-d] [-n] [-r] [-p]
              [-y year_before] [+y year_after] [-w whitelist | -b blacklist]
              [-f fixuplist]  input output

     -l indicates input is a text file that lists input pgn files (else input is a pgn file)
     -z indicates don't include zero length games (BYEs are unaffected)
     -Z indicates don't include zero length games, including BYEs
     -d indicates smart game de-duplication (eliminates more dups)
     -n indicates no sorting or de-duping
	 -r specifies smart reverse sort - yields most recent games first, smart because higher
        rounds/boards are adjusted to come first both here and in the conventional sort
        order
     -p indicates create a .pgn from output (filename is ".pgn" appended to output)
     -y discard games unless they are played in year_before or earlier
     +y discard games unless they are played in year_after or later
     -w specifies a whitelist list of tournaments, discard games not from these tournaments
     -b specifies a blacklist list of tournaments, discard games from these tournaments
     -f specifies a list of tournament name fixups
     -2 flag removes games unless both players have been fixed up (useful for
        cleaning online PGNs with a list of some but not all player handles)

*/

        printf(
        "pgn2line V3.02+ (from Github.com/billforsternz/pgn2line)\n"
        "Convert pgn file(s) to an intermediate format, one line per game, sorted\n"
        "\n"
        "Usage:\n"
        " pgn2line [-l] [-z] [-d] [-n] [-r] [-p] [-y year_before] [+y year_after]\n"
        "          [-w whitelist | -b blacklist]\n"
        "          [-f fixuplist] input output.lpgn\n"
        "\n"
        "-l indicates input is a text file that lists input pgn files\n"
        "   (otherwise input is a single pgn file)\n"
        "-z indicates don't include zero length games (BYEs are unaffected)\n"
        "-Z indicates don't include zero length games, including BYEs\n"
        "-d indicates smart game de-duplication (eliminates more dups)\n"
        "-n indicates no sorting or de-duping\n"
		"-r specifies smart reverse sort - yields most recent games first, smart\n"
		"   because higher rounds/boards are adjusted to come first both here and\n"
		"   in the conventional sort order\n"
        "-p indicates create a .pgn from output (filename is \".pgn\" appended to\n"
        "   output)\n"
        "-y discard games unless they are played in year_before or earlier\n"
        "+y discard games unless they are played in year_after or later\n"
        "-w specifies a whitelist list of tournaments, discard games not from one\n"
        "   of these tournaments\n"
        "-b specifies a blacklist list of tournaments, discard games from any of\n"
        "   these tournaments\n"
        "-f specifies a list of tournament name fixups\n"
        "\n"
        "-w -b and -f files are all lists of tournaments in the following format\n"
        "\n"
        "yyyy Event@Site\n"
        "\n"
        "In the case of the fixuplist there must be an even number of lines in the\n"
        "file because the file is interpreted as a list of before and after pairs\n"
        "\n"
        "Pairs of before and after player names are now allowed in the fixup file,\n"
        "player names are identified as those strings NOT in yyyy Event@Site format\n"
        "\n"
        "-2 flag removes games unless both players have been fixed up (useful for\n"
        "   cleaning online PGNs with a list of some but not all player handles)\n"
        "\n"
        "Output is all games found in one game per line format, sorted. The .lpgn\n"
        "extension shown is just a suggested convention\n"
        "\n"
        "See also companion programs line2pgn, wordsearch, tournaments and players\n"
        );
        return -1;
    }
    std::string fin(argv[arg_idx]);
    std::string fout(argv[arg_idx+1]);
#endif
    if( fin == fout )
    {
        printf( "Error: input and output filenames are the same.\n" );
        return -1;
    }

    // Read tournament files
    std::set<std::string> whitelist;
    std::set<std::string> blacklist;
    std::map<std::string,std::string> fixups;
    std::map<std::string,std::string> name_fixups;
    if( whitelist_flag )
    {
        std::vector<std::string> temp;
        ok = read_tournament_list( whitelist_file, temp );
        for( unsigned int i=0; ok && i<temp.size(); i++ )
            whitelist.insert(temp[i]);
    }
    if( ok && blacklist_flag )
    {
        std::vector<std::string> temp;
        ok = read_tournament_list( blacklist_file, temp );
        for( unsigned int i=0; ok && i<temp.size(); i++ )
            blacklist.insert(temp[i]);
    }
    std::string diag_fout;
    if( ok && fixup_flag )
    {
        std::vector<std::string> temp;
        std::vector<std::string> names;
        ok = read_tournament_list( fixup_file, temp, &names );
        if( ok && temp.size()%2 != 0 )
        {
            printf( "Error; Odd number of tournaments in fixup table, should be before and after pairs\n" );
            ok = false;
        }
        else
        {
            for( unsigned int i=0; ok && i<temp.size(); i+=2 )
                fixups.insert( std::pair<std::string,std::string>(temp[i],temp[i+1]) );
        }
        if( ok && names.size()%2 != 0 )
        {
            printf( "Error; Odd number of player names in fixup table, should be before and after pairs\n" );
            ok = false;
        }
        else
        {
            if( names.size() > 0 )
            {
                diag_fout = fout + "-name-fixups.txt";
                printf( "All player name fixups will be listed in file %s\n", diag_fout.c_str() );
            }
            for( unsigned int i=0; ok && i<names.size(); i+=2 )
                name_fixups.insert( std::pair<std::string,std::string>(names[i],names[i+1]) );
        }
    }
    if( !ok )
        return -1;
    unsigned int seed = static_cast<unsigned int>( time(NULL) );
    srand(seed);
    int r1=rand();
    int r2=rand();
    while( r1 == r2 )
        r2=rand();
    std::string temp1_fout = util::sprintf( "%s-temp-filename-pgn2line-presort-%05d.tmp", fout.c_str(), r1 );
    std::string temp2_fout = util::sprintf( "%s-temp-filename-pgn2line-postsort-%05d.tmp", fout.c_str(), r2 );
    ok = false;
    printf( "pgn2line V3.02+ (from Github.com/billforsternz/pgn2line)\n" );
    bool all_utf8_bom = true;
    if( !list_flag )
    {
        printf( "Processing 1 pgn file\n" );
        ok = pgn2line( fin, temp1_fout, diag_fout,
                    all_utf8_bom,
                    false,
                    reverse_flag,
                    remove_zero_length,
                    remove_zero_length_allow_bye,
                    remove_unfixed_players_flag,
                    year_before,
                    year_after,
                    whitelist,
                    blacklist,
                    fixups,
                    name_fixups
            );
    }
    else
    {
        std::ifstream in(fin);
        if( !in )
        {
            printf( "Error; Cannot open list file %s\n", fin.c_str() );
            return -1;
        }
        std::vector<std::string> pgn_files;
        int line_number = 0;
        for(;;)
        {
            std::string line;
            if( !std::getline(in,line) )
                break;

            // Strip out UTF8 BOM mark (hex value: EF BB BF)
            if( line_number==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                line = line.substr(3);
            line_number++;
            util::ltrim(line);
            util::rtrim(line);
            if( line != "" )
                pgn_files.push_back(line);
        }
        int nbr_files = pgn_files.size();
        bool append=false;
        int file_number=1;
        ok = false;
        for( std::string line : pgn_files )
        {
            printf( "Processing %d of %d pgn file%s\r", file_number++, nbr_files, nbr_files==1?"":"s" );
            bool utf8_bom;
            bool any = pgn2line( line, temp1_fout, diag_fout,
                        utf8_bom,
                        append,
		                reverse_flag,
                        remove_zero_length,
                        remove_zero_length_allow_bye,
                        remove_unfixed_players_flag,
                        year_before,
                        year_after,
                        whitelist,
                        blacklist,
                        fixups,
                        name_fixups );
            if( any )  // don't give up unless none of the files are processed
            {
                ok = true;
                if( !utf8_bom )
                    all_utf8_bom = false;
            }
            append = true;
        }
    }
    if( !ok )
        return -1;
    bool add_utf8_bom_to_output = all_utf8_bom;
    if( no_sort )
	{
		printf( "Removing tie breaker field\n");
        remove_tie_breaker_and_dups( temp1_fout, fout, add_utf8_bom_to_output, NULL, true );
		remove( temp1_fout.c_str() );
	}
    else
    {
        std::ofstream out_smart_uniq;
        std::string smart_uniq_msg;
        if( smart_uniq )
        {
            std::string dedup_fout = "dedup-" + fout;
            smart_uniq_msg = util::sprintf( ", use file %s to review smart de-duplication decisions", dedup_fout.c_str() );
            out_smart_uniq.open( dedup_fout.c_str() );
            if( !out_smart_uniq )
            {
                printf( "Warning; Cannot open smart deduplication file %s for writing, so smart dedup disabled\n", dedup_fout.c_str() );
                smart_uniq_msg = "";
            }
        }
        std::ofstream *p_smart_uniq = (smart_uniq && out_smart_uniq) ? &out_smart_uniq : 0;
        printf( "%sStarting sort\n", list_flag?"\n":"" );   // list_flag = newline needed
        disksort( temp1_fout, temp2_fout );
	    remove( temp1_fout.c_str() );
        printf( "Sort complete\n");
        printf( "Starting refinement sort\n");
		refine_sort( temp2_fout, temp1_fout );
		printf( "Refinement sort complete\n");
        remove( temp2_fout.c_str() );
	    if( reverse_flag )
	    {
		    printf( "Starting reversal sort\n");
		    disksort( temp1_fout, temp2_fout, true );
		    printf( "Reversal sort complete\n");
		    remove( temp1_fout.c_str() );
		    printf( "Removing tie breaker field and dups%s\n", smart_uniq_msg.c_str() );
            remove_tie_breaker_and_dups( temp2_fout, fout, add_utf8_bom_to_output, p_smart_uniq );
		    remove( temp2_fout.c_str() );
	    }
	    else
	    {
		    printf( "Removing tie breaker field and dups%s\n", smart_uniq_msg.c_str() );
            remove_tie_breaker_and_dups( temp1_fout, fout, add_utf8_bom_to_output, p_smart_uniq );
		    remove( temp1_fout.c_str() );
	    }
    }
    if( pgn_create_flag )
        line2pgn( fout, fout + ".pgn" );
    return 0;
#endif
}

class Game
{
public:
	static unsigned int game_count;
    Game() {clear();}
    void clear();
    void process_header_line( const std::string &line );
    void process_moves_line( const std::string &line );
    bool is_game_non_zero_length();
    bool is_game_non_zero_length_or_BYE();
    bool is_both_players_fixed();
    std::string get_game_as_line(bool reverse_order);
    void fixup_tournament( const std::map<std::string,std::string> &fixup_list );
    void fixup_names( const std::map<std::string,std::string> &fixup_list, std::ofstream *p_out_diag );
    std::string get_yyyy_event_at_site();
    std::string get_prefix(bool reverse_order);
    std::string get_description();
    int yyyy;
private:
    std::string get_event_header();
    std::string get_site_header();
    std::string headers;
    std::string moves;
    std::string eventx;
    std::string site;
    std::string white;
    std::string black;
    std::string date;
    std::string year;
    std::string month;
    std::string day;
    std::string round;
    std::string result;
    bool both_players_fixed;
    int move_txt_len;
	unsigned int game_idx;
};

unsigned int Game::game_count = 0;

void Game::clear()
{
    headers.clear();
    moves.clear();
    eventx.clear();
    site.clear();
    white.clear();
    black.clear();
    date.clear();
    yyyy = 0;
    year = "0000";
    month = "00";
    day = "00";
    round.clear();
    result.clear();
    both_players_fixed = false;
    move_txt_len = 0;
	game_idx = 0;
}

std::string Game::get_game_as_line(bool reverse_order)
{
    std::string s=get_prefix(reverse_order);
    s += "@H";
    s += get_event_header();
    s += "@H";
    s += get_site_header();
    s += headers;
    s += moves;
    return s;
}

std::string Game::get_yyyy_event_at_site()
{
    std::string s = year; 
    s += ' ';
    s += eventx;
    s += '@';
    s += site;
    return s;
}

void Game::fixup_tournament( const std::map<std::string,std::string> &fixup_list )
{
    std::string t = get_yyyy_event_at_site();
    bool found=false;
    auto it = fixup_list.find(t);
    if( it != fixup_list.end() )
    {
        std::string to = it->second;
        auto offset = to.find('@');
        assert( offset != std::string::npos );
        eventx = to.substr(5,offset-5);
        site   = to.substr(offset+1);
    }
}

void Game::fixup_names( const std::map<std::string,std::string> &fixup_list, std::ofstream *p_out_diag )
{
    std::string t = get_yyyy_event_at_site();
    int nbr_changes=0;
    auto it1 = fixup_list.find(white);
    auto it2 = fixup_list.find(black);
    if( it1 != fixup_list.end()  || it2 != fixup_list.end() )
    {
        std::string before = get_description();
        if( it1 != fixup_list.end() )
        {
            nbr_changes++;
            std::string new_white = it1->second;
            std::string from = "@H[White \"" + white + "\"]";
            std::string to   = "@H[White \"" + new_white + "\"]";
            util::replace_once( headers, from, to );
            white = new_white;
        }
        if( it2 != fixup_list.end() )
        {
            nbr_changes++;
            std::string new_black = it2->second;
            std::string from = "@H[Black \"" + black + "\"]";
            std::string to   = "@H[Black \"" + new_black + "\"]";
            util::replace_once( headers, from, to );
            black = new_black;
        }
        std::string after = get_description();
        if( p_out_diag )
        {
            std::string
            s = util::sprintf( "Name change%s; %s", nbr_changes>1?"s":"", before.c_str() );
            util::putline(*p_out_diag,s);
            s = util::sprintf( "%s       ->    %s", nbr_changes>1?" ":"", after.c_str() );
            util::putline(*p_out_diag,s);
        }
    }
    both_players_fixed = (nbr_changes==2);
}

std::string Game::get_prefix(bool reverse_order)
{
    std::string event_site = eventx;
    event_site += ", ";
    event_site += site;
    util::replace_all( event_site, "#", "##" ); // very important the event and/or site don't happen to contain string " # "
    std::string s = year;
    s += '-';
    s += month;
    s += '-';
    s += day;
    s += ' ';
    s += event_site;
    s += " # ";
    s += year;
    s += '-';
    s += month;
    s += '-';
    s += day;
    s += ' ';

	// eg Round = "3" -> "003" or if reverse order "997"
	// eg Round = "3.1" -> "003.001" or if reverse order "997.999"
	// eg Round = "3.1.2" -> "003.001.002" or if reverse order "997.999.998"
	// The point is to make the text as usefully sortable as possible
    std::string sround = round;
    size_t offset = sround.find_first_of('.');
    while( offset != std::string::npos )
    {
		std::string temp = sround.substr(0,offset);
		int iround = atoi(temp.c_str());
		if( reverse_order )
			iround = 1000-iround;
		temp = util::sprintf("%03d",iround);
		s += temp;
		s += '.';
		sround = sround.substr(offset+1);
        offset = sround.find_first_of('.');
    }
	int iround = atoi(sround.c_str());
	if( reverse_order )
		iround = 1000-iround;
	std::string temp = util::sprintf("%03d",iround);
	s += temp;

	// New feature, append the game idx in original file as a sort tie breaker,
	//  in case round doesn't have a board number. Eg in a tournament you might
	//  have Round 3.1, 3.2, 3.3 etc (great). But if you just have Round 3 for
	//  all these games, without this tie breaker the games end up sorted
	//  according to White's Surname (not very helpful). The tie breaker means
	//  that the sort order will be the same as in the original .pgn, which is
	//  likely to be an improvement over White's surname.
	s += ' ';
	std::string sgame_idx = util::sprintf("%09u", game_idx);
	s += sgame_idx;

	// Add " White-Black"
	s += ' ';
    offset = white.find_first_of(",");
    std::string one_word;
    if( offset != std::string::npos )
        one_word = white.substr(0,offset);
    else
        one_word = white;
    util::replace_all( one_word, " ", "_" );
    s += one_word;
    s += "-";
    offset = black.find_first_of(",");
    if( offset != std::string::npos )
        one_word = black.substr(0,offset);
    else
        one_word = black;
    util::replace_all( one_word, " ", "_" );
    s += one_word;
    return s;
}

std::string Game::get_description()
{
    std::string s;
    s = white;
    s += " - ";
    s += black;
    s += " - ";
    s += eventx;
    s += ", ";
    s += site;
    s += ", ";
    s += year;
    s += '-';
    s += month;
    s += '-';
    s += day;
    return s;
}

bool Game::is_game_non_zero_length()
{
    return( move_txt_len > static_cast<int>(result.length()) );
}

bool Game::is_game_non_zero_length_or_BYE()
{
    if( move_txt_len > static_cast<int>(result.length()) )
        return true;
    std::string w = util::toupper(white);
    std::string b = util::toupper(black);
    return( w=="BYE" || b=="BYE" );
}

bool Game::is_both_players_fixed()
{
    return both_players_fixed;
}

void Game::process_header_line( const std::string &line )
{
	if (game_idx == 0)
		game_idx = ++game_count;
    bool event_site=false;
    bool white_black=false;
    std::string key, value;
    int from = line.find_first_not_of(' ',1);
    int to   = line.find_first_of(' ');
    if( std::string::npos != from && std::string::npos != to && to>from )
    {
        key = line.substr( from, to-from );
        from = line.find_first_of('\"');
        to   = line.find_last_of('\"');
        if( std::string::npos != from && std::string::npos != to && to>from )
        {
            value = line.substr( from+1, (to-from)-1 );
            if( key == "Event" )
            {
                event_site = true;
                util::trim(value);
                eventx = value;
            }
            else if( key == "Site" )
            {
                event_site = true;
                util::trim(value);
                site = value;
            }
            else if( key == "White" )
            {
                white_black = true;
                headers += "@H";
                if( util::trim(value) )
                {
                    headers += "[White \"";
                    headers += value;
                    headers += "\"]";
                }
                else
                    headers += line;
                white = value;
            }
            else if( key == "Black" )
            {
                white_black = true;
                headers += "@H";
                if( util::trim(value) )
                {
                    headers += "[Black \"";
                    headers += value;
                    headers += "\"]";
                }
                else
                    headers += line;
                black = value;
            }
            else if( key == "Date" )
            {
                bool ok=false;
                date = value;
                int y,m,d;
                ok = parse_date_format( date, '.', y, m, d );
                if( ok )
                {
                    yyyy = y;
					year = util::sprintf( "%04d", y );
					month = util::sprintf( "%02d", m );
					day = util::sprintf( "%02d", d );
                }
            }
            else if( key == "Round" )
                round = value;
            else if( key == "Result" )
                result = value;
        }
    }
    if( !event_site && !white_black )
    {
        headers += "@H";
        headers += line;
    }
}

void Game::process_moves_line( const std::string &line )
{
    move_txt_len += line.length();
    moves += "@M";
    moves += line;
}

std::string Game::get_event_header()
{
    std::string s = "[Event ";
    s += '\"';
    s += eventx;
    s += '\"';
    s += ']';
    return s;
}

std::string Game::get_site_header()
{
    std::string s = "[Site ";
    s += '\"';
    s += site;
    s += '\"';
    s += ']';
    return s;
}

static bool pgn2line( std::string fin, std::string fout, std::string diag_fout,
                    bool &utf8_bom,
                    bool append,
                    bool reverse_order,
                    bool remove_zero_length,
                    bool remove_zero_length_allow_bye,
                    bool remove_unfixed_players_flag,
                    int year_before,
                    int year_after,
                    const std::set<std::string> &whitelist,
                    const std::set<std::string> &blacklist,
                    const std::map<std::string,std::string> &fixups,
                    const std::map<std::string,std::string> &name_fixups )
{
    Game game;
    utf8_bom = false;
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return false;
    }
    std::ofstream out( fout.c_str(), append ? std::ios_base::app : std::ios_base::out );
    if( !out )
    {
        printf( "Error; Cannot open file %s for %s\n", fout.c_str(), append?"appending":"writing" );
        return false;
    }
    std::ofstream out_diag;
    if( diag_fout != "" )
    {
        out_diag.open( diag_fout.c_str(), append ? std::ios_base::app : std::ios_base::out );
        if( !out_diag )
            printf( "Warning; Cannot open diagnostic file %s for writing\n", diag_fout.c_str() );
    }
    std::ofstream *p_out_diag = out_diag ? &out_diag : NULL;
    int line_number=0;
    enum {search_for_header,start_header,in_header,process_header,
            search_for_moves,in_moves,process_game,
            process_game_and_exit,done} state=search_for_header;
    std::string line;
    bool next_line=true;
    while( state != done )
    {

        // Get next line (unless we haven't fully processed current line)
        if( next_line )
        {
            if( !std::getline(in, line) )
            {
                if( state == in_moves )
                {
                    state = process_game_and_exit;
                    printf( "Warning; File: %s, No empty line at end\n", fin.c_str() );
                }
                else
                    state = done;
            }
            else
            {
                // Strip out UTF8 BOM mark (hex value: EF BB BF)
                if( line_number==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                {
                    line = line.substr(3);
                    utf8_bom = true;
                }
                util::ltrim(line);
                util::rtrim(line);
                line_number++;

                // Escape out existing '@' characters
                if( std::string::npos != line.find('@') )   // a little optimisation, '@' chars are rare
                    util::replace_all(line, "@", "@$");     //  if it is there transform "@" -> "@$"
                                                            //  line2pgn reverts it back
                                                            //  (this avoids possible trouble with
                                                            //  false @H and @M special markers)
            }
        }
        next_line = false;  // state machine often involves stepping through states before going to next line

        // State machine
        switch( state )
        {
            default:
            case done:
            {
                state = done;
                break;
            }

            case search_for_header:
            {
                if( util::prefix(line,"[") && util::suffix(line,"]") )
                    state = start_header;
                else
                    next_line = true;
                break;
            }

            case start_header:
            {
                game.clear();
                state = in_header;
                break;
            }

            case in_header:
            {
                if( util::prefix(line,"[") && util::suffix(line,"]") )
                {
                    game.process_header_line(line);
                    next_line = true;
                }
                else
                {
                    // All headers are in, apply fixups
                    if( fixups.size() > 0 )
                        game.fixup_tournament(fixups);

                    if( name_fixups.size() > 0 )
                        game.fixup_names(name_fixups,p_out_diag);

                    // Next get moves
                    state = search_for_moves;
                    if( line != "" )
                        printf( "Warning; File: %s Line: %d, No gap between header and moves\n", fin.c_str(), line_number );
                }
                break;
            }

            case search_for_moves:
            {
                if( util::prefix(line,"[") && util::suffix(line,"]") )
                {
                    printf( "Warning; File: %s Line: %d, No moves to go with header\n", fin.c_str(), line_number );
                    state = search_for_header;
                }
                else if( line != "" )
                {
                    state = in_moves;
                }
                else
                {
                    next_line = true;
                }
                break;
            }

            case in_moves:
            {
                if( line == "" )
                {
                    state = process_game;
                }
                else if( util::prefix(line,"[") && util::suffix(line,"\"]") )
                {
                    state = process_game;
                    printf( "Warning; File: %s Line: %d, No gap between games\n", fin.c_str(), line_number );
                }
                else
                {
                    game.process_moves_line( line );
                    next_line = true;
                }
                break;
            }

            case process_game:
            case process_game_and_exit:
            {
                state = (state==process_game_and_exit ? done : search_for_header);
                bool ok = (game.yyyy>=year_after && game.yyyy<=year_before);
                if( ok && remove_zero_length_allow_bye )
                    ok = game.is_game_non_zero_length_or_BYE();
                if( ok && remove_zero_length )
                    ok = game.is_game_non_zero_length();
                if( ok && remove_unfixed_players_flag )
                    ok = game.is_both_players_fixed();
                std::string t = game.get_yyyy_event_at_site();
                if( ok && whitelist.size() > 0)
                {
                    ok = false; // discard game unless tournament is in the white list
                    auto it = whitelist.find(t);
                    if( it != whitelist.end() )
                        ok = true;  // tournament is in the whitelist
                }
                if( ok && blacklist.size() > 0 )
                {
                    auto it = blacklist.find(t);
                    if( it != blacklist.end() )
                        ok = false; // tournament is in the blacklist
                }
                if( ok )
                {
                    std::string line_out = game.get_game_as_line(reverse_order);
                    util::putline(out,line_out);
                }
                break;
            }
        }
    }
    return true;
}

static void remove_tie_breaker_and_dups( std::string fin, std::string fout, bool add_utf8_bom_to_output, std::ofstream *p_smart_uniq, bool no_deduping_at_all )
{

/*
    Line by line transformation
    In:  2001-12-28 Acme Open, Gotham # 2001-12-31 003.002.001 000000001 Smith-Jones
    Out: 2001-12-28 Acme Open, Gotham # 2001-12-31 003.002.001 Smith-Jones
 */

    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ofstream out(fout);
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return;
    }
    if( add_utf8_bom_to_output )
        out.write( "\xef\xbb\xbf", 3 );
    for(;;)
    {
        std::string line;
        std::string line_out;
        bool expected_format = false;
        if( !std::getline(in,line) )
            break;
        std::string prefix_end = " # ";     // we double up earlier '#' chars to make sure
                                            //  this doesn't occur earlier in prefix
        size_t offset = line.find( prefix_end );
        if( offset != std::string::npos )
        {
            offset += 3;
            size_t offset2 = line.find( ' ', offset );
            if( offset2!=std::string::npos && offset2==offset+10 )  
            {
                offset2++;
                size_t offset3 = line.find( ' ', offset2 );
                if( offset3 != std::string::npos )
                {
                    offset3++;
                    size_t offset4 = line.find( ' ', offset3 );
                    if( offset4!=std::string::npos && offset4==offset3+9 )  
                    {
                        offset4++;
                        expected_format = true;
                        for( int i=0; expected_format && i<9; i++ )
                        {
                            char c = line[offset3+i];
                            expected_format = (isascii(c) && isdigit(c));
                        }
                        if( expected_format )
                            line_out = line.substr(0,offset3) + line.substr(offset4);
                    }
                }
            }
        }
        if( !expected_format )
            line_out = line;
        if( !no_deduping_at_all )
            postponed_dedup_filter( false,line_out,out,p_smart_uniq );
        else
            util::putline( out,line_out );        // straight out without going through dedup filter
    }
    if( !no_deduping_at_all )
        postponed_dedup_filter(true, "", out, p_smart_uniq );
}                                                                     

static void line2pgn( std::string fin, std::string fout )
{
    Game game;
    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ofstream out(fout);
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return;
    }
    enum {in_prefix1,in_prefix2,in_header1,in_header2,in_moves1,in_moves2,finished} state=in_prefix1, old_state=in_prefix2;
    bool first_line = true;
    for(;;)
    {
        std::string line;
        std::string line_out;
        if( !std::getline(in,line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF). If it's there, put it in PGN
        if( first_line && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65 )
        {
            line = line.substr(3);
            out.write( "\xef\xbb\xbf", 3 );
        }
        first_line = false;
        const char *p = line.c_str();
        state=in_prefix1;
        old_state=in_prefix2;
        for(;;)
        {
            bool finished_header = false;
            bool finished_moves  = false;
            char c = *p++;
            old_state = state;
            switch( state )
            {
                case in_prefix1:
                {
                    if( c == '\0' )
                        state = finished;
                    else if( c == '@' )
                        state = in_prefix2;
                    break;
                }

                case in_prefix2:
                {
                    if( c == '\0' )
                        state = finished;
                    else if( c == 'H' )
                        state = in_header1;
                    else
                        state = in_prefix1; //go back
                    break;
                }

                case in_header1:
                {
                    if( c == '\0' )
                    {
                        finished_header = true;
                        state = finished;
                    }
                    else if( c == '@' )
                        state = in_header2;
                    else
                        line_out += c;
                    break;
                }

                case in_header2:
                {
                    if( c == '\0' )
                    {
                        finished_header = true;
                        state = finished;
                    }
                    else if( c == 'H' )
                    {
                        finished_header = true;
                        state = in_header1;
                    }
                    else if( c == 'M' )
                    {
                        finished_header = true;
                        state = in_moves1;
                    }
                    else
                    {
                        state = in_header1;  // @$ -> @ (normal @ character in data - not @H or @M),
                        line_out += '@';     //  so we expect c == '$' but not much point checking
                    }
                    break;
                }

                case in_moves1:
                {
                    if( c == '\0' )
                    {
                        finished_moves = true;
                        state = finished;
                    }
                    else if( c == '@' )
                        state = in_moves2;
                    else
                        line_out += c;
                    break;
                }

                case in_moves2:
                {
                    if( c == '\0' )
                    {
                        finished_moves = true;
                        state = finished;
                    }
                    else if( c == 'M' )
                    {
                        finished_moves = true;
                        state = in_moves1;
                    }
                    else
                    {
                        state = in_moves1;  // @$ -> @ (normal @ character in data - not @H or @M),
                        line_out += '@';    //  so we expect c == '$' but not much point checking
                    }
                    break;
                }
            }
            if( state != old_state )
            {
                if( finished_header )
                {
                    util::putline(out,line_out);
                    line_out.clear();
                    if( state == in_moves1 )
                        util::putline(out,"");
                }
                if( finished_moves )
                {
                    util::putline(out,line_out);
                    line_out.clear();
                    if( state == finished )
                        util::putline(out,"");
                }
            }
            if( c == '\0' )
                break;
        }
    }
}

static void tournaments( std::string fin, std::string fout, bool bare )
{
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ostream* fp = &std::cout;
    std::ofstream out;
    if( fout != "" )
    {
        out.open(fout);
        if( out )
            fp = &out;
        else
        {
            printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
            return;
        }
    }
    int nbr_games = 0;
    int total_games = 0;
    enum {first_time_thru,wait_for_tournament,in_tournament,new_tournament,
            print_tournament,print_tournament_and_finish,finished
         } state=first_time_thru;
    bool have_tournament=false;
    std::string line;
    std::string line_tournament;
    std::string tournament;
    std::string tournament_list_form;
    int line_number=0;
    while( state != finished )
    {
        bool replay_line=false;
        switch( state )
        {
            case first_time_thru:
            {
                state = wait_for_tournament;
                break;
            }

            case wait_for_tournament:
            {
                if( have_tournament )
                {
                    replay_line = true;
                    state = new_tournament;
                }
                break;
            }

            case new_tournament:
            {
                state = in_tournament;
                tournament = line_tournament;
                nbr_games = 1;
                total_games++;

                // Ironically, getting the "bare" tournament description is much harder
                //  because it's not just the prefix before " # "
                tournament_list_form = tournament;  // fallback
                if( bare )
                {
                    // Extract Event and Site from headers - pattern is; a Event b Site c
                    std::string a="@H[Event \"";
                    std::string b="\"]@H[Site \"";
                    std::string c="\"]@";
                    size_t a_offset = line.find(a);
                    if( a_offset != std::string::npos )
                    {
                        size_t b_offset = line.find(b,a_offset+a.length());
                        if( b_offset != std::string::npos )
                        {
                            size_t c_offset = line.find(c,b_offset+b.length());
                            if( c_offset != std::string::npos )
                            {
                                size_t event_offset = a_offset + a.length();
                                size_t event_length = b_offset-event_offset;
                                size_t site_offset = b_offset + b.length();
                                size_t site_length = c_offset-site_offset;
                                std::string eventx = line.substr(event_offset,event_length);
                                std::string site   = line.substr(site_offset,site_length);
                                std::string yyyy   = line.substr(0,4);
                                std::string s;
                                tournament_list_form = util::sprintf( "%s %s@%s", yyyy.c_str(), eventx.c_str(), site.c_str() );
                            }
                        }
                    }
                }
                break;
            }

            case in_tournament:
            {
                if( tournament == line_tournament )
                {
                    nbr_games++;
                    total_games++;
                }
                else
                {
                    replay_line = true;
                    state = print_tournament;
                }
                break;
            }

            case print_tournament:
            case print_tournament_and_finish:
            {
                replay_line = true;
                if( state == print_tournament_and_finish )
                    state = finished;
                else
                    state = have_tournament ? new_tournament : wait_for_tournament;
                std::string s;
                if( bare )
                    s = util::sprintf( "%s", tournament_list_form.c_str() );
                else
                    s = util::sprintf( "%s (%d game%s)", tournament.c_str(), nbr_games, nbr_games==1?"":"s" );
                util::putline(*fp,s);
                break;
            }
        }
        if( !replay_line )
        {
            if( !std::getline(in,line) )
                state = (state==in_tournament ? print_tournament_and_finish : finished);
            else
            {
                // Strip out UTF8 BOM mark (hex value: EF BB BF)
                if( line_number==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                    line = line.substr(3);
                line_number++;

                std::string prefix_end = " # ";     // we double up earlier '#' chars to make sure
                                                    //  this doesn't occur earlier in prefix
                size_t prefix_end_offset=0;
                prefix_end_offset = line.find( prefix_end );
                have_tournament = (prefix_end_offset != std::string::npos);
                line_tournament = have_tournament ? line.substr(0,prefix_end_offset) : "";
            }
        }
    }
    if( !bare )
    {
        std::string s;
        s = util::sprintf( "%d total game%s", total_games, total_games==1?"":"s" );
        util::putline(*fp,s);
    }
}


static void players( std::string fin, std::string fout, bool bare, bool dups_only )
{
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ostream* fp = &std::cout;
    std::ofstream out;
    if( fout != "" )
    {
        out.open(fout);
        if( out )
            fp = &out;
        else
        {
            printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
            return;
        }
    }
    std::string prefix1 = "[White \"";
    std::string prefix2 = "[Black \"";
    size_t prefix_len = prefix1.length();
    assert( prefix_len == prefix2.length() );
    std::string line;
    std::map<std::string,int> names;
    int line_number = 0;
    for(;;)
    {
        if( !std::getline(in,line) )
            break;
        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_number==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_number++;
        std::string player_name;
        std::string prefix = prefix1;
        for( int i=0; i<2; i++ )
        {
            size_t offset = line.find(prefix);
            if( offset != std::string::npos )
            {
                offset += prefix_len;
                size_t offset2 = line.find("\"",offset);
                if( offset2 != std::string::npos )
                {
                    player_name = line.substr(offset,offset2-offset);
                    auto it = names.find(player_name);
                    if( it == names.end() )
                        names.insert( std::pair<std::string,int>(player_name,1) );
                    else
                        it->second++;
                }
            }
            prefix = prefix2;
        }
    }
    std::vector<std::string> recent_names;
    std::vector<std::string> recent_surnames;
    for( auto it=names.begin(); it!=names.end(); it++ )
    {
        std::string name = it->first.c_str();
        int count  = it->second;
        std::string surname = name;
        size_t offset = name.find(',');
        bool comma_present = (offset != std::string::npos);
        if( comma_present )
            surname = name.substr(0,offset);
        std::string detail;
        if( bare )
            detail = util::sprintf( "%s%s", comma_present?"":"@ ", name.c_str() );
        else
            detail = util::sprintf( "%s%s: %d", comma_present?"":"@ ", name.c_str(), count );
        if( !dups_only )
            util::putline(*fp,detail);
        else
        {

            // Always flush after last name
            it++;
            bool flush = (it == names.end());
            it--;

            // If empty, simply buffer for later
            if( recent_surnames.size() == 0 )
            {
                recent_names.push_back( detail );
                recent_surnames.push_back( surname );
            }

            // Else if not empty flush if surname changes
            else
            {
                bool different = (surname != recent_surnames[recent_surnames.size()-1]);
                if( different )
                    flush = true;

                // Else multiple matching names in a row
                else
                {
                    recent_names.push_back( detail );
                    recent_surnames.push_back( surname );
                }
            }

            // If flush, dump multiple matching names in a row
            if( flush )
            {
                if( recent_names.size() > 1 )
                {
                    for( auto it2=recent_names.begin(); it2!=recent_names.end(); it2++ )
                        util::putline(*fp,*it2);
                }

                // Clear the buffer and add the latest line, possibly first of a run of matching lines
                recent_names.clear();
                recent_surnames.clear();
                recent_names.push_back( detail );
                recent_surnames.push_back( surname );
            }
        }
    }
}

/*

The refine_sort() function refines an already sorted (alphabetically) file of game lines.

We group together tournament games that share the same Event and Site in any 6 month window.
We achieve that by replacing the proxy tournament date with the real tournament date and
re-sorting. The previous sort means that the tournament date is now easily determined - it
is simply the first date encountered for the tournament.

Example:

2016-11-01 Brazil Champs, Brazil # 2016-11-01 Game 1...
2016-11-02 Andorra Champs, Andorra # 2016-11-02 Game 2...
2016-11-03 Brazil Champs, Brazil # 2016-11-03 Game 3...
2016-12-01 Andorra Champs, Andorra # 2016-12-01 Game 4...
2016-12-02 Zambia Champs, Zambia # 2016-12-02 Game 5...
2017-01-01 Andorra Champs, Andorra # 2017-01-01 Game 6...
2017-01-02 Brazil Champs, Brazil # 2017-01-02 Game 7...

    |
modify tournament date and re - sort which yields
    |
    v

2016-11-01 Brazil Champs, Brazil # 2016-11-01 Game 1...
2016-11-01 Brazil Champs, Brazil # 2016-11-03 Game 3...
2016-11-02 Andorra Champs, Andorra # 2016-11-02 Game 2...
2016-11-02 Andorra Champs, Andorra # 2016-12-01 Game 4...
2016-11-02 Andorra Champs, Andorra # 2017-01-01 Game 6...  <-this game shifted up, because the Andorra Champs ran across all 3 months
2016-12-02 Zambia Champs, Zambia # 2016-12-02 Game 5...
2017-01-02 Brazil Champs, Brazil # 2017-01-02 Game 7...  <-this game didn't because although active 2016-11 tournaments are still
                                                            remembered in January 2017, the 2016 11 Brazil Champs aren't, they
                                                            were discarded after processing December 2016 because they didn't
                                                            feature in that month. So this is considered the start of a new Brazil
                                                            Champs
*/


struct Tournament
{
    std::string start_date;
    bool hit=false;
};

struct Month
{
    bool hit=false;
    int yyyy;
    int mm;
    std::string yyyy_mm;
    std::map<std::string,Tournament> tournaments;
};

static bool refine_sort( std::string fin, std::string fout )
{
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error, cannot open file %s for reading\n", fin.c_str() );
        return false;
    }
    std::ofstream out(fout.c_str());
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return false;
    }

    enum {first_time_thru,new_month,buffering,flush_and_exit} state=first_time_thru;
    std::string line;
    bool bad=false;
    int yyyy, mm;
    std::string game_date;
    std::string tournament_description;
    std::deque<std::string> main_buffer;
    std::deque<Month> months;
    Month m;
    bool done=false;
    while( !done )
    {
        bool replay_line=false;
        bool resolve=false;
        switch( state )
        {
            default:
            case first_time_thru:
            {
                state = new_month;
                break;
            }

            case new_month:
            {
                if( bad )
                {
                    util::putline(out,line);
                }
                else
                {
                    state = buffering;
                    m.hit = false;
                    m.yyyy = yyyy;
                    m.mm = mm;
                    m.yyyy_mm = util::sprintf("%04d-%02d",yyyy,mm);
                    m.tournaments.clear();
                    replay_line = true;
                }
                break;
            }

            case buffering:
            {
                if( bad )
                {
                    main_buffer.push_back(line);
                    resolve = true;
                    state = new_month;
                }
                else
                {

                    // Same month as previous line?
                    if( yyyy==m.yyyy && mm==m.mm )
                    {

                        // Search though previous buffered months, oldest first, looking for the same
                        //  tournament description. Note that tournaments are erased if they aren't
                        //  used in every successive month. In other words, a tournament from January
                        //  won't be retained when we're processing March, unless it scored a hit in
                        //  February
                        bool found=false;
                        std::string tournament_start_date;
                        for( unsigned int i=0; !found && i<months.size(); i++)
                        {
                            Month &previous_month = months[i];
                            auto it = previous_month.tournaments.find(tournament_description);
                            if( it != previous_month.tournaments.end() )
                            {
                                found = true;
                                it->second.hit = true;
                                previous_month.hit = true;
                                tournament_start_date = it->second.start_date;
                            }
                        }

                        // If we didn't find it, look for it in this month's set of tournaments
                        //  if it's not there, add it
                        if( !found )
                        {
                            auto it = m.tournaments.find(tournament_description);
                            if( it != m.tournaments.end() )
                                tournament_start_date = it->second.start_date;
                            else
                            {
                                Tournament t;
                                t.start_date = game_date;   // the initial disk sort means this will be the
                                                            //  start date of the whole tournament
                                tournament_start_date = game_date;
                                m.tournaments.insert(std::pair<std::string,Tournament>(tournament_description,t));
                            }
                        }

                        // One way or another we now know the tournament start date, change the proxy
                        //  tournament start date to the real tournament start date and buffer line
                        line.replace(0,tournament_start_date.length(),tournament_start_date);
                        main_buffer.push_back(line);
                    }

                    // Move to next month?
                    else if( (yyyy==m.yyyy && mm==m.mm+1) || (yyyy==m.yyyy+1 && mm==1 && m.mm==12) )
                    {
                        // Re-sort because lines have been modified by replacing the proxy tournament
                        //  start date with the real tournament start date
                        std::sort( main_buffer.begin(), main_buffer.end() );

                        // Discard old or unused months 
                        while( months.size() )
                        {

                            // Months is restricted to never hold any more than the 6 previous months,
                            //  So if a tournament reappears every month for more than 6 months, stop
                            //  trying to associate it to that first month
                            Month &old_month = months[0];
                            if( months.size()>=6 || !old_month.hit )
                            {

                                // Write lines even older than old_month out to file
                                while( main_buffer.size() )
                                {
                                    std::string l = main_buffer[0];
                                    if( l.substr(0,7) == old_month.yyyy_mm )
                                        break;
                                    else
                                    {
                                        util::putline(out,l);
                                        main_buffer.pop_front();
                                    }
                                }

                                // Pop the old and/or disused month off the front of the queue
                                // Note this invalidates old_month, don't do it too soon!
                                months.pop_front();
                            }
                            else
                                break;  // stop when we reach a buffered month that's been hit
                                        //  during this month's processing
                        }

                        // Clear previous month hits
                        for( unsigned int i=0; i<months.size(); i++ )
                        {
                            Month &previous_month = months[i];
                            previous_month.hit = false;
                            for( auto it=previous_month.tournaments.begin(); it!=previous_month.tournaments.end();  )
                            {
                                if( !it->second.hit ) // if tournament description isn't hit every month, erase it
                                    it = previous_month.tournaments.erase(it);
                                else
                                {
                                    it->second.hit = false;
                                    it++;
                                }
                            }
                        }

                        // Current month is buffered
                        months.push_back(m);

                        // Start a new month
                        state = new_month;
                        replay_line = true;
                    }

                    // Else discontinuity
                    else
                    {
                        resolve = true;

                        // Start a new month
                        state = new_month;
                        replay_line = true;
                    }
                }
                break;
            }

            case flush_and_exit:
            {
                resolve = true;
                done = true;
            }
        }

        // Write out buffer and clear previous months
        if( resolve )
        {
            std::sort( main_buffer.begin(), main_buffer.end() );
            while( main_buffer.size() )
            {
                std::string l = main_buffer[0];
                main_buffer.pop_front();
                util::putline(out,l);
            }

            // Discard old months
            months.clear();
        }

        // Get next line and validate it
        if( !replay_line && !done )
        {
            if( !std::getline(in,line) )
                state = flush_and_exit;
            else
            {
                // Validate line, should have format "yyyy-mm-dd Event, Site # yyyy-mm-dd etc"
                //  First date is tournament start date, second date is game date.
                const size_t event_offset=11;
                bool ok = line.length()>event_offset && line[event_offset-1]==' ';
                int dd;
                if( ok && parse_date_format(line,'-',yyyy,mm,dd) )
                {
                    ok = false;
                    size_t offset = line.find(" # ");
                    if( std::string::npos != offset )
                    {
                        tournament_description = line.substr(event_offset, offset-event_offset);
                        offset += 3;
                        int y,m,d;
                        if( parse_date_format(line.substr(offset),'-',y,m,d) )
                        {
                            ok = true;
                            game_date = util::sprintf( "%04d-%02d-%02d",y,m,d);
                        }
                    }
                }

                // Set validation status of line
                bad = !ok;
            }
        }
    }
    return true;
}


// Read list of tournaments
static bool read_tournament_list( std::string fin, std::vector<std::string> &tournaments, std::vector<std::string> *names  )
{
    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s\n", fin.c_str() );
        return false;
    }
    int lines_read=0;
    bool ok=true;
    bool last_was_name = false;
    while( ok )
    {
        std::string line;
        if( !std::getline(in,line) )
            break;
        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( lines_read==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        lines_read++;
        util::rtrim(line);
        if( line.length() < 6 ) // shortest legit line is "0000 @" indicating bad year, empty site and event
            ok = false;
        if( ok )
        {
            if( line[4] != ' ' )
                ok = false;
        }
        if( ok )
        {
            std::string year=line.substr(0,4);
            if( std::string::npos != year.find_first_not_of("0123456789") )
                ok = false;
        }
        if( ok )
        {
            if( std::string::npos == line.find('@'))
                ok = false;
        }
        bool odd_line = (lines_read%2 == 1);    // first line is line 1, and is odd
        if( odd_line )
        {
            if( ok )
                tournaments.push_back(line);
            else if( names )                // If names vector supplied, non-tournaments go into that
            {
                ok = true;
                names->push_back(line);
                last_was_name = true;
            }
            else
                printf( "Error in tournament list file %s line %d: tournament not in expected \"yyyy Site@Event\" format\n", fin.c_str(), lines_read );
        }
        else
        {
            if( last_was_name && !ok )
            {
                ok = true;
                names->push_back(line);
            }
            else if( last_was_name && ok )
            {
                ok = false;
                printf( "Error in tournament list file %s line %d: Expected player name as second in name pair, but got tournament in \"yyyy Site@Event\" format\n", fin.c_str(), lines_read );
            }
            else if( ok )
                tournaments.push_back(line);
            else
            {
                if( names )
                    printf( "Error in tournament list file %s line %d: second tournament in pair not in expected \"yyyy Site@Event\" format\n", fin.c_str(), lines_read );
                else
                    printf( "Error in tournament list file %s line %d: tournament not in expected \"yyyy Site@Event\" format\n", fin.c_str(), lines_read );
            }
            last_was_name = false;
        }
    }
    return ok;
}


static bool test_date_format( const std::string &date, char separator )
{
    // Test only that for yyyy.mm.dd, each of yyyy, mm, and dd are present
    //  and numeric
    bool ok = date.length()>=10 && date[4]==separator && date[7]==separator;
    for( int i=0; ok && i<3; i++ )
    {
        std::string s;
        switch(i)
        {
            case 0: s = date.substr(0,4);   break;
            case 1: s = date.substr(5,2);   break;
            case 2: s = date.substr(8,2);   break;
        }
        ok = (s.find_first_not_of("0123456789?") == std::string::npos);
    }
    return ok;
}

// Don't just test that yyyy.mm.dd are present and numeric - get the numeric values and change
//  mm=0, dd=0 to mm=1, dd=1 - Helps refinement sort not fall off a cliff between December and
// January in cases where there are games with unknown month/day between them
static bool parse_date_format( const std::string &date, char separator, int &yyyy, int &mm, int &dd )
{
    // Test that for yyyy.mm.dd, each of yyyy, mm, and dd are present
    //  and numeric
    bool ok = date.length()>=10 && date[4]==separator && date[7]==separator;
    for( int i=0; ok && i<3; i++ )
    {
        std::string s;
        switch(i)
        {
            case 0: s = date.substr(0,4);
                    yyyy = atoi(s.c_str());
                    break;
            case 1: s = date.substr(5,2);
                    mm = atoi(s.c_str());
                    break;
            case 2: s = date.substr(8,2);
                    dd = atoi(s.c_str());
                    break;
        }
        ok = (s.find_first_not_of("0123456789?") == std::string::npos);
    }
    if( ok )
    {
        if( mm == 0 )
            mm = 1;
        if( dd == 0 )
            dd = 1;
    }
    return ok;
}

// Poor man's grep -w
static void word_search( bool case_insignificant, std::string word, std::string fin, std::string fout )
{
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ostream* fp = &std::cout;
    std::ofstream out;
    if( fout != "" )
    {
        out.open(fout);
        if( out )
            fp = &out;
        else
        {
            printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
            return;
        }
    }
    if( case_insignificant )
        word = util::toupper(word);
    int line_number=0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in,line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_number==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_number++;

        std::string search_line = case_insignificant ? util::toupper(line) : line;
        size_t offset=0;
        while( std::string::npos != (offset=search_line.find(word,offset)) )
        {
            size_t next = offset+word.length();
            char pre=' ', post=' ';
            if( offset>0 )
                pre = search_line[offset-1];
            if( next < search_line.length() )
                post = search_line[next];
            bool hit = (pre==' '||pre=='\'' || pre=='\"' || pre==',' || pre=='@' || pre=='\t')
                       && (post==' '||post=='\'' || post=='\"' || post==',' || post=='@' || post=='\t');
            if( hit )
            {
                util::putline(*fp,line);
                break;
            }
            else
            {
                offset = next;
            }
        }
    }
}

//static bool equal_exact_match( const std::string &s1, const std::string &s2 );
static bool equal_smart_match( const std::string &s1, const std::string &s2 );
static bool equal_prefix_only( const std::string &s1, const std::string &s2 );
static bool equal_moves(const std::string &s1, const std::string &s2);
static void get_main_line( const std::string &s, std::string &main_line );
//static size_t find_sort_tie_breaker_in_prefix( const std::string &prefix );

// We do a bit of LPGN format specific stuff (LPGN = output of pgn2line)
#if 0  // We now dedup AFTER sort tie-breaker fields have been removed
static size_t find_sort_tie_breaker_in_prefix( const std::string &prefix )
{
    // Prefix format is now;
    //  "1984-02-01 Reykjavik Open, Reykjavik # 1984-02-01 01 012345678 Chandler-Taylor"
    //  9 digit number (012345678 here) is the sorting tie breaker offset, it's the
    //  game index, so if everything else matches, try to keep the order from the
    //  original PGNs
    size_t tie_breaker_offset = std::string::npos;
    size_t offset = prefix.find_last_of(' ');
    if( offset != std::string::npos )
    {
        if( offset > 10 )
        {
            size_t base = offset-9;
            if( prefix[base-1] == ' ' )
            {
                std::string tie_breaker = prefix.substr(base,9);
                bool only_digits = (std::string::npos == tie_breaker.find_first_not_of("0123456789"));
                if( only_digits )
                    tie_breaker_offset = base;
            }
        }
    }
    return tie_breaker_offset;
}
#endif

// Compare two strings for equality, if they are in LPGN format, sort tie-breaker fields
//  do not have to match for equality
#if 0  // We now dedup AFTER sort tie-breaker fields have been removed
static bool equal_exact_match( const std::string &s1, const std::string &s2 )
{
    // Fallback
    bool eq = (s1 == s2);

    // Check for LPGN format
    size_t offset1 = s1.find("@H");
    size_t offset2 = s2.find("@H");
    if( !eq && offset1 != std::string::npos && offset2 != std::string::npos && offset1==offset2 )
    {

        // LPGN might still match,
        //  Do suffixes match? (suffix = rest of string after prefix)
        if( s1.substr(offset1) == s2.substr(offset2) )
        {

            // Suffixes match, test if prefixes match
            std::string  prefix1 = s1.substr(0, offset1);
            std::string  prefix2 = s2.substr(0, offset2);
            size_t idx = find_sort_tie_breaker_in_prefix( prefix1 );
            if( idx != std::string::npos )
                for( int i=0; i<9; i++ )
                    prefix1[idx++] = '#';	// fast way of making tie breakers (in throwaway strings) equal
            idx = find_sort_tie_breaker_in_prefix( prefix2 );
            if( idx != std::string::npos )
                for( int i=0; i<9; i++ )
                    prefix2[idx++] = '#';
            eq = (prefix1 == prefix2);
        }
    }
    return eq;
}
#endif

// Compare two LPGN prefixes for equality, sort tie-breaker fields
//  do not have to match for equality
static bool equal_prefix_only( const std::string &s1, const std::string &s2 )
{
    bool eq = false;
    size_t offset1 = s1.find("@H");
    size_t offset2 = s2.find("@H");
    if( offset1 != std::string::npos && offset2 != std::string::npos && offset1==offset2 )
    {
        std::string  prefix1 = s1.substr(0, offset1);
        std::string  prefix2 = s2.substr(0, offset2);
        eq = (prefix1 == prefix2);
    }
    return eq;
}

// Compare moves (main line) of two games for equality
static bool equal_moves(const std::string &s1, const std::string &s2)
{
    std::string main_line1;
    std::string main_line2;
    get_main_line( s1, main_line1 );
    get_main_line( s2, main_line2 );
    return( main_line1.length() > 10 && main_line1 == main_line2 ); // non-trivial and equal
}

static void get_main_line( const std::string &s, std::string &main_line )
{
    main_line.clear();
    size_t offset = s.find("@M");
    if( offset == std::string::npos )
        return;
    offset += 2;
    int nest_depth = 0;
    std::string move;
    size_t len = s.length();
    enum {in_main_line,in_move,in_variation,in_comment} state=in_main_line, old_state=in_main_line, save_state=in_main_line;
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
                if( c == '}' )
                    state = save_state;
                break;
            }
        }
        if( state != old_state )
        {
            if( state == in_comment )
                save_state = old_state;
            else if( old_state == in_move )
            {
                if( main_line == "" )
                    main_line = util::tolower(move);
                else
                {
                    main_line += ' ';
                    main_line += util::tolower(move);
                }
            }
        }
    }
}

static bool equal_smart_match( const std::string &s1, const std::string &s2 )
{
    return equal_prefix_only(s1,s2) && equal_moves( s1,s2);
}

struct CANDIDATE
{
    std::string line;
    bool keep;
};

static std::vector<CANDIDATE> postponed_dedup;

static bool sort_func(const CANDIDATE* lhs, const CANDIDATE* rhs)
{
    return (lhs->line) < (rhs->line);
}

static void postponed_dedup_filter( bool flush, const std::string &line, std::ofstream &out, std::ofstream *p_smart_uniq )
{
    static std::string cached_day;
    bool have_line = !flush;

    // Calculate the tournament plus date = 'day', eg
    // line = "2001-12-28 Acme Open, Gotham # 2001-12-31 003.002.001 Smith-Jones...
    // day  = "2001-12-28 Acme Open, Gotham # 2001-12-31"
    std::string day;
    if( have_line )
    {
        std::string prefix_end = " # ";     // we double up earlier '#' chars to make sure
                                            //  this doesn't occur earlier in prefix
        size_t offset = line.find(prefix_end);
        if( offset != std::string::npos )
        {
            offset += 3;
            size_t offset2 = line.find(' ', offset);
            if( offset2 != std::string::npos && offset2 == offset + 10)
                day = line.substr(0, offset2);
        }
        if( postponed_dedup.size() >= 1 && cached_day != day)
            flush = true;  // flush buffered lines, before storing this one
    }

    // All games in one 'day' are collected together and deduped
    if( flush )
    {
        std::vector<CANDIDATE*> sorted;
        for( CANDIDATE &c: postponed_dedup )
            sorted.push_back( &c );
        std::sort( sorted.begin(), sorted.end(), sort_func );

        // Smart deduplication ?
        if( p_smart_uniq )
        {
            size_t len = sorted.size();
            bool in_run=false;
            unsigned int run_idx = 0;
            unsigned int run_len = 0;
            for( unsigned int i=1; i<len; i++ )
            {

                // Monitor runs of duplicate games
                CANDIDATE *p = sorted[i];
                CANDIDATE *q = sorted[i-1];
                bool match = (p->line==q->line) || equal_smart_match( p->line, q->line );
                if( match )
                {
                    if( in_run )
                        run_len++;
                    else
                    {
                        in_run = true;
                        run_idx = i-1;
                        run_len = 2;
                    }
                }

                // Resolve completed runs of duplicate games
                bool more = (i+1<len);
                if( run_len>1 && (!match || !more) )
                {
                    unsigned int max=0;
                    unsigned int the_one=run_idx;
                    bool all_the_same=true;
                    std::string s = sorted[run_idx]->line;

                    // Find the longest game in the run
                    for( unsigned int j=run_idx; j<run_idx+run_len; j++ )
                    {
                        sorted[j]->keep = false;
                        if( j>run_idx && s!=sorted[j]->line )
                            all_the_same = false;
                        if( sorted[j]->line.length() >= max )
                        {
                            max = sorted[j]->line.length();
                            the_one = j;   
                        }
                    }

                    // Keep the selected game only
                    sorted[the_one]->keep = true;

                    // If they weren't all the same, append to diagnostics file to show what we did
                    s = "";
                    for( unsigned int j=run_idx; !all_the_same && j<run_idx+run_len; j++ )
                    {
                        std::string t = sorted[j]->line;
                        if( j == the_one )
                        {
                            util::replace_once(t,"[White \"","[White \"KEEP ");
                            util::putline(*p_smart_uniq,t);
                        }
                        else if( t != s )
                        {
                            s = t;   // Don't show identical discards
                            util::replace_once(t,"[White \"","[White \"DISCARD ");
                            util::putline(*p_smart_uniq,t);
                        }
                    }

                    // Run has been processed
                    in_run = false;
                    run_len = 0;
                }
            }
        }

        // Else drop exact duplicate lines only
        else
        {
            const CANDIDATE *prev = NULL;
            for( CANDIDATE *p : sorted )
            {
                if( prev && p->line == prev->line )
                    p->keep = false;
                prev = p;
            }
        }

        // Note that we don't reorder the games, we just drop dups we found by reordering temporarily
        for( const CANDIDATE& c : postponed_dedup )
        {
            if( c.keep )
                util::putline( out, c.line );
        }
        postponed_dedup.clear();
        cached_day.clear();
    }

    // Store the line
    if( have_line )
    {
        CANDIDATE c;
        c.line = line;
        c.keep = true;
        postponed_dedup.push_back(c);

        // Don't start a collection of games in one 'day' without a cached day to compare later games to
        if( postponed_dedup.size() == 1 )
        {
            if( day.length() > 0 )
                cached_day = day;
            else
            {
                util::putline( out, line );    // dump the game immediately
                postponed_dedup.clear();
            }
        }
    }
}

