/*

    Simple Disk Sort
                                                        Bill Forster, October 2018

    Why do we write our own disksort? Previously we used the system sort, but Windows
    sort.exe cannot deal with lines longer than 64K characters, and in our whole game
    on a line system, lines can be longer than that for extensively annotated games.

    We use default std::sort() as an underlying primitive, that means this is a case
    sensitive unlike system sort on (most?) systems, but that's neither here nor there
    for this application at least.
    
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include "util.h"
#include "disksort.h"

static bool equal_exact_match( const std::string &s1, const std::string &s2 );
static bool equal_smart_match( const std::string &s1, const std::string &s2 );
static bool equal_prefix_only( const std::string &s1, const std::string &s2 );
static bool equal_moves(const std::string &s1, const std::string &s2);
static void get_main_line( const std::string &s, std::string &main_line );
static size_t find_sort_tie_breaker_in_prefix( const std::string &prefix );

// We do a bit of LPGN format specific stuff (LPGN = output of pgn2line)
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

// Compare two strings for equality, if they are in LPGN format, sort tie-breaker fields
//  do not have to match for equality
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

bool disksort( std::string fin, std::string fout,  std::ofstream *p_smart_uniq, bool reverse, bool uniq )
{
    std::ifstream in(fin.c_str());
    if( !in )
    {
        printf( "Error, cannot open file %s for reading\n", fin.c_str() );
        return false;
    }

    // Name two temporary files
    unsigned int x1 = rand();
    unsigned int x2 = rand();
    while( x2 == x1 )     // occasionally this could happen, make sure names are different
        x2 = rand();
    std::string fname_temp1    = util::sprintf( "%s-disksort-tempfile-%05d.tmp", fout.c_str(), x1 );
    std::string fname_temp2    = util::sprintf( "%s-disksort-tempfile-%05d.tmp", fout.c_str(), x2 );

    // Names are assigned to an input file and an output file arbitrarily at first
    //  and subsequently the names flip flop back and forth
    bool flip_flop_temp_filenames = false;
    std::string fname_temp_in  = flip_flop_temp_filenames ? fname_temp1 : fname_temp2;
    std::string fname_temp_out = flip_flop_temp_filenames ? fname_temp2 : fname_temp1;

    // Start with empty output file
    std::ofstream empty_out(fname_temp_out);
    if( !empty_out )
    {
        printf( "Error; Cannot open file %s for writing\n", fname_temp_out.c_str() );
        return false;
    }
    empty_out.close();

    // While there's more to read
    bool first=true;                    // suppress uniq check first time through
    std::string previous_line;          // previous line for uniq check
    std::vector<std::string> lookback;  // previous lines for smart lookback
    while( in )
    {

        // Output temp file in previous loop has all sorted output to date, in this loop
        //  use it as an input temp file and build a new longer output temp file comprising
        //  everything from the previous temp file, plus a new chunk from the input
        flip_flop_temp_filenames = !flip_flop_temp_filenames;
        fname_temp_in  = flip_flop_temp_filenames ? fname_temp1 : fname_temp2;
        fname_temp_out = flip_flop_temp_filenames ? fname_temp2 : fname_temp1;

        // Read a chunk of the input file
        std::vector< std::string > chunk;
        chunk.clear();
        const unsigned int chunk_size = 100000000;  // 100M of memory, should be manageable
        unsigned int total_read_to_date=0;
        while( total_read_to_date < chunk_size )
        {
            std::string line;
            if( !std::getline(in,line) )
                break;
            chunk.push_back(line);
            total_read_to_date += line.length();
        }

        // Sort it
		if( reverse )
			std::sort( chunk.rbegin(), chunk.rend() );
		else
			std::sort( chunk.begin(), chunk.end() );

        // Read line by line from the input temporary file, merge sorting with this chunk and
        //  writing line by line to a new temporary file
        std::ifstream temp_in(fname_temp_in);
        std::ofstream temp_out(fname_temp_out);
        unsigned int get = 0;
        bool memory_line_available = get<chunk.size();
        std::string memory_line;
        if( memory_line_available )
            memory_line = chunk[get];
        std::string file_line;
        bool file_line_available = static_cast<bool>(std::getline(temp_in,file_line));
        while( file_line_available || memory_line_available )
        {
            bool use_memory = memory_line_available;
            if( file_line_available && memory_line_available )
			{  // merge sort happens here
				if( reverse )
					use_memory = (memory_line > file_line);
				else
					use_memory = (memory_line < file_line);
			}

            // Next line is from file or from memory - if it's from file get next file line
            //  if it's from memory get next memory line
            std::string output_line;
            if( use_memory )
            {
                output_line = memory_line;
                get++;
                memory_line_available = get<chunk.size();
                if( memory_line_available )
                    memory_line = chunk[get];
            }
            else
            {
                output_line = file_line;
                file_line_available = static_cast<bool>(std::getline(temp_in,file_line));
            }

            // Smart deduplication ?
            if( p_smart_uniq )
            {
                bool run_broken = false;
                if( lookback.size() > 0 )
					run_broken = !equal_smart_match( lookback[lookback.size()-1], output_line );

                // If run not broken, add output to lookback buffer before possible resolution
                if( !run_broken )
                    lookback.push_back(output_line);

                // Resolve lookback buffer if no more input, or if run of matching games broken
                bool more = (file_line_available || memory_line_available || in);
                if( lookback.size()>0 && (!more||run_broken)  )
                {
                    size_t max=0;
                    size_t the_one=0;
                    bool all_the_same=true;
                    std::string s = lookback[0];
                    for( size_t i=0; i<lookback.size(); i++ )
                    {
                        if( i>0 && !equal_exact_match(s,lookback[i]) )
                            all_the_same = false;
                        if( lookback[i].length() >= max )
                        {
                            max = lookback[i].length();
                            the_one = i;   
                        }
                    }

                    // Choose the longest of the matching games
                    util::putline(temp_out,lookback[the_one]);

                    // If they weren't all the same, append to diagnostics file to show what we did
                    s = "";
                    for( size_t i=0; !all_the_same && i<lookback.size(); i++ )
                    {
                        std::string t = lookback[i];
                        if( i == the_one )
                        {
                            util::replace_once(t,"[White \"","[White \"KEEP ");
                            util::putline(*p_smart_uniq,t);
                        }
                        else if( !equal_exact_match(t,s) )
                        {
                            s = t;   // Don't show identical discards
                            util::replace_once(t,"[White \"","[White \"DISCARD ");
                            util::putline(*p_smart_uniq,t);
                        }
                    }

                    // Lookback buffer is resolved
                    lookback.clear();
                }

                // If run was broken, we haven't processed the output line
                if( run_broken )
                {
                    if( more )  // so either buffer it or write it out immediately
                        lookback.push_back(output_line);
                    else
                        util::putline(temp_out,output_line);
                }
            }

            // Else simple uniq feature optionally drops duplicate lines, never drop first line
            else if( first || !uniq || !equal_exact_match(output_line,previous_line) )
            {
                util::putline(temp_out,output_line);
                previous_line = output_line;
            }
            first = false;
        }
    }

    // Discard temporary input file and rename temporary output file as it's now the final sorted output
    remove(fname_temp_in.c_str());
    remove(fout.c_str());
    if( rename( fname_temp_out.c_str(), fout.c_str() ) )
    {
        printf( "Error; Cannot create file %s by renaming temporary file %s\n", fout.c_str(), fname_temp_out.c_str() );
        return false;
    }
    return true;
}

