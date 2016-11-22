/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#ifndef _STRUS_VECTOR_SPACE_MODEL_DEPENDENCY_GRAPH_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_DEPENDENCY_GRAPH_HPP_INCLUDED
#include <utility>
#include <set>
#include "simGroup.hpp"

namespace strus {

class SampleSimGroupMap;
class SimGroupMap;

typedef std::pair<ConceptIndex,ConceptIndex> Dependency;
typedef std::set<Dependency> DependencyGraph;

/// \brief Build a directed graph of dependencies of groups derived from the map of samples to groups.
/// \note A group is dependent on another if every of its member is also member of the other group
DependencyGraph buildGroupDependencyGraph( std::size_t nofSamples, std::size_t nofGroups, const SampleSimGroupMap& sampleSimGroupMap);

DependencyGraph reduceGroupDependencyGraphToIsa( DependencyGraph& graph, const SimGroupMap& groupMap, float isaf);

/// \brief Eliminate circular dependencies from a directed graph represented as set
void eliminateCircularReferences( DependencyGraph& graph);

}//namespace
#endif


