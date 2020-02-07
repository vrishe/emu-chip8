#pragma once

namespace config {

	struct Configuration {

		static const Configuration DEFAULT;

		// Interpretation
		size_t machineCyclesPerStep;

		// Display
		unsigned displayCellSize;
		unsigned char displayCellColor[3];
		unsigned char displayVoidColor[3];

		//Speaker
		bool hasSpeaker;
	};

	bool read(std::istream &is, Configuration &config);

	bool write(std::ostream os, const Configuration &config);

} // namespace config