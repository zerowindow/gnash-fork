//   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


#ifndef __GNASH_UTIL_H
#define __GNASH_UTIL_H

#include <boost/cstdint.hpp> // for boost::int16_t

namespace gnash {
namespace media {


class Util {

public:
/// VERY crude sample-rate and steroe conversion. Converts input data to 
/// output format.
//
/// @param adjusted_data
/// Where the converted data is placed.
///
/// @param adjusted_size
/// The size of the converted data.
///
/// @param data
/// Data that needs to be converted.
///
/// @param sample_count
/// The datas current sample count.
/// 
/// @param sample_size
/// The datas current sample size.
///
/// @param sample_rate
/// The datas current sample rate.
///
/// @param stereo
/// Whether the current data is in stereo
///
/// @param m_sample_rate
/// The samplerate we which to convert to.
///
/// @param m_stereo
/// Do we want the output data to be in stereo?
///
	static void convert_raw_data(boost::int16_t** adjusted_data,
		  int* adjusted_size, void* data, int sample_count,
		  int sample_size, int sample_rate, bool stereo,
		  int m_sample_rate, bool m_stereo);
};

} // namespace media
} // namespace gnash

#endif // __GNASH_UTIL_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End: