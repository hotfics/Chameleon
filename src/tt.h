/*
  Chameleon, a UCI chinese chess playing engine derived from Stockfish
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2017 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2017 Wilbert Lee

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TT_H_INCLUDED
#define TT_H_INCLUDED

#include "misc.h"
#include "types.h"

// TTEntry struct is the 10 bytes transposition table entry, defined as below:
//
// key        16 bit
// move       16 bit
// value      16 bit
// eval value 16 bit
// generation  6 bit
// bound type  2 bit
// depth       8 bit
struct TTEntry
{
	Move  move()  const { return (Move)move16; }
	Value value() const { return (Value)value16; }
	Value eval()  const { return (Value)eval16; }
	Depth depth() const { return (Depth)depth8; }
	Bound bound() const { return (Bound)(genBound8 & 0x3); }

	void save(uint64_t k, Value v, Bound b, Depth d, Move m, Value ev, uint8_t g)
	{
		// Preserve any existing move for the same position
		if (m || (k >> 48) != key16)
			move16 = (uint16_t)m;

		// Don't overwrite more valuable entries
		if ((k >> 48) != key16
			|| d > depth8 - 2
			|| b == BOUND_EXACT)
		{
			key16 = (uint16_t)(k >> 48);
			value16 = (int16_t)v;
			eval16 = (int16_t)ev;
			genBound8 = (uint8_t)(g | b);
			depth8 = (int8_t)d;
		}
	}

private:
	friend class TranspositionTable;

	uint16_t key16;
	uint16_t move16;
	int16_t  value16;
	int16_t  eval16;
	uint8_t  genBound8;
	int8_t   depth8;
};

// A TranspositionTable consists of a power of 2 number of clusters and each
// cluster consists of ClusterSize number of TTEntry. Each non-empty entry
// contains information of exactly one position. The size of a cluster should
// divide the size of a cache line size, to ensure that clusters never cross
// cache lines. This ensures best cache performance, as the cacheline is
// prefetched, as soon as possible.
class TranspositionTable
{
	static const int CacheLineSize = 64;
	static const int ClusterSize = 3;

	struct Cluster
	{
		TTEntry entry[ClusterSize];
		char padding[2]; // Align to a divisor of the cache line size
	};

	static_assert(CacheLineSize % sizeof(Cluster) == 0, "Cluster size incorrect");

public:
	~TranspositionTable() { free(mem); }
	void init() { resize(DEFAULT_HASH_MB); };
	void new_search() { generation8 += 4; } // Lower 2 bits are used by Bound
	uint8_t generation() const { return generation8; }
	TTEntry* probe(const uint64_t key, bool& found) const;
	int hashfull() const;
	void resize(size_t mbSize);
	void clear();

	/// first_entry() the lowest order bits of the key are
	/// used to get the index of the cluster.
	TTEntry* first_entry(const uint64_t key) const
	{
		return &table[(size_t)key & (clusterCount - 1)].entry[0];
	}

private:
	size_t clusterCount;
	Cluster* table;
	void* mem;
	uint8_t generation8; // Size must be not bigger than TTEntry::genBound8
};

extern TranspositionTable TT;

#endif // #ifndef TT_H_INCLUDED
