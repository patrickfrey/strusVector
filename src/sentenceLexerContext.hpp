/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Context for parsing a sentence
/// \file sentenceLexerContext.hpp
#ifndef _STRUS_SENTENCE_LEXER_CONTEXT_IMPL_HPP_INCLUDED
#define _STRUS_SENTENCE_LEXER_CONTEXT_IMPL_HPP_INCLUDED
#include "strus/sentenceLexerContextInterface.hpp"
#include <vector>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DebugTraceContextInterface;

/// \brief Implementation of the context for parsing the lexical tokens of a sentence based on a vector storage
class SentenceLexerContext
	:public SentenceLexerContextInterface
{
public:
	/// \brief Constructor
	SentenceLexerContext( ErrorBufferInterface* errorhnd_);
	
	virtual ~SentenceLexerContext();

	virtual std::vector<SentenceTerm> altLexems() const;

	virtual bool skipToFollow( int length);

	virtual void skipBack();

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
};

}//namespace
#endif

