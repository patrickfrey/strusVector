/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a sentence lexer based on a vector storage
/// \file sentenceLexerInstance.hpp
#ifndef _STRUS_SENTENCE_LEXER_INSTANCE_IMPL_HPP_INCLUDED
#define _STRUS_SENTENCE_LEXER_INSTANCE_IMPL_HPP_INCLUDED
#include "strus/sentenceLexerInstanceInterface.hpp"
#include "vectorStorageClient.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class SentenceLexerContextInterface;
/// \brief Forward declaration
class DebugTraceContextInterface;
/// \brief Forward declaration
class DatabaseClientInterface;

/// \brief Implementation of a sentence lexer based on a vector storage
class SentenceLexerInstance
	:public SentenceLexerInstanceInterface
{
public:
	/// \brief Constructor
	SentenceLexerInstance( const VectorStorageClient* vstorage_, const DatabaseClientInterface* database_, ErrorBufferInterface* errorhnd_);

	virtual ~SentenceLexerInstance();

	virtual void addLink( int uchr, char sbchr, int priority);

	virtual void addSeparator( int uchr);

	virtual SentenceLexerContextInterface* createContext( const std::string& source) const;

	virtual double getSimilarity( const SentenceTerm& term, const SentenceTerm& other) const;

public:
	struct LinkChar
	{
		char chr;
		char priority;

		LinkChar()
			:chr(0),priority(0){}
		LinkChar( char chr_, char priority_)
			:chr(chr_),priority(priority_){}
		LinkChar( const LinkChar& o)
			:chr(o.chr),priority(o.priority){}

		bool operator < (const LinkChar& o) const
		{
			return chr == o.chr ? (priority > o.priority) : (chr < o.chr);
		}
	};

	struct LinkDef
	{
		char uchr[ 6];
		char sbchr;
		char priority;

		LinkDef( int uchr_, char sbchr_, int priority_);
		LinkDef( const LinkDef& o);
	};

	struct SeparatorDef
	{
		char uchr[ 4];

		SeparatorDef( int uchr_);
		SeparatorDef( const SeparatorDef& o);
	};

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
	const DatabaseClientInterface* m_database;
	std::vector<LinkDef> m_linkDefs;
	std::vector<SeparatorDef> m_separators;
	std::vector<LinkChar> m_linkChars;
};

}//namespace
#endif

