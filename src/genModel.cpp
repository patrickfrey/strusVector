/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for breeding good representants of similarity classes with help of genetic algorithms
#include "genModel.hpp"
#include "simGroupList.hpp"
#include "simGroupMap.hpp"
#include "random.hpp"
#include "internationalization.hpp"
#include <ctime>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <algorithm>

using namespace strus;
#undef STRUS_LOWLEVEL_DEBUG

static Random g_random;

class GroupIdAllocator
{
public:
	GroupIdAllocator()
		:m_cnt(0){}
	GroupIdAllocator( const GroupIdAllocator& o)
		:m_cnt(o.m_cnt),m_freeList(o.m_freeList){}

	FeatureIndex alloc()
	{
		FeatureIndex rt;
		if (!m_freeList.empty())
		{
			rt = m_freeList.back();
			m_freeList.pop_back();
		}
		else
		{
			rt = ++m_cnt;
		}
		return rt;
	}

	void free( const FeatureIndex& idx)
	{
		m_freeList.push_back( idx);
	}

	unsigned int nofGroupsAllocated() const
	{
		return m_cnt - m_freeList.size();
	}

private:
	FeatureIndex m_cnt;
	std::vector<FeatureIndex> m_freeList;
};


static SimRelationMap getSimRelationMap( const SimHashCollection& samplear, unsigned int simdist)
{
	SimRelationMap rt;

	SimHashCollection::const_iterator si = samplear.begin(), se = samplear.end();
	for (SampleIndex sidx=0; si != se; ++si,++sidx)
	{
		std::vector<SimRelationMap::Element> row;

		SimHashCollection::const_iterator pi = samplear.begin();
		for (SampleIndex pidx=0; pi != si; ++pi,++pidx)
		{
			if (pidx != sidx && si->near( *pi, simdist))
			{
				unsigned short dist = si->dist( *pi);
#ifdef STRUS_LOWLEVEL_DEBUG
				std::cerr << "declare similarity " << sidx << " ~ " << pidx << " by " << dist << std::endl;
#endif
				row.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		rt.addRow( sidx, row);
	}
	return rt;
}

typedef std::list<SimGroup> GroupList;
typedef std::map<FeatureIndex,GroupList::iterator> GroupInstanceMap;

static void removeGroup(
		const FeatureIndex& group_id,
		GroupIdAllocator& groupIdAllocator,
		GroupList& groupList,
		GroupInstanceMap& groupInstanceMap,
		SimGroupMap& simGroupMap)
{
	GroupInstanceMap::iterator group_slot = groupInstanceMap.find( group_id);
	if (group_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (removeGroup): %u"), group_id);
	GroupList::iterator group_iter = group_slot->second;
	if (group_iter->id() != group_id) throw strus::runtime_error(_TXT("illegal reference in group list"));
	groupInstanceMap.erase( group_slot);

	SimGroup::const_iterator
		mi = group_iter->begin(), me = group_iter->end();
	for (; mi != me; ++mi)
	{
		simGroupMap.remove( *mi, group_id);
	}
	groupIdAllocator.free( group_id);
	groupList.erase( group_iter);
}

static unsigned int age_mutations( const SimGroup& group, unsigned int maxage, unsigned int conf_mutations)
{
	return (conf_mutations * (maxage - std::min( maxage, group.age()))) / maxage;
}

static unsigned int age_mutation_votes( const SimGroup& group, unsigned int maxage, unsigned int conf_votes)
{
	return conf_votes * (std::min( group.age(), maxage) / maxage) + 1;
}

static bool tryAddGroupMember( const FeatureIndex& group_id, SampleIndex newmember,
				GroupInstanceMap& groupInstanceMap,
				SimGroupMap& simGroupMap, const SimHashCollection& samplear,
				unsigned int descendants, unsigned int mutations, unsigned int votes,
				unsigned int maxage,
				SimHashAllocator& simHashAllocator)
{
	GroupInstanceMap::iterator group_slot = groupInstanceMap.find( group_id);
	if (group_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (tryAddGroupMember): %u"), group_id);
	GroupList::iterator group_inst = group_slot->second;

	SimGroup newgroup( *group_inst);
	newgroup.addMember( newmember);
	newgroup.mutate( samplear, descendants, age_mutations( newgroup, maxage, mutations), age_mutation_votes( newgroup, maxage, votes), simHashAllocator);
	if (newgroup.fitness( samplear) >= group_inst->fitness( samplear))
	{
		*group_inst = newgroup;
		return simGroupMap.insert( newmember, group_inst->id());
	}
	else
	{
		return false;
	}
}

///\brief Eval the group closest to a sample that is closer than a min dist
static FeatureIndex getSampleClosestSimGroup(
				SampleIndex main_sampleidx, SampleIndex search_sampleidx, 
				unsigned short min_dist,
				const GroupList& groupList,
				const GroupInstanceMap& groupInstanceMap,
				const SimGroupMap& simGroupMap,
				const SimHashCollection& samplear)
{
	FeatureIndex rt = 0;
	SimGroupMap::const_node_iterator gi = simGroupMap.node_begin( main_sampleidx), ge = simGroupMap.node_end( main_sampleidx);
	for (; gi != ge; ++gi)
	{
		GroupInstanceMap::const_iterator group_slot = groupInstanceMap.find( *gi);
		if (group_slot == groupInstanceMap.end())
		{
			throw strus::runtime_error(_TXT("illegal reference in group map (getSampleClosestSimGroup): %u"), *gi);
		}
		GroupList::const_iterator group_inst = group_slot->second;

		// The new element is closer to a group already existing, then add it there
		if (samplear[ search_sampleidx].near( group_inst->gencode(), min_dist))
		{
			rt = group_inst->id();
			min_dist = samplear[ search_sampleidx].dist( group_inst->gencode());
			break;
		}
	}
	return rt;
}

/// \brief Find the closest sample to a given sample that is not yet in a group with this sample and can still be assigned to a new group
static bool findClosestFreeSample( SimRelationMap::Element& res, SampleIndex sampleidx, const SimGroupMap& simGroupMap, const SimRelationMap& simrelmap)
{
	SimRelationMap::Row row = simrelmap.row( sampleidx);
	SimRelationMap::Row::const_iterator ri = row.begin(), re = row.end();
	for (; ri != re; ++ri)
	{
		if (simGroupMap.hasSpace( ri->index) && !simGroupMap.shares( sampleidx, ri->index))
		{
			res = *ri;
			return true;
		}
	}
	return false;
}


typedef std::pair<unsigned int,unsigned int> Dependency;
typedef std::set<Dependency> DependencyGraph;

/// \brief Build a directed graph of dependencies of groups derived from the map of samples to groups.
/// \note A group is dependent on another if every of its member is also member of the other group
static DependencyGraph buildGroupDependencyGraph( std::size_t nofSamples, const SimGroupMap& simGroupMap)
{
	DependencyGraph rt;
	std::vector<unsigned char> independentmark( nofSamples, 0); /*values are 0,1,2*/
	SampleIndex si=0, se=nofSamples;
	for (; si < se; ++si)
	{
		SimGroupMap::const_node_iterator ni = simGroupMap.node_begin( si), ne = simGroupMap.node_end( si);
		SimGroupMap::const_node_iterator na = ni;
		for (; ni != ne; ++ni)
		{
			if (independentmark[ *ni] == 2)
			{}
			else if (independentmark[ *ni] == 0)
			{
				SimGroupMap::const_node_iterator oni = na, one = ne;
				for (; oni != one; ++oni)
				{
					if (*oni != *ni)
					{
						// postulate *ni dependent from *oni
						rt.insert( Dependency( *ni, *oni));
					}
				}
				independentmark[ *ni] = 1;
			}
			else if (independentmark[ *ni] == 1)
			{
				bool have_found = false;
				std::set<Dependency>::iterator di = rt.upper_bound( Dependency( *ni, 0));
				while (di != rt.end() && di->first == *ni)
				{
					if (simGroupMap.contains( si, di->second))
					{
						have_found = true;
						++di;
					}
					else
					{
						std::set<Dependency>::iterator di_del = di;
						++di;
						rt.erase( di_del);
					}
				}
				if (!have_found)
				{
					independentmark[ *ni] = 2;
					continue;
				}
			}
			else
			{
				throw strus::runtime_error(_TXT("internal: wrong group dependency status"));
			}
		}
	}
	return rt;
}

/// \brief Eliminate circular dependencies from a directed graph represented as set
static void eliminateCircularReferences( DependencyGraph& graph)
{
	std::set<Dependency>::iterator hi = graph.begin(), he = graph.end();
	while (hi != he)
	{
		std::set<unsigned int> visited;		// set of visited for not processing nodes more than once
		std::vector<unsigned int> queue;	// queue with candidates to process
		std::size_t qidx = 0;			// iterator in the candidate queue
		unsigned int gid = hi->first;		// current node visited
		// Iterate through all vertices pointed to by arcs rooted in the current node visited:
		for (++hi; hi != he && gid == hi->first; ++hi)
		{
			// Insert the visited vertex into the queue of nodes to process:
			if (visited.insert( hi->second).second)
			{
				queue.push_back( hi->second);
			}
		}
		// Iterate through the set of reachable vertices and check if they have an arc pointing to
		//   the current node visited. If yes, eliminate this arc to eliminate the circular dependency:
		for (; qidx < queue.size(); ++qidx)
		{
			std::set<Dependency>::iterator
				dep_hi = graph.find( Dependency( queue[qidx], gid));
			if (dep_hi != graph.end())
			{
				// Found a circular reference, eliminate the dependency to the current node:
				graph.erase( dep_hi);
			}
			// Expand the set of vertices directed to from the follow arcs of the processed edge:
			std::set<Dependency>::iterator
				next_hi = graph.upper_bound( Dependency( queue[qidx], 0));
			for (; next_hi != graph.end() && queue[qidx] == next_hi->first; ++next_hi)
			{
				if (visited.insert( next_hi->second).second)
				{
					queue.push_back( hi->second);
				}
			}
		}
	}
}

#ifdef STRUS_LOWLEVEL_DEBUG
static void checkSimGroupStructures(
				const GroupList& groupList,
				const GroupInstanceMap& groupInstanceMap,
				const SimGroupMap& simGroupMap,
				std::size_t nofSamples)
{
	std::cerr << "check structures:" << std::endl;
	simGroupMap.check();
	std::ostringstream errbuf;
	bool haserr = false;

	GroupList::const_iterator gi = groupList.begin(), ge = groupList.end();
	for (; gi != ge; ++gi)
	{
		std::cerr << "group " << gi->id() << " members = ";
		SimGroup::const_iterator mi = gi->begin(), me = gi->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) std::cerr << ", ";
			std::cerr << *mi;
			if (!simGroupMap.contains( *mi, gi->id()))
			{
				errbuf << "missing element group relation in simGroupMap: " << *mi << " IN " << gi->id() << std::endl;
				haserr = true;
			}
		}
		std::cerr << std::endl;
		gi->check();

		GroupInstanceMap::const_iterator gi_slot = groupInstanceMap.find( gi->id());
		if (gi_slot == groupInstanceMap.end())
		{
			errbuf << "missing entry in sim group map (check structures): " << gi->id() << std::endl;
			haserr = true;
		}
	}
	SampleIndex si=0, se=nofSamples;
	for (; si != se; ++si)
	{
		SimGroupMap::const_node_iterator ni = simGroupMap.node_begin( si), ne = simGroupMap.node_end( si);
		unsigned int nidx=0;
		if (ni != ne) std::cerr << "sample " << si << " groups = ";
		for (; ni != ne; ++ni,++nidx)
		{
			if (nidx) std::cerr << ", ";
			std::cerr << *ni;

			GroupInstanceMap::const_iterator gi_slot = groupInstanceMap.find( *ni);
			if (gi_slot == groupInstanceMap.end())
			{
				errbuf << "entry not found in group instance map (check structures): " << *ni << std::endl;
				haserr = true;
			}
			GroupList::const_iterator gi = gi_slot->second;
			if (!gi->isMember( si))
			{
				errbuf << "illegal entry in sim group map (check structures): expected " << si << " to be member of group " << gi->id() << std::endl;
				haserr = true;
			}
		}
		if (nidx) std::cerr << std::endl;
	}
	if (haserr)
	{
		throw std::runtime_error( errbuf.str());
	}
}
#endif

SimHashCollection GenModel::run( const SimHashCollection& samplear, const char* logfile) const
{
	GroupIdAllocator groupIdAllocator;					// Allocator of group ids
	GroupList groupList;					// list of similarity group representants
	GroupInstanceMap groupInstanceMap;					// map indices to group representant list iterators
	SimGroupMap simGroupMap( samplear.size());				// map of sample idx to group idx
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "build similarity relation map" << std::endl;
#endif
	SimRelationMap simrelmap( getSimRelationMap( samplear, m_simdist));	// similarity relation map of the list of samples
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "got similarity relation map:" << std::endl << simrelmap.tostring() << std::endl;
#endif
	std::ostream* logout = &std::cerr;
	std::ofstream logfilestream;
	if (logfile)
	{
		if (logfile[0] != '-' || logfile[1])
		{
			try
			{
				logfilestream.open( logfile, std::ofstream::out);
				logout = &logfilestream;
			}
			catch (const std::exception& err)
			{
				throw strus::runtime_error(_TXT("failed to open logfile '%s': %s"), logfile, err.what());
			}
		}
	}
	// Do the iterations of creating new individuals
	unsigned int iteration=0;
	for (; iteration != m_iterations; ++iteration)
	{
		if (logfile) (*logout) << "iteration " << iteration << ":" << std::endl;
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "GenModel::run iteration " << iteration << std::endl;
		checkSimGroupStructures( groupList, groupInstanceMap, simGroupMap, samplear.size());

		std::cerr << "existing groups:";
		GroupList::iterator li = groupList.begin(), le = groupList.end();
		for (; li != le; ++li)
		{
			std::cerr << " " << li->id();
		}
		std::cerr << std::endl;
		std::cerr << "create new individuals ..." << std::endl;
#endif
		if (logfile) (*logout) << "create new groups" << std::endl;
		// Go through all elements and try to create new groups with the closest free neighbours:
		SimHashCollection::const_iterator si = samplear.begin(), se = samplear.end();
		for (SampleIndex sidx=0; si != se; ++si,++sidx)
		{
			SimHashAllocator simHashAllocator( samplear.nofbits());
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << "visit sample " << sidx << std::endl;
#endif
			if (!simGroupMap.hasSpace( sidx)) continue;

			// Find the closest neighbour, that is not yet in a group with this sample:
			SimRelationMap::Element neighbour;
			if (findClosestFreeSample( neighbour, sidx, simGroupMap, simrelmap))
			{
				// Try to find a group the visited sample belongs to that is closer to the
				// found candidate than the visited sample:
				FeatureIndex bestmatch_simgroup = getSampleClosestSimGroup( sidx, neighbour.index, neighbour.simdist, groupList, groupInstanceMap, simGroupMap, samplear);
				if (bestmatch_simgroup)
				{
					// ...if we found such a group, we try to add the candidate there instead:
					if (tryAddGroupMember( bestmatch_simgroup, neighbour.index, groupInstanceMap, simGroupMap, samplear, m_descendants, m_mutations, m_votes, m_maxage, simHashAllocator))
					{
#ifdef STRUS_LOWLEVEL_DEBUG
						std::cerr << "add new member " << neighbour.index << " to closest group " << bestmatch_simgroup << std::endl;
#endif
					}
					else
					{
						bestmatch_simgroup = 0;
					}
				}
				if (!bestmatch_simgroup)
				{
					// ...if we did not find such a group we found a new one with the two
					// elements as members:
					SimGroup newgroup( samplear, sidx, neighbour.index, groupIdAllocator.alloc(), simHashAllocator);
					newgroup.mutate( samplear, m_descendants, age_mutations( newgroup, m_maxage, m_mutations), age_mutation_votes( newgroup, m_maxage, m_votes), simHashAllocator);
					simGroupMap.insert( neighbour.index, newgroup.id());
					simGroupMap.insert( sidx, newgroup.id());
					groupList.push_back( newgroup);
					GroupList::iterator enditr = groupList.end();
					groupInstanceMap[ newgroup.id()] = --enditr;
#ifdef STRUS_LOWLEVEL_DEBUG
					std::cerr << "create new group " << newgroup.id() << " with members " << sidx << " and " << neighbour_sampleidx << std::endl;
#endif
				}
			}
		}
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "find neighbour groups out of " << groupIdAllocator.nofGroupsAllocated() << std::endl;
#endif
		if (logfile) (*logout) << "unify individuals ..." << std::endl;

		// Go through all groups and try to make elements jump to neighbour groups and try
		// to unify groups:
		GroupList::iterator gi = groupList.begin(), ge = groupList.end();
		for (; gi != ge; ++gi)
		{
			SimHashAllocator simHashAllocator( samplear.nofbits());
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << "visit group " << gi->id() << " age " << gi->age() << std::endl;
#endif
			// Build the set of neighbour groups:
			std::set<FeatureIndex> neighbour_groups;
			SimGroup::const_iterator mi = gi->begin(), me = gi->end();
			for (; mi != me; ++mi)
			{
				SimGroupMap::const_node_iterator si = simGroupMap.node_begin(*mi), se = simGroupMap.node_end(*mi);
				for (; si != se; ++si)
				{
					if (*si != gi->id())
					{
#ifdef STRUS_LOWLEVEL_DEBUG
						std::cerr << "found neighbour group " << *si << " member sample " << *mi << std::endl;
#endif
						neighbour_groups.insert( *si);
					}
				}
			}
			// Go through all neighbour groups that are in a distance closer to eqdist
			// and delete them if the members are all within the group:
			std::set<FeatureIndex>::const_iterator ni = neighbour_groups.begin(), ne = neighbour_groups.end();
			for (; ni != ne; ++ni)
			{
#ifdef STRUS_LOWLEVEL_DEBUG
				std::cerr << "found neighbour group " << *ni << std::endl;
#endif
				GroupInstanceMap::iterator sim_gi_slot = groupInstanceMap.find( *ni);
				if (sim_gi_slot == groupInstanceMap.end()) throw strus::runtime_error(_TXT("illegal reference in group map (join eqdist groups): %u"), *ni);
				GroupList::iterator sim_gi = sim_gi_slot->second;

				if (sim_gi->gencode().near( gi->gencode(), m_eqdist))
				{
					// Try to swallow members in sim_gi as long as it is in eqdist:
					SimGroup::const_iterator 
						sim_mi = sim_gi->begin(), sim_me = sim_gi->end();
					for (; sim_mi != sim_me; ++sim_mi)
					{
						if (!gi->isMember( *sim_mi))
						{
							if (!simGroupMap.hasSpace( *sim_mi)) break;
							// Add member of sim_gi to gi:
							gi->addMember( *sim_mi);
							simGroupMap.insert( *sim_mi, gi->id());
							gi->mutate( samplear, m_descendants, age_mutations( *gi, m_maxage, m_mutations), age_mutation_votes( *gi, m_maxage, m_votes), simHashAllocator);
							if (!sim_gi->gencode().near( gi->gencode(), m_eqdist))
							{
								break;
							}
						}
					}
					if (sim_mi == sim_me)
					{
						// ... delete the neighbour group sim_gi where all elements 
						// can be added to gi and it is still in eqdist:
#ifdef STRUS_LOWLEVEL_DEBUG
						std::cerr << "remove group (swallowed by another group) " << sim_gi->id() << std::endl;
#endif
						if (sim_gi->fitness( samplear) < gi->fitness( samplear))
						{
							removeGroup( sim_gi->id(), groupIdAllocator, groupList, groupInstanceMap, simGroupMap);
						}
						sim_gi = groupList.end();
					}
				}
				if (sim_gi != groupList.end() && gi->gencode().near( sim_gi->gencode(), m_simdist))
				{
					// Try add one member of sim_gi to gi that is similar to gi:
					SimGroup::const_iterator mi = sim_gi->begin(), me = sim_gi->end();
					for (; mi != me; ++mi)
					{
						if (gi->gencode().near( samplear[*mi], m_simdist) && !gi->isMember( *mi))
						{
							if (simGroupMap.hasSpace( *mi))
							{
								if (tryAddGroupMember( gi->id(), *mi, groupInstanceMap, simGroupMap, samplear, m_descendants, m_mutations, m_votes, m_maxage, simHashAllocator))
								{
									break;
								}
							}
						}
					}
					// Eliminate similar group that contains most of this group members and that has a smaller fitness:
					unsigned int eqdiff = std::min( gi->size(), sim_gi->size()) / 4;
					if (gi->diffMembers( *sim_gi, eqdiff) < eqdiff)
					{
						if (sim_gi->fitness( samplear) < gi->fitness( samplear))
						{
							removeGroup( sim_gi->id(), groupIdAllocator, groupList, groupInstanceMap, simGroupMap);
						}
						sim_gi = groupList.end();
					}
				}
			}
		}
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << "start mutation step" << std::endl;
#endif
		if (logfile) (*logout) << "mutations ..." << std::endl;

		// Mutation step for all groups and dropping of elements that got too far away from the
		// representants genom:
		gi = groupList.begin(), ge = groupList.end();
		for (; gi != ge; ++gi)
		{
			SimHashAllocator simHashAllocator( samplear.nofbits());
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << "visit group " << gi->id() << std::endl;
#endif
			gi->mutate( samplear, m_descendants, age_mutations( *gi, m_maxage, m_mutations), age_mutation_votes( *gi, m_maxage, m_votes), simHashAllocator);

			SimGroup::const_iterator mi = gi->begin(), me = gi->end();
			for (std::size_t midx=0; mi != me; ++mi,++midx)
			{
				unsigned int dist = m_raddist + ((m_simdist - m_raddist) * (m_iterations - iteration - 1) / (m_iterations - 1));
				if (!gi->gencode().near( samplear[ *mi], dist))
				{
					// Dropped members that got too far out of the group:
					SampleIndex member = *mi;
					mi = gi->removeMemberItr( mi);
					--mi;
					--midx;
					simGroupMap.remove( member, gi->id());
					if (gi->size() < 2) break;
				}
			}
			if (gi->size() < 2)
			{
				// Delete group that lost too many members:
#ifdef STRUS_LOWLEVEL_DEBUG
				std::cerr << "remove group (too few members left) " << gi->id() << std::endl;
#endif
				FeatureIndex gid = gi->id();
				++gi;
				removeGroup( gid, groupIdAllocator, groupList, groupInstanceMap, simGroupMap);
				--gi;
			}
		}
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "eliminate redundant groups" << std::endl;
#endif
	if (logfile) (*logout) << "done, eliminate redundant groups ..." << std::endl;
	{
		// Build the dependency graph:
		DependencyGraph groupDependencyGraph = buildGroupDependencyGraph( samplear.size(), simGroupMap);

		// Eliminate circular references from the graph:
		eliminateCircularReferences( groupDependencyGraph);

		// Eliminate the groups dependent of others:
		DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
		while (di != de)
		{
			unsigned int gid = di->first;
			removeGroup( gid, groupIdAllocator, groupList, groupInstanceMap, simGroupMap);
			for (++di; di != de && gid == di->first; ++di){}
		}
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "build the result" << std::endl;
#endif
	if (logfile) (*logout) << "build the result ..." << std::endl;

	// Build the result:
	SimHashCollection rt( samplear.nofbits());
	GroupList::const_iterator gi = groupList.begin(), ge = groupList.end();
	for (; gi != ge; ++gi)
	{
		rt.push_back( gi->gencode());
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "got " << rt.size() << " categories" << std::endl;
	gi = groupList.begin(), ge = groupList.end();
	for (; gi != ge; ++gi)
	{
		std::cerr << "category " << gi->id() << ": ";
		SimGroup::const_iterator mi = gi->begin(), me = gi->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) std::cerr << ", ";
			std::cerr << *mi;
		}
		std::cerr << std::endl;
	}
#endif
	if (logfile) (*logout) << "done, got " << rt.size() << " categories"<< std::endl;
	return rt;
}

std::string GenModel::tostring() const
{
	std::ostringstream rt;
	rt << "simdist=" << m_simdist << std::endl
		<< ", eqdist=" << m_eqdist << std::endl
		<< ", mutations=" << m_mutations << std::endl
		<< ", votes=" << m_votes << std::endl
		<< ", descendants=" << m_descendants << std::endl
		<< ", maxage=" << m_maxage << std::endl
		<< ", iterations=" << m_iterations << std::endl;
	return rt.str();
}

