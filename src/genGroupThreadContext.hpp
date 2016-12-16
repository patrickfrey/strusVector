/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Multithreading context for the genetic algorithms
#ifndef _STRUS_VECTOR_GEN_GROUP_THREAD_CONTEXT_HPP_INCLUDED
#define _STRUS_VECTOR_GEN_GROUP_THREAD_CONTEXT_HPP_INCLUDED
#include "simGroupMap.hpp"
#include "simRelationReader.hpp"
#include "genGroupContext.hpp"
#include "sampleSimGroupAssignmentQueue.hpp"
#include <vector>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

typedef void (*GenGroupProcedure)( 
		const GenGroupParameter* parameter,
		GlobalCountAllocator* glbcnt,
		GenGroupContext* genGroupContext,
		const SimRelationReader* simrelreader,
		std::size_t startidx, std::size_t endidx,
		ErrorBufferInterface* errorhnd);

class GenGroupThreadContext
{
public:
	GenGroupThreadContext(
			GlobalCountAllocator* glbcnt_,
			GenGroupContext* groupctx_,
			const SimRelationReader* simrelreader_,
			const GenGroupParameter* parameter_,
			unsigned int nofThreads_,
			ErrorBufferInterface* errorhnd_);

	void run( GenGroupProcedure proc, std::size_t startidx, std::size_t endidx);
	void runGroupAssignments();

private:
	ErrorBufferInterface* m_errorhnd;
	GlobalCountAllocator* m_glbcnt;
	unsigned int m_nofThreads;
	GenGroupContext* m_groupctx;
	const SimRelationReader* m_simrelreader;
	const GenGroupParameter* m_parameter;
};

}//namespace
#endif

