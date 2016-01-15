#include "stdafx.h"

#include "configuration.h"

namespace config {

	const Configuration Configuration::DEFAULT = {
		1466,						// macine cycles per step

		10,							// display cell size
		{ 0x00, 0xA7, 0x00 },		// display cell color
		{ 0x1A, 0x1A, 0x1A }		// display void color
	};


	bool read(std::istream &is, Configuration &config) {
		config = Configuration::DEFAULT;

		return true;
	}

	bool write(std::ostream &os, const Configuration &config) {
		return false;
	}

} // namespace config