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

enum LexemId
{
	LEXEM_STARTRULE,
	LEXEM_NAME,
	LEXEM_REDIRECT,
	LEXEM_ENDRULE
};

static const char* lexemIdName( LexemId lid)
{
	const char* ar[] = {"STARTRULE","NAME","REDIRECT","ENDRULE"};
	return ar[ lid];
}

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
		else if (*tk == ';')
		{
			lexemId = LEXEM_ENDRULE;
			++m_itr;
		}
		else if (tk[0] == '-' && tk[0] == '>')
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
		ch |= 32;
		if (ch == '_') return true;
		return (ch >= 'a' && ch <= 'z');
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



int main( int argc, const char** argv)
{
	try
	{
		if (argc <= 1 || std::strcmp( argv[0], "-h") == 0 || std::strcmp( argv[0], "--help") == 0)
		{
			std::cerr << "usage: strusPageRank <inputfile>" << std::endl;
			std::cerr << "    <inputfile> = input file path or '-' for stdin" << std::endl;
			std::cerr << "                  file with lines of the for \"*\" SOURCEID = [->] {<TARGETID>} \";\"" << std::endl;
		}
		InputParser input( argv[0]);
		LexemId lid;
		std::string lname;
		while (input.parseLexem( lid, lname))
		{
			std::cout << lexemIdName( lid) << " " << lname << std::endl;
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


