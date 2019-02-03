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

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class SentenceLexerContextInterface;
/// \brief Forward declaration
class DebugTraceContextInterface;

/// \brief Implementation of a sentence lexer based on a vector storage
class SentenceLexerInstance
	:public SentenceLexerInstanceInterface
{
public:
	/// \brief Constructor
	SentenceLexerInstance( const VectorStorageClient* vstorage_, ErrorBufferInterface* errorhnd_);

	virtual ~SentenceLexerInstance();

	virtual SentenceLexerContextInterface* createLexer( const std::string& source) const;

	virtual double getSimilarity( const SentenceTerm& term, const SentenceTerm& other) const;

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
};

}//namespace
#endif

