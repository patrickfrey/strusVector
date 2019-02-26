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
#include "strus/wordVector.hpp"
#include "sentenceLexerInstance.hpp"
#include "vectorStorageClient.hpp"
#include <vector>
#include <utility>
#include <algorithm>

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
	typedef SentenceLexerInstance::LinkDef LinkDef;
	typedef SentenceLexerInstance::SeparatorDef SeparatorDef;

public:
	/// \brief Constructor
	SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_, 
		const std::vector<SeparatorDef>& separators,
		const std::vector<LinkDef>& linkDefs,
		const std::vector<char>& linkChars,
		const std::string& source_,
		ErrorBufferInterface* errorhnd_);
	
	virtual ~SentenceLexerContext();

	virtual bool fetchFirstSplit();

	virtual bool fetchNextSplit();

	virtual int nofTokens() const;

	virtual std::string featureValue( int idx) const;

	virtual std::vector<std::string> featureTypes( int idx) const;

	virtual std::vector<SentenceGuess> rankSentences( const std::vector<SentenceGuess>& sentences, int maxNofResults) const;

public:
	struct AlternativeSplit
	{
		struct Element
		{
			std::string feat;
			strus::Index featno;

			Element()
				:feat(),featno(){}
			Element( const std::string& feat_, const strus::Index& featno_)
				:feat(feat_),featno(featno_){}
			Element( const Element& o)
				:feat(o.feat),featno(o.featno){}
			Element& operator=( const Element& o) {feat=o.feat;featno=o.featno; return *this;}
#if __cplusplus >= 201103L
			Element( Element&& o) :feat(std::move(o.feat)),featno(o.featno){}
			Element& operator=( Element&& o) {feat=std::move(o.feat); featno=o.featno; return *this;}
#endif
		};
		AlternativeSplit()
			:ar(){}
		AlternativeSplit( const AlternativeSplit& o)
			:ar(o.ar){}
		AlternativeSplit& operator=( const AlternativeSplit& o) {ar=o.ar; return *this;}
#if __cplusplus >= 201103L
		AlternativeSplit( AlternativeSplit&& o) :ar(std::move(o.ar)){}
		AlternativeSplit& operator=( AlternativeSplit&& o) {ar=std::move(o.ar); return *this;}
#endif
		std::vector<Element> ar;
	};

public:
	/// \brief Defines prunning of evaluation paths not minimizing the number of features detected
	enum {MaxPositionVisits=10};
	/// \brief Defines a limit for prunning variants evaluated dependend on the minimum number of features of a found solution
	static int maxFeaturePrunning( int minNofFeatures)
	{
		enum {ArSize=16};
		static const int ar[ ArSize+1] = {0/*0*/,3/*1*/,4/*2*/,5/*3*/,7/*4*/,8/*5*/,10/*6*/,11/*7*/,12/*8*/,13/*9*/,14/*10*/,15/*11*/,16/*12*/,17/*13*/,18/*14*/,19/*15*/,21/*16*/};
		return (minNofFeatures <= ArSize) ? ar[ minNofFeatures] : (minNofFeatures + 5 + (minNofFeatures >> 4));
	}

private:
	ErrorBufferInterface* m_errorhnd;
	DebugTraceContextInterface* m_debugtrace;
	const VectorStorageClient* m_vstorage;
	const DatabaseClientInterface* m_database;
	std::vector<AlternativeSplit> m_splits;
	int m_splitidx;	
};

}//namespace
#endif

