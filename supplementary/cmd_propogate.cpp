// supplementary.cpp : Do something special to LPGN file
//  cmd_propogate.cpp, implement the propogate fide-ids back into game file command

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
#include "key.h"
#include "cmd.h"

struct Triple
{
    std::string name;
    long id;
    int count = 1;
};

enum NameMatchTier
{
    TIER_NULL=0,
    TIER_DOES_NOT_HAVE_ID,
    TIER_HAS_ID,
    TIER_NATIVE,
    TIER_MANUAL,
    TIER_NATIONAL_A,
    TIER_NATIONAL_B,
    TIER_NATIONAL_C,
    TIER_NATIONAL_D,
    TIER_NATIONAL_E,
    TIER_NATIONAL_F,
    TIER_FIDE_A,
    TIER_FIDE_B,
    TIER_FIDE_C,
    TIER_FIDE_D,
    TIER_FIDE_E,
    TIER_FIDE_F,
    NBR_TIERS
};

static const char *tier_txt[] =
{
    "no fide-id present or inferred",
    "no fide-id explicitly specified manually",
    "player has fide-id in this game",
    "player has fide-id in other games",
    "Manual adjustment",
    "LOCAL complete name match",
    "LOCAL case insensitive match",
    "LOCAL case insensitive match after removing period",
    "LOCAL surname plus forename plus initial match",
    "LOCAL surname plus forename match",
    "LOCAL surname plus initial match",
    "FIDE complete name match",
    "FIDE case insensitive match",
    "FIDE case insensitive match after removing period",
    "FIDE surname plus forename plus initial match",
    "FIDE surname plus forename match",
    "FIDE surname plus initial match"
};

struct PlayerDetails
{
    std::string name;
    long id  = 0;
    int  nbr_games=0;
    int  nbr_games_with_id=0;
    int  nbr_games_with_other_ids=0;
    int  first_year=0;
    int  last_year=0;
    std::set<long> other_ids;
    bool operator < (const PlayerDetails &lhs ) { return nbr_games < lhs.nbr_games; }
    NameMatchTier match_tier = TIER_NULL;
    Triple match;         // for inferred (look up in name lists match)
};

struct NameSplit
{
    std::string name;
    std::string lower;
    std::string lower_no_period;
    std::string surname_forename_initial;
    std::string surname_forename;
    std::string surname_initial;
    std::string surname;
};

void split_name( const std::string &name, NameSplit &out )
{                   
    out.name = name;
    std::string s = util::tolower(name);
    size_t len = s.length();
    out.lower = s;
    out.lower_no_period = (len>0 && s[len-1] == '.')
        ? s.substr(0,len-1)
        : s;
    s = out.lower_no_period;
    len = s.length();
    out.surname_forename_initial.clear();
    out.surname_forename.clear();
    out.surname_initial.clear();
    out.surname.clear();
    size_t offset = s.find_first_of(" ,");
    if( offset != std::string::npos )
    {
        out.surname = s.substr(0,offset);
        if( (s[offset]==' ') && (out.surname=="van" || out.surname=="mc" || out.surname=="du" || out.surname=="de") )
        {
            offset = s.find_first_not_of(' ',offset);
            if( offset != std::string::npos )
            {
                offset = s.find_first_of(" ,",offset);
                if( offset != std::string::npos )
                {
                    out.surname = s.substr(0,offset);
                    if( s[offset]==' ' && out.surname=="van der" )
                    {
                        offset = s.find_first_not_of(' ',offset);
                        if( offset != std::string::npos )
                        {
                            offset = s.find_first_of(" ,",offset);
                            if( offset != std::string::npos )
                                out.surname = s.substr(0,offset);
                        }
                    }
                }
            }
        }
    }
    if( offset != std::string::npos )
    {
        offset = s.find_first_not_of(" ,",offset);
        if( offset != std::string::npos )
        {
            out.surname_initial = out.surname + ", " + s[offset];
            if( offset+1 < len )
                out.surname_forename = out.surname + ", " + s.substr(offset);
            size_t offset2 = s.find_first_of(' ',offset);
            if( offset2 != std::string::npos )
            {
                std::string forename = s.substr(offset,offset2-offset);
                out.surname_forename = out.surname + ", " + forename;
                size_t offset3 = s.find_first_not_of(' ',offset2);
                if( offset3 != std::string::npos )
                    out.surname_forename_initial  = out.surname_forename + " " + s[offset3];
            }
        }
    }
}    

