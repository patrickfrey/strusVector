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
#include "sentenceLexerKeySearch.hpp"
#include "vectorStorageClient.hpp"
#include "sentenceLexerConfig.hpp"
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
	SentenceLexerInstance(
			const VectorStorageClient* vstorage_,
			const DatabaseClientInterface* database_,
			const SentenceLexerConfig& config_,
			ErrorBufferInterface* errorhnd_);

	virtual ~SentenceLexerInstance();

	virtual std::vector<SentenceGuess> call( const std::vector<std::string>& fields, int maxNofResults, double minWeight) const;

	virtual std::vector<SentenceTerm> similarTerms( const std::string& type, const std::vector<SentenceTerm>& termlist, double dist, int maxNofResults, double minWeight) const;

private:
	std::vector<strus::Index> getSelectedTypes( strus::Index featno) const;

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
	const DatabaseClientInterface* m_database;
	SentenceLexerConfig m_config;
	std::map<strus::Index,int> m_typepriomap;
};

}//namespace
#endif

