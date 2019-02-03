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
#include "internationalization.hpp"
#include "errorUtils.hpp"

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"


SentenceLexerContext::SentenceLexerContext( ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0)
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);
}

SentenceLexerContext::~SentenceLexerContext()
{
	if (m_debugtrace) delete m_debugtrace;
}

std::vector<SentenceTerm> SentenceLexerContext::altLexems() const
{
	try
	{
		std::vector<SentenceTerm> rt;
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' fetching lexems: %s"), MODULENAME, *m_errorhnd, std::vector<SentenceTerm>());
}

bool SentenceLexerContext::skipToFollow( int length)
{
	try
	{
		return false;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' skipping to follow lexem: %s"), MODULENAME, *m_errorhnd, false);
}

void SentenceLexerContext::skipBack()
{
	try
	{
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in '%s' in backtrack (skip back): %s"), MODULENAME, *m_errorhnd);
}


