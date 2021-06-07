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

bool disksort( std::string fin, std::string fout, bool reverse )
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
            util::putline(temp_out,output_line);
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

