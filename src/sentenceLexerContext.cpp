/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Context for parsing a sentence
/// \file sentenceLexerContext.cpp
#include "sentenceLexerContext.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/minimalCover.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <map>

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"

#error DEPRECATED 

SentenceLexerContext::SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		const SentenceLexerConfig& config,
		double similarityDistance_,
		const std::map<std::string,int>& typepriomap_,
		ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_),m_database(database_)
	,m_similarityDistance(similarityDistance_),m_typepriomap(typepriomap_)
	,m_keySearch( vstorage_, database_, errorhnd_, config.spaceSubst, config.linkSubst)
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);
}

SentenceLexerContext::~SentenceLexerContext()
{
	if (m_debugtrace) delete m_debugtrace;
}