struct NameMaps
{
    std::map<std::string,Triple> map_name;
    std::map<std::string,Triple> map_lower;
    std::map<std::string,Triple> map_lower_no_period;
    std::map<std::string,Triple> map_surname_forename_initial;
    std::map<std::string,Triple> map_surname_forename;
    std::map<std::string,Triple> map_surname_initial;
};

int cmd_propogate( std::ifstream &in_aux_manual, std::ifstream &in_aux_loc, std::ifstream &in_aux_fide, std::ifstream &in, std::ofstream &out )
{

    // Stage 1:
    // 
    // Read the NZ fide ids from fide id lists
    //  Split each name into a few (N) name forms
    //  Create N maps named map_ mapping all name form instances to the original name plus id (plus unhelpful but possible collision count)
    //
    NameMaps nm_manual;
    NameMaps nm_nzcf;
    NameMaps nm_fide;
    for( int fed=0; fed<3; fed++) {
        int nbr_manual_players = 0;
        int nbr_nzcf_players = 0;
        int nbr_fide_players = 0;
        int *nbr_players     = fed==0 ? &nbr_manual_players : (fed==1 ? &nbr_nzcf_players : &nbr_nzcf_players);
        const char *fed_name = fed==0 ? "Manual"            : (fed==1 ? "NZCF" : "FIDE");
        NameMaps &fmaps      = fed==0 ? nm_manual           : (fed==1 ? nm_nzcf : nm_fide );
        std::string line;
        int line_nbr=0;
        for(;;)
        {
            std::ifstream &in_aux = fed==0 ? in_aux_manual : (fed==1 ? in_aux_loc : in_aux_fide);
            if( !std::getline(in_aux, line) )
                break;

            // Strip out UTF8 BOM mark (hex value: EF BB BF)
            if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
                line = line.substr(3);
            line_nbr++;
            util::rtrim(line);
            long id = atol(line.c_str());
            if( id > 0 )
            {
                size_t offset = line.find(' ');
                if( offset != std::string::npos )
                {
                    offset = line.find_first_not_of(' ',offset);
                    if( offset != std::string::npos )
                    {
                        (*nbr_players)++;
                        std::string name = line.substr(offset);
                        util::rtrim(name);
                        Triple triple;
                        triple.name = name;
                        triple.id   = id;
                        NameSplit ns;
                        split_name(name,ns);
                        #if 0
                        static int count=10;
                        if( count>0 )
                        {
                            count--;
                            printf( "input=%s\n"
                                " name=%s\n"
                                " lower=%s\n"
                                " lower_no_period=%s\n"
                                " surname_forename_initial=%s\n"
                                " surname_forename=%s\n"
                                " surname_initial=%s\n"
                                " surname=%s\n",
                                name.c_str(),
                                ns.name.c_str(),
                                ns.lower.c_str(),
                                ns.lower_no_period.c_str(),
                                ns.surname_forename_initial.c_str(),
                                ns.surname_forename.c_str(),
                                ns.surname_initial.c_str(),
                                ns.surname.c_str()
                            );
                        }
                        #endif
                        for( int i=0; i<6; i++ )
                        {
                            std::map<std::string,Triple> *p = &fmaps.map_name;                    
                            std::string q;
                            switch(i)
                            {
                                case 0: p = &fmaps.map_name;
                                        q = ns.name;
                                        break;
                                case 1: p = &fmaps.map_lower;
                                        q = ns.lower;
                                        break;
                                case 2: p = &fmaps.map_lower_no_period;
                                        q = ns.lower_no_period;
                                        break;
                                case 3: p = &fmaps.map_surname_forename_initial;
                                        q = ns.surname_forename_initial;
                                        break;
                                case 4: p = &fmaps.map_surname_forename;
                                        q = ns.surname_forename;
                                        break;
                                case 5: p = &fmaps.map_surname_initial;
                                        q = ns.surname_initial;
                                        break;
                            }
                            if( q.length() > 0 )
                            {
                                auto it = p->find(q);
                                if( it == p->end() )
                                    (*p)[q] = triple;
                                else
                                    it->second.count++;
                            }
                        }
                    }
                }
            }
        }
        printf( "%d players read from %s file\n", *nbr_players, fed_name );
    }

    // Stage 2:
    // 
    // Read the players from the game file
    // Make a single map called players of the player names to all info we have about that player name
    //
    std::map<std::string,PlayerDetails> players;
    int line_nbr=0;
    std::string line;

    // Find names line by line
    line_nbr=0;
    unsigned int nbr_names_replaced = 0;
    int total_names=0, total_names_with_fide_ids = 0;
    for(;;)
    {
        if( !std::getline(in, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        util::rtrim(line);
        auto offset = line.find("@M");
        if( offset == std::string::npos )
            continue;
        std::string header = line.substr(0,offset);
        std::string moves  = line.substr(offset);
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( ok && name!="BYE" )
            {
                total_names++;
                auto it = players.find(name);
                if( it == players.end() )
                {
                    PlayerDetails pd;
                    players[name] = pd;
                    it = players.find(name);
                }    
                it->second.name = name;
                it->second.nbr_games++;
                std::string date;
                bool ok2 = key_find(header,"Date",date);
                if( ok2 )
                {
                    int year = atoi( date.c_str() );
                    if( year > 1000 )
                    {
                        if( it->second.first_year == 0 || year < it->second.first_year )
                            it->second.first_year = year;
                        if( it->second.last_year == 0 || year > it->second.last_year)
                            it->second.last_year = year;
                    }
                }
                std::string fide_id;
                ok2  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
                long id = atol(fide_id.c_str());
                ok2 = ok2 && id!=0;
                if( ok2 )
                {
                    total_names_with_fide_ids++;
                    if( it->second.id == 0 )
                    {
                        it->second.id = id;                   
                        it->second.nbr_games_with_id = 1;
                    }
                    else
                    {
                        if( it->second.id == id )                    
                        {
                            it->second.nbr_games_with_id++;
                        }
                        else
                        {
                            printf( "Multiple FIDE ids for name=%s, fide_id=%s, id=%ld\n", name.c_str(), fide_id.c_str(), id );
                            it->second.nbr_games_with_other_ids++;
                            auto it2 = it->second.other_ids.find(id);
                            if( it2 == it->second.other_ids.end() )
                                it->second.other_ids.insert(id);
                        }    
                    }
                }
            }
        }
    }

    // Sort the PlayerDetails by number of games, use this ordering to process and report on the most
    //  important players first. Unfortunately we now have a map of PlayerDetails and a vector of
    //  PlayerDetails. Oh well
    std::vector<PlayerDetails> v;
    for( std::pair<std::string,PlayerDetails> pr: players )
        v.push_back(pr.second);
    std::sort( v.rbegin(), v.rend() );

    // Reports on the number of players, number with fide-ids
    printf( "%d names in game file, %d with immediate fide_ids %.1f%%\n",
        total_names, total_names_with_fide_ids, (total_names_with_fide_ids*100.0) / ((total_names?total_names:1)*1.0) );
    int total_names_with_fide_ids_ext=0;
    for( const PlayerDetails &pd: v )
    {
        if( pd.nbr_games_with_id!=0 && pd.nbr_games_with_other_ids==0 )
            total_names_with_fide_ids_ext++;
    }
    printf( "%zu distinct players, %d with fide_ids %.1f%%\n",
                players.size(),
                total_names_with_fide_ids_ext,
                (total_names_with_fide_ids_ext*100.0) / ((players.size()?players.size():1)*1.0) );

    // Stage 3:
    // 
    // Try to match the player names from players to the map_ name forms from the NZ and FIDE fide id files
    //
    for( int fed=0; fed<3; fed++)
    {
        NameMaps &fmaps = fed==0 ? nm_manual : (fed==1 ? nm_nzcf : nm_fide );
        bool manual = (fed==0); // shoehorn in the manual file first
        for( PlayerDetails pd2: v )
        {
            auto jt = players.find(pd2.name);
            if( jt == players.end() )
            {
                printf( "WARNING, unexpected miss\n");
                continue;
            }
            PlayerDetails &pd = jt->second;
            if( pd.match_tier != TIER_NULL )
                continue;   // Player dealt with in an earlier file. One and done.
            if( pd.nbr_games_with_id!=0 && pd.nbr_games_with_other_ids==0 )
                pd.match_tier = TIER_NATIVE;    // first time through only
            else
            {
                NameSplit ns;
                split_name( pd.name, ns );
                for( int infer=1; infer<=(manual?1:6); infer++ )    // for manual file - only exact matches
                {
                    std::map<std::string,Triple> *p = &fmaps.map_name;                    
                    std::string q;
                    switch(infer)
                    {
                        case 1: p = &fmaps.map_name;
                                q = ns.name;
                                break;
                        case 2: p = &fmaps.map_lower;
                                q = ns.lower;
                                break;
                        case 3: p = &fmaps.map_lower_no_period;
                                q = ns.lower_no_period;
                                break;
                        case 4: p = &fmaps.map_surname_forename_initial;
                                q = ns.surname_forename_initial;
                                break;
                        case 5: p = &fmaps.map_surname_forename;
                                q = ns.surname_forename;
                                break;
                        case 6: p = &fmaps.map_surname_initial;
                                q = ns.surname_initial;
                                break;
                    }
                    if( q.length() > 0 )
                    {
                        auto it = p->find(q);
                        if( it != p->end() )
                        {
                            if( infer == 6 ) // surname_initial
                            {
                                // Important exception,
                                //  "Smith, G" should (weakly) match "Smith, Garry"  BUT
                                //  "Smith, Gordon" should not match "Smith, Garry" through surname initial
                                NameSplit potential;
                                split_name( it->second.name, potential );
                                if( ns.surname_forename!="" && potential.surname_forename!="")
                                {
                                    infer = 0;  // Kill "Smith, Gordon" and "Smith, Garry"
                                    printf("Debug kill %30s %s\n",ns.name.c_str(),it->second.name.c_str());
                                }
                                else
                                    printf("Debug keep (%d) %30s %s\n",it->second.count,ns.name.c_str(),it->second.name.c_str());
                            }
                            if( infer > 0 )
                            {
                                if( it->second.count == 1 ) // collisions are poison
                                {
                                    pd.match = it->second;
                                    if( manual )
                                    {
                                        // Special feature of manual file - fide-id == 1 is a proxy for specifically
                                        //  saying player does not have a fide-id, eg Cecil Purdy (useful for filtering
                                        //  out someone early - useful if their name might be more likely to yield false
                                        //  positives than Cecil Purdy)
                                        pd.match_tier = pd.match.id == 1 ? TIER_DOES_NOT_HAVE_ID : TIER_MANUAL;
                                    }
                                    else
                                    {
                                        int tier = (fed == 1 ? (int)TIER_NATIONAL_A : (int)TIER_FIDE_A);
                                        tier = tier-1+infer;
                                        pd.match_tier = (NameMatchTier)tier;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // Stage 4:
    // 
    // Write out a report
    //
    for( PlayerDetails pd2: v )
    {
        auto jt = players.find(pd2.name);
        if( jt == players.end() )
        {
            printf( "WARNING, unexpected miss\n");
            continue;
        }
        PlayerDetails &pd = jt->second;
        std::string line = util::sprintf("%-30s", pd.name.c_str() );
        if( pd.match_tier <= TIER_DOES_NOT_HAVE_ID )
            line += util::sprintf("%9s"," " );
        else
        {
            long id = (pd.match_tier < TIER_MANUAL ? pd.id : pd.match.id);
            line += util::sprintf( "%9ld", id );
        }
        line += util::sprintf("  %d game%s",
            pd.nbr_games,
            pd.nbr_games==1 ? "" : "s"
        );
        line += pd.first_year == pd.last_year
                ? util::sprintf( ", (%d)", pd.first_year )
                : util::sprintf( ", (%d-%d)",
                    pd.first_year,
                    pd.last_year );
        if( pd.nbr_games_with_id != 0 )
        {
            line += util::sprintf( " %d game%s with id %ld",
                pd.nbr_games_with_id,
                pd.nbr_games_with_id==1 ? "" : "s",
                pd.id
            );
            if( pd.nbr_games_with_other_ids != 0 )
            {
                line += util::sprintf( " %d game%s with %zu other ids",
                    pd.nbr_games_with_other_ids,
                    pd.nbr_games_with_other_ids==1 ? "" : "s",
                    pd.other_ids.size()
                );
                bool first = true;
                for( long id: pd.other_ids )
                {
                    line += util::sprintf( "%s%ld", first?"(":",", id );
                    first = false;
                }
                line += util::sprintf( ")" );
            }
        }
        else if( pd.match_tier != TIER_NULL )
        {
            std::string confidence = "(no clashes)";
            if( pd.match.count > 1 )
                confidence = util::sprintf( "(%d clashes)", pd.match.count );
            line += util::sprintf(" %s %s %s",
                        tier_txt[pd.match_tier],
                        pd.match.name.c_str(),
                        confidence.c_str()
                    );
        }
        util::putline(out,line);
    }

    // Calculate number of games with FIDE-ids, before and after
    in.clear();
    in.seekg(0);
    line_nbr=0;
    int nbr_games = 0;

    // Total number of games with each of 0, 1 or 2 fide-ids up to each tier level
    int nbr_games_0[NBR_TIERS];
    int nbr_games_1[NBR_TIERS];
    int nbr_games_2[NBR_TIERS];
    for( int i=0; i<NBR_TIERS; i++ )
    {
        nbr_games_0[i] = 0;
        nbr_games_1[i] = 0;
        nbr_games_2[i] = 0;
    }
    for(;;)
    {
        if( !std::getline(in, line) )
            break;

        // Strip out UTF8 BOM mark (hex value: EF BB BF)
        if( line_nbr==0 && line.length()>=3 && line[0]==-17 && line[1]==-69 && line[2]==-65)
            line = line.substr(3);
        line_nbr++;
        util::rtrim(line);
        auto offset = line.find("@M");
        if( offset == std::string::npos )
            continue;
        std::string header = line.substr(0,offset);
        std::string moves  = line.substr(offset);

        // Number of fide_ids in this game; 0, 1 or 2
        int nbr_fide_ids_in_game[NBR_TIERS];
        for( int i=0; i<NBR_TIERS; i++ )
            nbr_fide_ids_in_game[i] = 0;

        // Eval name and fideid tags
        for( int i=0; i<2; i++ )
        {
            std::string name;
            bool ok = key_find( header, i==0?"White":"Black", name );
            if( ok && name!="BYE" )
            {
                NameMatchTier match_tier = TIER_NULL;
                std::string fide_id;
                bool ok2  = key_find( header, i==0?"WhiteFideId":"BlackFideId", fide_id );
                long id = atol(fide_id.c_str());
                ok2 = ok2 && id!=0;
                if( ok2 )
                    match_tier = TIER_HAS_ID;
                else
                {
                    auto it = players.find(name);
                    if( it == players.end() )
                    {
                        printf( "WARNING: Player name %s, not found second time through\n", name.c_str() );
                        continue;
                    }    
                    match_tier = it->second.match_tier;
                }

                // Increment this tier and all weaker tiers
                if( match_tier > TIER_DOES_NOT_HAVE_ID )
                {
                    for( int i=match_tier; i<NBR_TIERS; i++ )
                        nbr_fide_ids_in_game[i]++;
                }
            }
        }

        // Update stats
        nbr_games++;
        for( int i=0; i<NBR_TIERS; i++ )
        {
            nbr_games_0[i] += (nbr_fide_ids_in_game[i] == 0 ? 1 : 0 );
            nbr_games_1[i] += (nbr_fide_ids_in_game[i] == 1 ? 1 : 0 );
            nbr_games_2[i] += (nbr_fide_ids_in_game[i] == 2 ? 1 : 0 );
        }
    }

    // Number of games with 0, 1 or 2 fide-ids once matches from each tier are included
    for( int i=2; i<NBR_TIERS; i++ )
    {
        printf( "Number of games with 0, 1 or 2 fide-ids if we include name matches up to tier: %s.\n", tier_txt[i] );
        printf( " 0: %d, %.1f percent\n", nbr_games_0[i],
                                         (nbr_games_0[i]*100.0) / ((nbr_games?nbr_games:1)*1.0) );
        printf( " 1: %d, %.1f percent\n", nbr_games_1[i],
                                         (nbr_games_1[i]*100.0) / ((nbr_games?nbr_games:1)*1.0) );
        printf( " 2: %d, %.1f percent\n", nbr_games_2[i],
                                         (nbr_games_2[i]*100.0) / ((nbr_games?nbr_games:1)*1.0) );
        printf( "Penetration = %.1f%%\n", (nbr_games_2[i]*2 + nbr_games_1[i])*100.0 / ((nbr_games?nbr_games:1)*2.0) );                                        
    }
    return 0;
}
