Introduction
============

PGN2LINE is a chess PGN file management system based on the idea of converting PGN file(s)
to a new and highly convenient one game per line text format. The system makes
it easy to collect massive collections of disparate pgn files into a large,
neatly ordered and consolidated pgn file (or files). Along the way exact
duplicate games will be discarded and problematic missing blank lines (between
headers and moves, and between games) will be fixed. Whitelist, blacklist and
fixuplist facilities make it easy to filter tournaments and harmonise
tournament naming.

Workflow
========

A typical application involves making sense of a massive collection of PGN files.
Perhaps you have downloaded countless PGN file over the years. Start by going to
your downloads folder;

<pre>
cd \Users\Bill\Downloads
</pre>

This assumes Windows, but PGN2LINE is written in fairly idiomatic modern C++ and
should work just as well on other desktop platforms.

Now create a text file listing all the PGN files under this directory;

<pre>
dir/b/s >pgnlist.txt
</pre>

Now apply PGN2LINE, this is where the magic happens;

<pre>
pgn2line -l pgnlist.txt bigfile.lpgn
</pre>

The -l command line parameter simply indicates pgnlist.txt is a list of PGN
files, without that flag pgn2line expects as input a single PGN file.

The file bigfile.lpgn now contains all the games from all the files in pgnlist.txt
in a special one game per line format. The extension .lpgn is just a suggestion.
Exact duplicates (and empty games) are removed. The games are all sorted neatly.
To convert into something your other chess software can use simply;

<pre>
line2pgn bigfile.lpgn bigfile.pgn
</pre>

For extra credit, program *tournaments* in this suite operates on .lpgn files
and produces a nice report of the tournaments present. It's possible to edit
this report easily to create whitelist files, blacklist files and fixuplist
files. These files are useful for repeating the whole process with filters
in place to select or reject specific tournaments, and also to improve tournament
event and site information. Dig further into the details below if you are
interested in this.

Finally for now, the program suite also has (from V1.1) a program wordsearch
which is basically a poor man's grep -w (not every Windows user has access to
a grep program). So

<pre>
wordsearch Carlsen bigfile.lpgn carlsen.lpgn
line2pgn carlsen.lpgn carlsen.pgn
wordsearch Carlsen-Nakamura bigfile.lpgn carlsen-nakamura.lpgn
line2pgn carlsen-nakamura.lpgn carlsen-nakamura.pgn
</pre>

This illustrates the beauty of .lpgn files, they are normal text files with
a chess game on every line. Any tool that filters, shuffles, sorts etc. text
files can operate on .lpgn files and the output will still be a text file
with a chess game on every line, that converts easily to PGN with program
line2pgn.

Details
=======

Why is a one game per line format convenient? An example should make the concept
clear. Consider using grep (i.e. search) on a one game per line file searching
for the text (say) "Forster". The grep output will be a valid one game per line
file comprising all games featuring that text - which includes all games played
by anyone named Forster.

Another example, in a Nakamura-Carlsen game, the format used will generate the
text "Nakamura-Carlsen" as part of the line prefix. Search for all lines with
that text and you get all such games ready for immediate conversion back into PGN.

By way of contrast, text searching in PGN games can only ever find fragments of
games, with no immediate way of accessing the containing game.

Usage:

<pre>
 pgn2line [-l] [-z] [-d] [-y year_before] [+y year_after]
          [-w whitelist | -b blacklist]
          [-f fixuplist] [-r] input output.lpgn

-l indicates input is a text file that lists input pgn files
   (otherwise input is a single pgn file)
-z indicates don't include zero length games (BYEs are unaffected)
-d indicates smart game de-duplication (eliminates more dups)
-y discard games unless they are played in year_before or earlier
+y discard games unless they are played in year_after or later
-w specifies a whitelist list of tournaments, discard games not from these tournaments
-b specifies a blacklist list of tournaments, discard games from these tournaments
-f specifies a list of tournament name fixups
</pre>

Output is all games found in one game per line format, sorted for maxium utility.

Whitelist, blacklist and fixuplist are all text files listing tournaments in the 
following format;

yyyy Event@Site

For example;

-	2018 German Open@Berlin

Fixuplist must have an even number of lines as it is interpreted as before/after pairs.
For example

-	2018 German Open@Berlin
-	2018 23rd German Open@Berlin, Germany

This will apply the simple transformation Event->"23rd German Open" and Site->"Berlin, Germany"
For the moment Events and/or Sites with embedded @ characters are not accommodated.

Pairs of before and after player names are now allowed in the fixup file,
player names are identified as those strings NOT in yyyy Event@Site format.

The program suite also includes *line2pgn*, a program to convert back to pgn, and *tournaments*,
a program to convert the line format into a tournament list. This list can be
in the same format as the whitelist/blacklist/fixuplist greatly simplifying preparation of
such lists.

Games are represented on a single line as follows;

	Prefix@Hheader1@Hheader2...@Mmoves1@Mmoves2...

The header1, header2 etc. are the PGN header lines, moves1, moves2 etc. are the PGN
moves lines

The prefix to the line serves two purposes
1) readable summary of the game
2) allows simple string sort to order games effectively

Example for Smith v Jones, Event=Acme Open, Site=Gotham, round 3.2 on 2001-12-31 prefix is;
<pre>
2001-12-28 Acme Open, Gotham # 2001-12-31 03.002 Smith-Jones
</pre>

The first date is the tournament date (first day in tournament), the second date is the
game date.

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

Formerly TODO now DONE (see -d flag) - At the moment only exact duplicates are eliminated with an internal "uniq" step.
To eliminate more dups, consider eliminating games with identical prefixes but different
content. Usually the different content will be due to annotations, so keep longest content.

Formerly TODO now DONE - A simple and useful enhancement would be to allow player name pairs in the
fixuplist.  Any line that didn't match the yyyy event@site syntax would be considered
a player name. Then the before and after player name pairs would be checked and
possibly applied for every "White" and "Black" name in the file

TODO - Events and/or Sites with embedded @ characters are not accommodated by the
whitelist, blacklist and fixuplist files, extend the tournament list syntax used by
those files with an appropriate extension to allow that. One idea to allow this is
<pre>
"@#2008 John@Smith.com Open#Berlin"
</pre>
The concept here is that if the first charracter is a '@' then the 2nd character
replaces @ as the Event/Site separator.

