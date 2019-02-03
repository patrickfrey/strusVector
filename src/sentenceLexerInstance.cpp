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
#include <limits>

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"

SentenceLexerInstance::SentenceLexerInstance( const VectorStorageClient* vstorage_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_)
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);
}


SentenceLexerInstance::~SentenceLexerInstance()
{
	if (m_debugtrace) delete m_debugtrace;
}

SentenceLexerContextInterface* SentenceLexerInstance::createLexer( const std::string& source) const
{
	try
	{
		return NULL;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' fetching lexems: %s"), MODULENAME, *m_errorhnd, NULL);
}

double SentenceLexerInstance::getSimilarity( const SentenceTerm& term, const SentenceTerm& other) const
{
	try
	{
		return 0.0;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' fetching lexems: %s"), MODULENAME, *m_errorhnd, std::numeric_limits<double>::quiet_NaN());
}


