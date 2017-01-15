/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Program calculating pagerank for a list of link definitions read from stdin
#include "pagerank.hpp"
#include "strus/base/inputStream.hpp"
#include "strus/base/string_format.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cmath>

#undef STRUS_LOWLEVEL_DEBUG

enum LexemId
{
	LEXEM_STARTRULE,
	LEXEM_NAME,
	LEXEM_EQUAL,
	LEXEM_REDIRECT,
	LEXEM_ENDRULE
};

static const char* lexemIdName( LexemId lid)
{
	const char* ar[] = {"STARTRULE","NAME","EQUAL","REDIRECT","ENDRULE"};
	return ar[ lid];
}

class InputParser
	:public strus::InputStream
{
public:
	explicit InputParser( const char* path)
		:strus::InputStream(path?path:"-"),m_nofLines(0),m_eof(false)
	{
		m_itr = m_buf.c_str();
	}

	bool parseLexem( LexemId& lexemId, std::string& lexemStr)
	{
		lexemStr.clear();
		char const* tk = nextToken();
		if (!tk)
		{
			return false;
		}
		else if (*tk == '*')
		{
			lexemId = LEXEM_STARTRULE;
			++m_itr;
		}
		else if (*tk == '=')
		{
			lexemId = LEXEM_EQUAL;
			++m_itr;
		}
		else if (*tk == ';')
		{
			lexemId = LEXEM_ENDRULE;
			++m_itr;
		}
		else if (tk[0] == '-' && tk[1] == '>')
		{
			lexemId = LEXEM_REDIRECT;
			m_itr += 2;
		}
		else if (isAlphaNum(*m_itr))
		{
			for (; isAlphaNum(*m_itr); ++m_itr)
			{
				lexemStr.push_back( *m_itr);
			}
			lexemId = LEXEM_NAME;
		}
		else
		{
			throw std::runtime_error( strus::string_format( "illegal token at '%s'", m_itr));
		}
		return true;
	}

private:
	static bool isAlphaNum( char ch)
	{
		if ((unsigned char)ch >= 128) return true;
		if (ch >= '0' && ch <= '9') return true;
		if (ch == '_') return true;
		return ((ch|32) >= 'a' && (ch|32) <= 'z');
	}
	static bool isSpace( char ch)
	{
		return (ch && (unsigned char)ch <= 32);
	}
	void skipSpaces()
	{
		for (; *m_itr && isSpace(*m_itr); ++m_itr){}
	}

	const char* nextToken()
	{
		for (skipSpaces(); !*m_itr; skipSpaces())
		{
			if (!fetchNextLine())
			{
				return 0;
			}
			++m_nofLines;
			if (m_nofLines % 100000 == 0)
			{
				fprintf( stderr, "\rprocessed %u lines              ", (unsigned int)m_nofLines);
			}
			m_itr = m_buf.c_str();
		}
		return m_itr;
	}

	bool fetchNextLine()
	{
		enum {BufSize=2048};
		char buf[ BufSize];
		m_buf.clear();

		for (;;)
		{
			std::size_t nn = readAhead( buf, sizeof(buf));
			if (!nn)
			{
				if (m_eof) return false;
				int ec = error();
				if (ec) throw std::runtime_error( strus::string_format( "error reading input file: %s", ::strerror(ec)));
				fprintf( stderr, "\rprocessed %u lines              \n", (unsigned int)m_nofLines);
				m_eof = true;
				return !m_buf.empty();
			}
			char const* eoln = (const char*)std::memchr( buf, '\n', nn);
			if (eoln)
			{
				m_buf.append( buf, eoln - buf);
				nn = read( buf, eoln - buf + 1);
				return true;
			}
			else
			{
				m_buf.append( buf, nn);
				read( buf, nn);
			}
		}
	}

private:
	char const* m_itr;
	std::string m_buf;
	std::size_t m_nofLines;
	bool m_eof;
};

