/****
DIAMOND protein aligner
Copyright (C) 2013-2019 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#ifndef DP_H_
#define DP_H_

#include <list>
#include <vector>
#include "../util/simd.h"
#include "../basic/sequence.h"
#include "../basic/match.h"
#include "comp_based_stats.h"
#include "../basic/statistics.h"
#include "score_profile.h"

int smith_waterman(const sequence &query, const sequence &subject, unsigned band, unsigned padding, int op, int ep);

struct Local {};
struct Global {};

template<typename _t>
struct Fixed_score_buffer
{

	inline void init(size_t col_size, size_t cols, _t init)
	{
		col_size_ = col_size;
		data_.clear();
		data_.reserve(col_size * cols);
		data_.resize(col_size);
		for (size_t i = 0; i<col_size; ++i)
			data_[i] = init;
	}
	
	std::pair<int, int> find(_t s) const
	{
		const int i = int(std::find(data_.begin(), data_.end(), s) - data_.begin());
		return std::pair<int, int>(int(i%col_size_), int(i / col_size_));
	}

	inline std::pair<_t*, _t*> get()
	{
		data_.resize(data_.size() + col_size_);
		_t* ptr = last();
		return std::pair<_t*, _t*>(ptr - col_size_, ptr);
	}

	inline _t* last()
	{
		return &*(data_.end() - col_size_);
	}

	const _t* column(int col) const
	{
		return &data_[col_size_*col];
	}

	_t operator()(int i, int j) const
	{
		return data_[j*col_size_ + i];
	}

	friend std::ostream& operator<<(std::ostream &s, const Fixed_score_buffer &buf)
	{
		s << '\t';
		for (int j = 0; j < int(buf.data_.size() / buf.col_size_); ++j)
			s << j << '\t';
		s << endl;
		for (int i = 0; i < int(buf.col_size_); ++i) {
			s << i << '\t';
			for (int j = 0; j < int(buf.data_.size() / buf.col_size_); ++j)
				s << buf(i, j) << '\t';
			s << endl;
		}
		return s;
	}

private:
	vector<_t> data_;
	size_t col_size_;

};

template<typename _score, typename _mode>
const Fixed_score_buffer<_score>& needleman_wunsch(sequence query, sequence subject, int &max_score, const _mode&, const _score&);

struct Band
{
	void init(int diags, int cols)
	{
		diags_ = diags;
		cols_ = cols;
		data_.clear();
		data_.resize((size_t)diags*cols);
	}
	struct Iterator {
		Iterator(uint8_t *p, int diags) :
			diags_(diags),
			p_(p)			
		{}
		uint8_t& operator[](int i)
		{
			return p_[i*diags_];
		}
	private:
		const int diags_;
		uint8_t *p_;
	};
	Iterator diag(int o)
	{
		return Iterator(&data_[o], diags_);
	}
	int cols() const
	{
		return cols_;
	}
	int diags() const
	{
		return diags_;
	}
	uint8_t* data()
	{
		return data_.data();
	}
	bool check(uint8_t *ptr) const
	{
		return ptr >= data_.data() && ptr <= data_.data() + data_.size();
	}
private:
	int diags_, cols_;
	vector<uint8_t> data_;
};

extern size_t cells;

struct DpTarget
{
	DpTarget(const sequence &seq, int d_begin, int d_end, int j_begin, int j_end, int target_idx = 0) :
		seq(seq),
		d_begin(d_begin),
		d_end(d_end),
		j_begin(j_begin),
		j_end(j_end),
		target_idx(target_idx)
	{}
	int left_i1() const
	{
		return std::max(d_end - 1, 0);
	}
	bool operator<(const DpTarget &x) const
	{
		const int i = left_i1(), j = x.left_i1();
		return i < j || (i == j && (target_idx < x.target_idx || (target_idx == x.target_idx && d_begin < x.d_begin)));
	}
	sequence seq;
	int d_begin, d_end, j_begin, j_end, target_idx;
};

struct DpStat
{
	DpStat():
		gross_cells(0),
		net_cells(0)
	{}
	DpStat& operator+=(DpStat &x)
	{
		mtx_.lock();
		gross_cells += x.gross_cells;
		net_cells += x.net_cells;
		mtx_.unlock();
		return *this;
	}
	size_t gross_cells, net_cells;
private:
	std::mutex mtx_;
};

extern DpStat dp_stat;

void smith_waterman(sequence q, sequence s, Hsp &out);
int score_range(sequence query, sequence subject, int i, int j, int j_end);

namespace DP {

struct Traceback {};
struct ScoreOnly {};

enum { TRACEBACK = 1, PARALLEL = 2 };

struct NoCBS {
	constexpr void* operator[](int i) const { return nullptr; }
};
	
namespace Swipe {

DECL_DISPATCH(std::list<Hsp>, swipe, (const sequence &query, const sequence *subject_begin, const sequence *subject_end, int score_cutoff))

}

namespace BandedSwipe {

DECL_DISPATCH(std::list<Hsp>, swipe, (const sequence &query, std::vector<DpTarget> &targets8, std::vector<DpTarget> &targets16, Frame frame, const Bias_correction *composition_bias, int flags, int score_cutoff, Statistics &stat))

}

DECL_DISPATCH(void, scan_diags128, (const LongScoreProfile& qp, sequence s, int d_begin, int j_begin, int j_end, int* out))
DECL_DISPATCH(void, scan_diags64, (const LongScoreProfile& qp, sequence s, int d_begin, int j_begin, int j_end, int* out))
DECL_DISPATCH(int, diag_alignment, (const int* s, int count))

}

void banded_sw(const sequence &query, const sequence &subject, int d_begin, int d_end, int j_begin, int j_end, Hsp &out);

void anchored_3frame_dp(const TranslatedSequence &query, sequence &subject, const DiagonalSegment &anchor, Hsp &out, int gap_open, int gap_extend, int frame_shift);
int sw_3frame(const TranslatedSequence &query, Strand strand, const sequence &subject, int gap_open, int gap_extend, int frame_shift, Hsp &out);

DECL_DISPATCH(std::list<Hsp>, banded_3frame_swipe, (const TranslatedSequence &query, Strand strand, vector<DpTarget>::iterator target_begin, vector<DpTarget>::iterator target_end, DpStat &stat, bool score_only, bool parallel))

#endif