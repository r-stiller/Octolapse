#pragma once
#include "position.h"
#define NUM_FEATURE_TYPES 10
static const std::string feature_type_name[NUM_FEATURE_TYPES] = {
		"bridge_feature", "unknown_feature", "outer_perimeter_feature", "unknown_perimeter_feature", "inner_perimeter_feature", "skirt_feature", "solid_infill_feature", "ooze_shield_feature", "infill_feature", "prime_pillar_feature"
};
static const int feature_type_quality[NUM_FEATURE_TYPES] = {
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8
};

class gcode_comment_processor
{
	
public:
	enum feature_type { unknown_feature, bridge_feature, outer_perimeter_feature, unknown_perimeter_feature, inner_perimeter_feature, skirt_feature, solid_infill_feature, ooze_shield_feature, infill_feature, prime_pillar_feature };
	enum comment_process_type { OFF, UNKNOWN, SLIC3R_PE, CURA, SIMPLIFY_3D };
	// used for marking slicer sections for cura and simplify 3d
	enum section_type { no_section, outer_perimeter_section, inner_perimeter_section, infill_section, skirt_section, solid_layer_section, ooze_shield_section, prime_pillar_section };
	gcode_comment_processor();
	~gcode_comment_processor();
	void update(position& pos);
	void update(std::string & comment);
private:
	section_type current_section_;
	comment_process_type processing_type_;
	void update_feature_from_section(position& pos);
	bool update_feature_from_section_for_unknown(position& pos) const;
	bool update_feature_from_section_for_cura(position& pos) const;
	bool update_feature_from_section_for_simplify_3d(position& pos) const;
	bool update_feature_from_section_for_slice3r_pe(position& pos) const;
	void update_feature_for_unknown_slicer_comment(position& pos, std::string &comment);
	bool update_feature_for_slic3r_pe_comment(position& pos, std::string &comment) const;
	void update_unknown_section(std::string & comment);
	bool update_cura_section(std::string &comment);
	bool update_simplify_3d_section(std::string &comment);
	bool update_slic3r_pe_section(std::string &comment);
	
	
};

