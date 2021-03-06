/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Program calculating page weight for a list of link definitions read from stdin
#include "pageweight.hpp"
#include "pageweightdefs.hpp"
#include "strus/base/inputStream.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/math.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdio>

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
	std::cerr << "usage: strusPageWeight [options] <inputfile>" << std::endl;
#if STRUS_WITH_PAGERANK
	std::cerr << "description: Calculate the weight of a page derived from the linkage of documents with others," << std::endl;
	std::cerr << "             with help of the pagerank algorithm." << std::endl;
#else
	std::cerr << "description: Calculate the weight of a page derived from the number of links pointing to a document." << std::endl;
#endif
	std::cerr << "    <inputfile> :text file to process, lines with the following syntax:" << std::endl;
	std::cerr << "        DECLARATION   = \"*\"  ITEMID \"=\" [\"->\"] { ITEMID } \";\"" << std::endl; 
	std::cerr << "        ITEMID        : document identifier (unicode alpha characters without space)" << std::endl; 
	std::cerr << "    Each declaration describes the links of a document (left side)" << std::endl; 
	std::cerr << "    to other documents (right side). Redirects are marked with an arrow (->)." << std::endl; 
	std::cerr << "    options     :" << std::endl;
	std::cerr << "    -h          : print this usage" << std::endl;
	std::cerr << "    -V          : verbose output, print all declarations to stdout." << std::endl;
	std::cerr << "    -g          : logarithmic scale page rank calculation." << std::endl;
	std::cerr << "    -n <NORM>   : normalize result to an integer between 0 and <NORM>." << std::endl;
	std::cerr << "    -r <PATH>   : specify file <PATH> to write redirect definitions to." << std::endl;
	std::cerr << "    -t <PATH>   : specify file <PATH> to write the tokens to." << std::endl;
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
		std::string tokensFilename;
		int iterations = strus::PageWeight::NofIterations;
		int normval = 0;

		for (; argi < argc; ++argi)
		{
			if (std::strcmp( argv[ argi], "-h") == 0 || std::strcmp( argv[ argi], "--help") == 0)
			{
				printUsage();
				return 0;
			}
			else if (std::strcmp( argv[ argi], "-V") == 0 || std::strcmp( argv[ argi], "--verbose") == 0)
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
		strus::PageWeight pageweight( iterations);
		InputParser input( argv[ argi]);
		LexemId lid;
		std::string lname;
		std::string declname;
		std::vector<std::string> linknames;
		std::string redirectname;

		// Parse input:
		while (input.parseLexem( lid, lname))
		{
			if (verbose)
			{
				std::cerr << "lexem " << lexemIdName( lid) << " " << lname << std::endl;
			}
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
					bool isdecl = !linknames.empty();
					strus::PageWeight::PageId dpg = pageweight.getOrCreatePageId( declname, isdecl);
					if (declname.empty())
					{
						std::cerr << "empty declaration found" << std::endl;
					}
					else
					{
						if (!redirectname.empty())
						{
							// declare redirect
							strus::PageWeight::PageId rpg = pageweight.getOrCreatePageId( redirectname, false);
							pageweight.defineRedirect( dpg, rpg);
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
							strus::PageWeight::PageId lpg = pageweight.getOrCreatePageId( *li, false);
							pageweight.addLink( dpg, lpg);
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
			pageweight.printRedirectsToFile( redirectFilename);
		}
		std::cerr << "remove garbagge (eliminate links to nowhere and resolve redirects)" << std::endl;
		pageweight = pageweight.reduce();
		std::cerr << "calculate ..." << std::endl;
		std::vector<double> pageweightResults = pageweight.calculate();

		std::cerr << "output results ..." << std::endl;
		std::vector<double>::const_iterator ri = pageweightResults.begin(), re = pageweightResults.end();
		double maxresval = 0.0;
		if (normval > 0)
		{
			for (strus::PageWeight::PageId rid=1; ri != re; ++ri,++rid)
			{
				double resval = *ri;
				if (logscale)
				{
					resval = strus::Math::log10( resval * pageweight.nofPages() + 1);
				}
				if (resval > maxresval)
				{
					maxresval = resval;
				}
			}
			ri = pageweightResults.begin();
		}
		for (strus::PageWeight::PageId rid=1; ri != re; ++ri,++rid)
		{
			double resval = *ri;
			if (logscale)
			{
				resval = strus::Math::log10( resval * pageweight.nofPages() + 1);
			}
			if (normval > 0)
			{
				if (resval < 0.0)
				{
					resval = 0.0;
				}
				resval = (resval / maxresval) * normval;
				std::cout << pageweight.getPageName( rid) << "\t" << (unsigned int)resval << std::endl;
			}
			else
			{
				std::cout << pageweight.getPageName( rid) << "\t" << resval << std::endl;
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


