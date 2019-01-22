/*

    Simple Disk Sort
    
*/

#ifndef DISKSORT_H_INCLUDED
#define DISKSORT_H_INCLUDED

#include <string>
#include <vector>

bool disksort( std::string fin, std::string fout, std::ofstream *p_smart_uniq=0, bool reverse=false, bool uniq=true );

#endif // DISKSORT_H_INCLUDED

