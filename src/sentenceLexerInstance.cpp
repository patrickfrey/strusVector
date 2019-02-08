/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a sentence lexer based on a vector storage
/// \file sentenceLexerInstance.cpp
#include "sentenceLexerInstance.hpp"
#include "sentenceLexerContext.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "strus/base/utf8.hpp"
#include <limits>
#include <cstring>

using namespace strus;

#define MODULENAME "sentence lexer instance (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"

SentenceLexerInstance::SentenceLexerInstance( const VectorStorageClient* vstorage_, const DatabaseClientInterface* database_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_),m_database(database_)
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);
}


SentenceLexerInstance::~SentenceLexerInstance()
{
	if (m_debugtrace) delete m_debugtrace;
}

SentenceLexerInstance::LinkDef::LinkDef( int uchr_, char sbchr_, int priority_)
	:sbchr(sbchr_),priority(priority_)
{
	std::size_t uchrlen = strus::utf8encode( uchr, uchr_);
	if (uchrlen == 0 || uchrlen >= sizeof(uchr)) throw std::runtime_error(_TXT("punctuation character is not unicode or out of the range accepted by this method"));
	if (priority_ < std::numeric_limits<char>::min() || priority_ > std::numeric_limits<char>::max()) throw std::runtime_error(_TXT("punctuation priority is out of the range accepted by this method"));
	uchr[ uchrlen] = 0;
}

SentenceLexerInstance::LinkDef::LinkDef( const LinkDef& o)
{
	std::memcpy( this, &o, sizeof(*this));
}

SentenceLexerInstance::SeparatorDef::SeparatorDef( int uchr_)
{
	std::size_t uchrlen = strus::utf8encode( uchr, uchr_);
	if (uchrlen == 0 || uchrlen >= sizeof(uchr)) throw std::runtime_error(_TXT("punctuation character is not unicode or out of the range accepted by this method"));
	uchr[uchrlen] = 0;
}

SentenceLexerInstance::SeparatorDef::SeparatorDef( const SeparatorDef& o)
{
	std::memcpy( this, &o, sizeof(*this));
}


void SentenceLexerInstance::addLink( int uchr, char sbchr, int priority)
{
	try
	{
		if (priority > std::numeric_limits<char>::max() || priority < std::numeric_limits<char>::min())
		{
			throw std::runtime_error(_TXT("priority out of range"));
		}
		m_linkDefs.push_back( LinkDef( uchr, sbchr, priority));
		if (sbchr)
		{
			std::vector<LinkChar>::iterator ci = m_linkChars.begin(), ce = m_linkChars.begin();
			for (; ci != ce && (unsigned char)ci->chr < (unsigned char)sbchr; ++ci){}
			if (ci->chr == sbchr)
			{
				if (ci->priority > priority)
				{
					ci->priority = priority;
				}
			}
			else
			{
				m_linkChars.insert( ci, LinkChar( sbchr, (char)priority));
			}
		}
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in '%s' adding a link character: %s"), MODULENAME, *m_errorhnd);
}

void SentenceLexerInstance::addSeparator( int uchr)
{
	try
	{
		m_separators.push_back( SeparatorDef( uchr));
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in '%s' adding a separator character: %s"), MODULENAME, *m_errorhnd);
}

SentenceLexerContextInterface* SentenceLexerInstance::createContext( const std::string& source) const
{
	try
	{
		return new SentenceLexerContext( m_vstorage, m_database, m_separators, m_linkDefs, m_linkChars, source, m_errorhnd);

	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' creating context for analysis: %s"), MODULENAME, *m_errorhnd, NULL);
}

double SentenceLexerInstance::getSimilarity( const SentenceTerm& term, const SentenceTerm& other) const
{
	try
	{
		WordVector term_vec = m_vstorage->featureVector( term.type(), term.value());
		WordVector other_vec = m_vstorage->featureVector( other.type(), other.value());
		return m_vstorage->vectorSimilarity( term_vec, other_vec);
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' calculating the similarity of terms: %s"), MODULENAME, *m_errorhnd, std::numeric_limits<double>::quiet_NaN());
}


