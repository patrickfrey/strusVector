/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#include "dependencyGraph.hpp"
#include "internationalization.hpp"
#include "sampleSimGroupMap.hpp"
#include "simGroupMap.hpp"
#include "genGroupContext.hpp"

using namespace strus;

typedef std::pair<ConceptIndex,ConceptIndex> Dependency;
typedef std::set<Dependency> DependencyGraph;

DependencyGraph strus::buildGroupDependencyGraph( std::size_t nofSamples, const SampleSimGroupMap& sampleSimGroupMap, const SimGroupMap& groupMap, float isaf)
{
	DependencyGraph rt;
	SampleIndex si=0, se=nofSamples;
	for (; si < se; ++si)
	{
		SampleSimGroupMap::const_node_iterator ni = sampleSimGroupMap.node_begin( si), ne = sampleSimGroupMap.node_end( si);
		SampleSimGroupMap::const_node_iterator na = ni;
		for (; ni != ne; ++ni)
		{
			SimGroupRef group_n = groupMap.get( *ni);
			if (!group_n.get()) throw std::runtime_error( _TXT("internal: inconsistency in group map"));

			SampleSimGroupMap::const_node_iterator oi = na, oe = ne;
			for (; oi != oe; ++oi)
			{
				if (*oi != *ni)
				{
					SimGroupRef group_o = groupMap.get( *oi);
					if (!group_o.get()) throw std::runtime_error( _TXT("internal: inconsistency in group map"));
					if (group_n->contains( *group_o))
					{
						if ((float)group_o->size() >= (float)group_n->size() * isaf)
						{
							rt.insert( Dependency( *oi, *ni));
						}
					}
				}
			}
		}
	}
	return rt;
}

void strus::eliminateCircularReferences( DependencyGraph& graph, const SimGroupMap& groupMap)
{
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	while (hi != he)
	{
		std::set<ConceptIndex> visited;		// set of visited for not processing nodes more than once
		std::vector<ConceptIndex> queue;	// queue with candidates to process
		ConceptIndex gid = hi->first;		// current node visited
		// Iterate through all vertices pointed to by arcs rooted in the current node visited:
		for (; hi != he && gid == hi->first; ++hi)
		{
			// Insert the visited vertex into the queue of nodes to process:
			if (visited.insert( hi->second).second)
			{
				SimGroupRef group_first = groupMap.get( hi->first);
				SimGroupRef group_second = groupMap.get( hi->second);
				if (group_first.get() && group_second.get() && group_first->size() == group_second->size())
				{
					queue.push_back( hi->second);
				}
			}
		}
		// Iterate through the set of reachable vertices and check if they have an arc pointing to
		//   the current node visited. If yes, eliminate this arc to eliminate the circular dependency:
		std::size_t qidx = 0;			// iterator in the candidate queue
		for (; qidx < queue.size(); ++qidx)
		{
			std::set<Dependency>::iterator dep_hi = graph.find( Dependency( queue[qidx], gid));
			if (dep_hi != graph.end())
			{
				// Found a circular reference, eliminate the dependency to the current node:
				if (hi == dep_hi) --hi; // ... ensure that the next time we get on for loop increment, we get to the correct follow element.
				graph.erase( dep_hi);
			}
			// Expand the set of vertices directed to from the follow arcs of the processed edge:
			std::set<Dependency>::iterator
				next_hi = graph.upper_bound( Dependency( queue[qidx], 0));
			for (; next_hi != graph.end() && queue[qidx] == next_hi->first; ++next_hi)
			{
				if (visited.insert( next_hi->second).second)
				{
					SimGroupRef group_first = groupMap.get( next_hi->first);
					SimGroupRef group_second = groupMap.get( next_hi->second);
					if (group_first.get() && group_second.get() && group_first->size() == group_second->size())
					{
						queue.push_back( next_hi->second);
					}
				}
			}
		}
	}
}
