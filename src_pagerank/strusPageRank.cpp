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

#undef STRUS_LOWLEVEL_DEBUG

enum LexemId
{
	LEXEM_STARTRULE,
	LEXEM_NAME,
	LEXEM_EQUAL,
	LEXEM_REDIRECT,
	LEXEM_ENDRULE
};

#ifdef STRUS_LOWLEVEL_DEBUG
static const char* lexemIdName( LexemId lid)
{
	const char* ar[] = {"STARTRULE","NAME","EQUAL","REDIRECT","ENDRULE"};
	return ar[ lid];
}
#endif

class InputParser
	:public strus::InputStream
{
public:
	explicit InputParser( const char* path)
		:strus::InputStream(path?path:"-")
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
				int ec = error();
				if (ec) throw std::runtime_error( strus::string_format( "error reading input file: %s", ::strerror(ec)));
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
			}
		}
	}

private:
	char const* m_itr;
	std::string m_buf;
};

static void printUsage()
{
	std::cerr << "usage: strusPageRank [options] <inputfile>" << std::endl;
	std::cerr << "    options     :" << std::endl;
	std::cerr << "    -h          : print this usage" << std::endl;
	std::cerr << "    -v          : verbose output, print all declarations to stdout" << std::endl;
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
		strus::PageRank pagerank;
		InputParser input( argv[ argi]);
		LexemId lid;
		std::set<strus::PageRank::PageId> declaredset;
		std::map<strus::PageRank::PageId,strus::PageRank::PageId> redirectmap;
		std::string lname;
		std::string declname;
		std::vector<std::string> linknames;
		std::string redirectname;

		while (input.parseLexem( lid, lname))
		{
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << lexemIdName( lid) << " " << lname << std::endl;
#endif
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
						throw std::runtime_error( "id of link source expected after '*' (start of rule declaration)");
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
						std::cerr << "name of redirect target expected after '->'" << std::endl;
					}
					break;
				case LEXEM_ENDRULE:
				{
					strus::PageRank::PageId dpg = pagerank.getOrCreatePageId( declname);
					if (declname.empty())
					{
						std::cerr << "empty declaration found" << std::endl;
					}
					else if (linknames.empty())
					{
						if (!redirectname.empty())
						{
							// declare redirect
							strus::PageRank::PageId rpg = pagerank.getOrCreatePageId( redirectname);
							redirectmap[ dpg] = rpg;
							if (verbose)
							{
								std::cerr << "redirect " << declname << " -> " << redirectname << std::endl;
							}
						}
					}
					else
					{
						if (redirectname.empty())
						{
							// add declared links:
							std::vector<std::string>::const_iterator
								li = linknames.begin(), le = linknames.end();
							for (; li != le; ++li)
							{
								strus::PageRank::PageId lpg = pagerank.getOrCreatePageId( *li);
								pagerank.addLink( dpg, lpg);
								if (verbose)
								{
									std::cerr << "link " << declname << " = " << *li << std::endl;
								}
							}
							declaredset.insert( dpg);
						}
						else
						{
							std::cerr << "mixing real links with redirect in definition of '" << declname << "'" << std::endl;
						}
					}
					declname.clear();
					linknames.clear();
					redirectname.clear();
					break;
				}
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


