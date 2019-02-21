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

	virtual void addSeparator( int uchr);

	virtual void addSpace( int uchr);
	
	virtual void addLink( int uchr, char substchr);

	virtual SentenceLexerContextInterface* createContext( const std::string& source) const;

public:
	struct LinkDef
	{
		char uchr[ 6];
		char substchr;

		LinkDef( int uchr_, char substchr_);
		LinkDef( const LinkDef& o);
	};

	struct SeparatorDef
	{
		char uchr[ 4];

		SeparatorDef( int uchr_);
		SeparatorDef( const SeparatorDef& o);
	};

private:
	void addLinkDef( int uchr, char substchr);

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
	const DatabaseClientInterface* m_database;
	std::vector<LinkDef> m_linkDefs;
	std::vector<SeparatorDef> m_separators;
	std::vector<char> m_linkChars;
};

}//namespace
#endif

