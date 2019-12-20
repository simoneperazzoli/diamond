/****
DIAMOND protein aligner
Copyright (C) 2013-2020 Max Planck Society for the Advancement of Science e.V.
                        Benjamin Buchfink
                        Eberhard Karls Universitaet Tuebingen

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

#include <algorithm>
#include "target.h"
#include "../basic/config.h"

using std::vector;

namespace Extension {

void score_only_culling(vector<Target> &targets) {
	std::stable_sort(targets.begin(), targets.end());

	if (config.toppercent == 100.0 && (config.min_id > 0 || config.query_cover > 0 || config.subject_cover > 0 || config.no_self_hits))
		return;

	vector<Target>::iterator i;
	if (config.toppercent < 100.0) {
		const int cutoff = (1.0 - config.toppercent / 100.0)*targets.front().filter_score;
		while (i < targets.end() && i->filter_score >= cutoff)
			++i;
	}
	else
		i = targets.begin() + std::min(config.max_alignments, targets.size());
	targets.erase(i, targets.end());
}

}