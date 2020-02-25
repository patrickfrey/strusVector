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
#include "strus/storage/wordVector.hpp"
#include "sentenceLexerInstance.hpp"
#include "sentenceLexerKeySearch.hpp"
#include "vectorStorageClient.hpp"
#include <vector>
#include <utility>
#include <algorithm>

#error DEPRECATED 
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
	typedef SentenceLexerInstance::CharDef CharDef;

public:
	/// \brief Constructor
	SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_, 
		const SentenceLexerConfig& config,
		ErrorBufferInterface* errorhnd_);
	
	virtual ~SentenceLexerContext();

	virtual std::vector<SentenceGuess> call( const std::string& source, int maxNofResults) const;

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
	const DatabaseClientInterface* m_database;
	SentenceLexerKeySearch m_keySearch;
};

}//namespace
#endif