static void printUsage()
{
	std::cerr << "usage: strusPageRank [options] <inputfile>" << std::endl;
	std::cerr << "    options     :" << std::endl;
	std::cerr << "    -h          : print this usage" << std::endl;
	std::cerr << "    -v          : verbose output, print all declarations to stdout." << std::endl;
	std::cerr << "    -g          : logarithmic scale page rank calculation." << std::endl;
	std::cerr << "    -n <NORM>   : normalize result to an integer between 0 and <NORM>." << std::endl;
	std::cerr << "    -r <PATH>   : specify file <PATH> to write redirect definitions to." << std::endl;
	std::cerr << "    -i <ITER>   : specify number of iterations to run as <ITER>." << std::endl;
	std::cerr << "    <inputfile> = input file path or '-' for stdin" << std::endl;
	std::cerr << "                  file with lines of the for \"*\" SOURCEID = [->] {<TARGETID>} \";\"" << std::endl;
}

int main( int argc, const char** argv)
{
	try
	{
		if (argc <= 1)
		{
			std::cerr << "too few arguments" << std::endl;
			printUsage();
			return 0;
		}
		int argi = 1;
		bool verbose = false;
		bool logscale = false;
		std::string redirectFilename;
		int iterations = strus::PageRank::NofIterations;
		int normval = 0;

		for (; argi < argc; ++argi)
		{
			if (std::strcmp( argv[ argi], "-h") == 0 || std::strcmp( argv[ argi], "--help") == 0)
			{
				printUsage();
				return 0;
			}
			else if (std::strcmp( argv[ argi], "-v") == 0 || std::strcmp( argv[ argi], "--verbose") == 0)
			{
				verbose = true;
			}
			else if (std::strcmp( argv[ argi], "-g") == 0 || std::strcmp( argv[ argi], "--logscale") == 0)
			{
				logscale = true;
			}
			else if (std::strcmp( argv[ argi], "-r") == 0 || std::strcmp( argv[ argi], "--redirect") == 0)
			{
				++argi;
				if (argi == argc) throw std::runtime_error("option -r (redirect file) expects argument");
				redirectFilename = argv[argi];
			}
			else if (std::strcmp( argv[ argi], "-i") == 0 || std::strcmp( argv[ argi], "--iterations") == 0)
			{
				++argi;
				if (argi == argc) throw std::runtime_error("option -i (iterations) expects argument");
				iterations = atoi( argv[argi]);
				if (iterations <= 0) throw std::runtime_error("option -i (iterations) needs positive integer number as argument");
			}
			else if (std::strcmp( argv[ argi], "-n") == 0 || std::strcmp( argv[ argi], "--norm") == 0)
			{
				++argi;
				if (argi == argc) throw std::runtime_error("option -n (norm) expects argument");
				normval = atoi( argv[argi]);
				if (normval <= 0) throw std::runtime_error("option -n (norm) needs positive integer number as argument");
			}
			else if (std::strcmp( argv[ argi], "-") == 0)
			{
				break;
			}
			else if (std::strcmp( argv[ argi], "--") == 0)
			{
				++argi;
				break;
			}
			else if (argv[ argi][0] == '-')
			{
				std::cerr << "unknown option: " << argv[argi] << std::endl;
				printUsage();
				return -1;
			}
			else
			{
				break;
			}
		}
		if (argi == argc)
		{
			std::cerr << "too few arguments" << std::endl;
			printUsage();
			return -1;
		}
		else if (argi+1 > argc)
		{
			std::cerr << "too many arguments" << std::endl;
			printUsage();
			return -1;
		}
		strus::PageRank pagerank( iterations);
		InputParser input( argv[ argi]);
		LexemId lid;
		std::string lname;
		std::string declname;
		std::vector<std::string> linknames;
		std::string redirectname;

		// Parse input:
		while (input.parseLexem( lid, lname))
		{
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << lexemIdName( lid) << " " << lname << std::endl;
#endif
		AGAIN:
			switch (lid)
			{
				case LEXEM_STARTRULE:
					if (!declname.empty() || !linknames.empty() || !redirectname.empty())
					{
						std::cerr << "rule definition not terminated before definition of '" << lname << "'" << std::endl;
					}
					declname.clear();
					linknames.clear();
					redirectname.clear();
					if (!input.parseLexem( lid, declname) || lid != LEXEM_NAME)
					{
						if (lid == LEXEM_EQUAL)
						{
							// ... skip rule with empty name
							while (input.parseLexem( lid, lname) && lid != LEXEM_ENDRULE){}
						}
						else
						{
							throw std::runtime_error( "id of link source expected after '*' (start of rule declaration)");
						}
					}
					break;
				case LEXEM_NAME:
					linknames.push_back( lname);
					break;
				case LEXEM_EQUAL:
					break;
				case LEXEM_REDIRECT:
					if (!input.parseLexem( lid, redirectname) || lid != LEXEM_NAME)
					{
						if (lid == LEXEM_REDIRECT) goto AGAIN;
						if (lid == LEXEM_ENDRULE)
						{
							declname.clear();
							linknames.clear();
							redirectname.clear();
							continue;
						}
						else
						{
							std::cerr << "name of redirect target expected after '->' instead of " << lexemIdName(lid) << std::endl;
						}
					}
					break;
				case LEXEM_ENDRULE:
				{
					strus::PageRank::PageId dpg = pagerank.getOrCreatePageId( declname, true);
					if (declname.empty())
					{
						std::cerr << "empty declaration found" << std::endl;
					}
					else
					{
						if (!redirectname.empty())
						{
							// declare redirect
							strus::PageRank::PageId rpg = pagerank.getOrCreatePageId( redirectname, false);
							pagerank.defineRedirect( dpg, rpg);
							if (verbose)
							{
								std::cerr << "redirect " << declname << " -> " << redirectname << std::endl;
							}
						}
						// add declared links:
						std::vector<std::string>::const_iterator
							li = linknames.begin(), le = linknames.end();
						for (; li != le; ++li)
						{
							strus::PageRank::PageId lpg = pagerank.getOrCreatePageId( *li, false);
							pagerank.addLink( dpg, lpg);
							if (verbose)
							{
								std::cerr << "link " << declname << " = " << *li << std::endl;
							}
						}
					}
					declname.clear();
					linknames.clear();
					redirectname.clear();
					break;
				}
			}
		}
		if (!redirectFilename.empty())
		{
			std::cerr << "write redirects to file " << redirectFilename << std::endl;
			pagerank.printRedirectsToFile( redirectFilename);
		}
		std::cerr << "remove garbagge (eliminate links to nowhere and resolve redirects)" << std::endl;
		pagerank = pagerank.reduce();
		std::cerr << "calculate ..." << std::endl;
		std::vector<double> pagerankResults = pagerank.calculate();

		std::cerr << "output results ..." << std::endl;
		std::vector<double>::const_iterator ri = pagerankResults.begin(), re = pagerankResults.end();
		double maxresval = 0.0;
		if (normval > 0)
		{
			for (strus::PageRank::PageId rid=1; ri != re; ++ri,++rid)
			{
				double resval = *ri;
				if (logscale)
				{
					resval = std::log10( resval * pagerank.nofPages() + 1);
				}
				if (resval > maxresval)
				{
					maxresval = resval;
				}
			}
		}
		for (strus::PageRank::PageId rid=1; ri != re; ++ri,++rid)
		{
			double resval = *ri;
			if (logscale)
			{
				resval = std::log10( resval * pagerank.nofPages() + 1);
			}
			if (normval > 0)
			{
				if (resval < 0.0)
				{
					resval = 0.0;
				}
				resval = (resval / maxresval + 0.5) * normval;
				std::cout << pagerank.getPageName( rid) << "\t" << (unsigned int)resval << std::endl;
			}
			else
			{
				std::cout << pagerank.getPageName( rid) << "\t" << resval << std::endl;
			}
		}
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		return -1;
	}
	catch (const std::logic_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -1;
	}
}


