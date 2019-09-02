////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Octolapse - A plugin for OctoPrint used for making stabilized timelapse videos.
// Copyright(C) 2019  Brad Hochgesang
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.If not, see the following :
// https ://github.com/FormerLurker/Octolapse/blob/master/LICENSE
//
// You can contact the author either through the git - hub repository, or at the
// following email address : FormerLurker@pm.me
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "gcode_position.h"
#include "utilities.h"
#include "logging.h"

gcode_position::gcode_position()
{

	autodetect_position_ = false;
	home_x_ = 0;
	home_y_ = 0;
	home_z_ = 0;
	home_x_none_ = true;
	home_y_none_ = true;
	home_z_none_ = true;

	retraction_length_ = 0;
	z_lift_height_ = 0;
	priming_height_ = 0;
	minimum_layer_height_ = 0;
	g90_influences_extruder_ = false;
	e_axis_default_mode_ = "absolute";
	xyz_axis_default_mode_ = "absolute";
	units_default_ = "millimeters";
	gcode_functions_ = get_gcode_functions();

	is_bound_ = false;
	snapshot_x_min_ = 0;
	snapshot_x_max_ = 0;
	snapshot_y_min_ = 0;
	snapshot_y_max_ = 0;
	snapshot_z_min_ = 0;
	snapshot_z_max_ = 0;

	x_min_ = 0;
	x_max_ = 0;
	y_min_ = 0;
	y_max_ = 0;
	z_min_ = 0;
	z_max_ = 0;
	is_circular_bed_ = false;

	cur_pos_ = 0;
	position initial_pos;
	initial_pos.set_xyz_axis_mode(xyz_axis_default_mode_);
	initial_pos.set_e_axis_mode(e_axis_default_mode_);
	initial_pos.set_units_default(units_default_);
	for(int index = 0; index < NUM_POSITIONS; index ++)
	{
		add_position(initial_pos);
	}
	
	
}

gcode_position::gcode_position(gcode_position_args* args)
{
	autodetect_position_ = args->autodetect_position;
	home_x_ = args->home_x;
	home_y_ = args->home_y;
	home_z_ = args->home_z;
	home_x_none_ = args->home_x_none;
	home_y_none_ = args->home_y_none;
	home_z_none_ = args->home_z_none;

	retraction_length_ = args->retraction_length;
	z_lift_height_ = args->z_lift_height;
	priming_height_ = args->priming_height;
	minimum_layer_height_ = args->minimum_layer_height;
	g90_influences_extruder_ = args->g90_influences_extruder;
	e_axis_default_mode_ = args->e_axis_default_mode;
	xyz_axis_default_mode_ = args->xyz_axis_default_mode;
	units_default_ = args->units_default;
	gcode_functions_ = get_gcode_functions();

	is_bound_ = args->is_bound_;
	snapshot_x_min_ = args->snapshot_x_min;
	snapshot_x_max_ = args->snapshot_x_max;
	snapshot_y_min_ = args->snapshot_y_min;
	snapshot_y_max_ = args->snapshot_y_max;
	snapshot_z_min_ = args->snapshot_z_min;
	snapshot_z_max_ = args->snapshot_z_max;

	x_min_ = args->x_min;
	x_max_ = args->x_max;
	y_min_ = args->y_min;
	y_max_ = args->y_max;
	z_min_ = args->z_min;
	z_max_ = args->z_max;

	is_circular_bed_ = args->is_circular_bed;

	cur_pos_ = -1;
	position initial_pos;
	initial_pos.set_xyz_axis_mode(xyz_axis_default_mode_);
	initial_pos.set_e_axis_mode(e_axis_default_mode_);
	initial_pos.set_units_default(units_default_);
	for (int index = 0; index < NUM_POSITIONS; index++)
	{
		add_position(initial_pos);
	}

}

gcode_position::gcode_position(const gcode_position &source)
{
	// Private copy constructor - you can't copy this class
}


void gcode_position::add_position(position& pos)
{
	cur_pos_ = (++cur_pos_) % NUM_POSITIONS;
	positions_[cur_pos_] = pos;
}

void gcode_position::add_position(parsed_command& cmd)
{
	const int prev_pos = cur_pos_;
	cur_pos_ = (++cur_pos_) % NUM_POSITIONS;
	positions_[cur_pos_] = positions_[prev_pos];
	positions_[cur_pos_].reset_state();
	positions_[cur_pos_].command = cmd;
	positions_[cur_pos_].is_empty = false;
}

position gcode_position::get_current_position() const
{
	return positions_[cur_pos_];
}

position gcode_position::get_previous_position() const
{
	return  positions_[(cur_pos_ - 1 + NUM_POSITIONS) % NUM_POSITIONS];
}

position * gcode_position::get_current_position_ptr()
{
	return &positions_[cur_pos_];
}

position * gcode_position::get_previous_position_ptr()
{

	return &positions_[(cur_pos_ - 1 + NUM_POSITIONS) % NUM_POSITIONS];
}

void gcode_position::update(parsed_command& command, const int file_line_number, const int gcode_number)
{
	if (command.command.empty())
	{
		// process any comment sections
		comment_processor_.update(command.comment);
		return;
	}
	// Move the current position to the previous and the previous to the undo position
	// then copy previous to current

	
	add_position(command);
	position * p_current_pos = get_current_position_ptr();
	position * p_previous_pos = get_previous_position_ptr();

	comment_processor_.update(*p_current_pos);

	p_current_pos->file_line_number = file_line_number;
	p_current_pos->gcode_number = gcode_number;
	// Does our function exist in our functions map?
	gcode_functions_iterator_ = gcode_functions_.find(command.command);

	if (gcode_functions_iterator_ != gcode_functions_.end())
	{
		p_current_pos->gcode_ignored = false;
		// Execute the function to process this gcode
		const pos_function_type func = gcode_functions_iterator_->second;
		(this->*func)(p_current_pos, command);
		// calculate z and e relative distances
		p_current_pos->e_relative = (p_current_pos->e - p_previous_pos->e);
		p_current_pos->z_relative = (p_current_pos->z - p_previous_pos->z);
		// Have the XYZ positions changed after processing a command ?

		p_current_pos->has_xy_position_changed = (
			!utilities::is_equal(p_current_pos->x, p_previous_pos->x) ||
			!utilities::is_equal(p_current_pos->y, p_previous_pos->y)
			);
		p_current_pos->has_position_changed = (
			p_current_pos->has_xy_position_changed ||
			!utilities::is_equal(p_current_pos->z, p_previous_pos->z) ||
			!utilities::is_zero(p_current_pos->e_relative) ||
			p_current_pos->x_null != p_previous_pos->x_null ||
			p_current_pos->y_null != p_previous_pos->y_null ||
			p_current_pos->z_null != p_previous_pos->z_null);

		// see if our position is homed
		if (!p_current_pos->has_definite_position)
		{
			p_current_pos->has_definite_position = (
				//p_current_pos->x_homed_ &&
				//p_current_pos->y_homed_ &&
				//p_current_pos->z_homed_ &&
				p_current_pos->is_metric &&
				!p_current_pos->is_metric_null &&
				!p_current_pos->x_null &&
				!p_current_pos->y_null &&
				!p_current_pos->z_null &&
				!p_current_pos->is_relative_null &&
				!p_current_pos->is_extruder_relative_null);
		}
	}

	if (p_current_pos->has_position_changed)
	{
		p_current_pos->extrusion_length_total += p_current_pos->e_relative;

		if (utilities::greater_than(p_current_pos->e_relative, 0) && p_previous_pos->is_extruding && !p_previous_pos->is_extruding_start)
		{
			// A little shortcut if we know we were extruding (not starting extruding) in the previous command
			// This lets us skip a lot of the calculations for the extruder, including the state calculation
			p_current_pos->extrusion_length = p_current_pos->e_relative;
		}
		else
		{

			// Update retraction_length and extrusion_length
			p_current_pos->retraction_length = p_current_pos->retraction_length - p_current_pos->e_relative;
			if (utilities::less_than_or_equal(p_current_pos->retraction_length, 0))
			{
				// we can use the negative retraction length to calculate our extrusion length!
				p_current_pos->extrusion_length = -1.0 * p_current_pos->retraction_length;
				// set the retraction length to 0 since we are extruding
				p_current_pos->retraction_length = 0;
			}
			else
				p_current_pos->extrusion_length = 0;

			// calculate deretraction length
			if (utilities::greater_than(p_previous_pos->retraction_length, p_current_pos->retraction_length))
			{
				p_current_pos->deretraction_length = p_previous_pos->retraction_length - p_current_pos->retraction_length;
			}
			else
				p_current_pos->deretraction_length = 0;

			// *************Calculate extruder state*************
			// rounding should all be done by now
			p_current_pos->is_extruding_start = utilities::greater_than(p_current_pos->extrusion_length, 0) && !p_previous_pos->is_extruding;
			p_current_pos->is_extruding = utilities::greater_than(p_current_pos->extrusion_length, 0);
			p_current_pos->is_primed = utilities::is_zero(p_current_pos->extrusion_length) && utilities::is_zero(p_current_pos->retraction_length);
			p_current_pos->is_retracting_start = !p_previous_pos->is_retracting && utilities::greater_than(p_current_pos->retraction_length, 0);
			p_current_pos->is_retracting = utilities::greater_than(p_current_pos->retraction_length, p_previous_pos->retraction_length);
			p_current_pos->is_partially_retracted = utilities::greater_than(p_current_pos->retraction_length, 0) && utilities::less_than(p_current_pos->retraction_length, retraction_length_);
			p_current_pos->is_retracted = utilities::greater_than_or_equal(p_current_pos->retraction_length, retraction_length_);
			p_current_pos->is_deretracting_start = utilities::greater_than(p_current_pos->deretraction_length, 0) && !p_previous_pos->is_deretracting;
			p_current_pos->is_deretracting = utilities::greater_than(p_current_pos->deretraction_length, p_previous_pos->deretraction_length);
			p_current_pos->is_deretracted = utilities::greater_than(p_previous_pos->retraction_length, 0) && utilities::is_zero(p_current_pos->retraction_length);
			// *************End Calculate extruder state*************
		}

		// Calcluate position restructions
		// TODO:  INCLUDE POSITION RESTRICTION CALCULATIONS!
		// Set is_in_bounds_ to false if we're not in bounds, it will be true at this point
		bool is_in_bounds = true;
		if (is_bound_)
		{
			if (!is_circular_bed_)
			{
				is_in_bounds = !(
					utilities::less_than(p_current_pos->x, snapshot_x_min_) ||
					utilities::greater_than(p_current_pos->x, snapshot_x_max_) ||
					utilities::less_than(p_current_pos->y,  snapshot_y_min_) ||
					utilities::greater_than(p_current_pos->y, snapshot_y_max_) ||
					utilities::less_than(p_current_pos->z, snapshot_z_min_) ||
					utilities::greater_than(p_current_pos->z, snapshot_z_max_)
					);

			}
			else
			{
				double r;
				r = snapshot_x_max_; // good stand in for radius
				const double dist = sqrt(p_current_pos->x*p_current_pos->x + p_current_pos->y*p_current_pos->y);
				is_in_bounds = utilities::less_than_or_equal(dist, r);

			}
			p_current_pos->is_in_bounds = is_in_bounds;
		}

		// calculate last_extrusion_height and height
		// If we are extruding on a higher level, or if retract is enabled and the nozzle is primed
		// adjust the last extrusion height
		if (utilities::greater_than(p_current_pos->z, p_current_pos->last_extrusion_height))
		{
			if (!p_current_pos->z_null)
			{
				// detect layer changes/ printer priming/last extrusion height and height 
				// Normally we would only want to use is_extruding, but we can also use is_deretracted if the layer is greater than 0
				if (p_current_pos->is_extruding || (p_current_pos->layer >0 && p_current_pos->is_deretracted))
				{
					// Is Primed
					if (!p_current_pos->is_printer_primed)
					{
						// We haven't primed yet, check to see if we have priming height restrictions
						if (utilities::greater_than(priming_height_, 0))
						{
							// if a priming height is configured, see if we've extruded below the  height
							if (utilities::less_than(p_current_pos->z, priming_height_))
								p_current_pos->is_printer_primed = true;
						}
						else
							// if we have no priming height set, just set is_printer_primed = true.
							p_current_pos->is_printer_primed = true;
					}

					if (p_current_pos->is_printer_primed && is_in_bounds)
					{
						// Update the last extrusion height
						p_current_pos->last_extrusion_height = p_current_pos->z;
						p_current_pos->last_extrusion_height_null = false;

						// Calculate current height
						if (utilities::greater_than_or_equal(p_current_pos->z, p_previous_pos->height + minimum_layer_height_))
						{
							p_current_pos->height = p_current_pos->z;
							p_current_pos->is_layer_change = true;
							p_current_pos->layer++;
						}
					}
				}

				// calculate is_zhop
				if (p_current_pos->is_extruding || p_current_pos->z_null || p_current_pos->last_extrusion_height_null)
					p_current_pos->is_zhop = false;
				else
					p_current_pos->is_zhop = utilities::greater_than_or_equal(p_current_pos->z - p_current_pos->last_extrusion_height, z_lift_height_);
			}

		}

		

	}
}

void gcode_position::undo_update()
{
	cur_pos_ = (cur_pos_ - 1 + NUM_POSITIONS) % NUM_POSITIONS;
}

// Private Members
std::map<std::string, gcode_position::pos_function_type> gcode_position::get_gcode_functions()
{
	std::map<std::string, pos_function_type> newMap;
	newMap.insert(std::make_pair("G0", &gcode_position::process_g0_g1));
	newMap.insert(std::make_pair("G1", &gcode_position::process_g0_g1));
	newMap.insert(std::make_pair("G2", &gcode_position::process_g2));
	newMap.insert(std::make_pair("G3", &gcode_position::process_g3));
	newMap.insert(std::make_pair("G10", &gcode_position::process_g10));
	newMap.insert(std::make_pair("G11", &gcode_position::process_g11));
	newMap.insert(std::make_pair("G20", &gcode_position::process_g20));
	newMap.insert(std::make_pair("G21", &gcode_position::process_g21));
	newMap.insert(std::make_pair("G28", &gcode_position::process_g28));
	newMap.insert(std::make_pair("G90", &gcode_position::process_g90));
	newMap.insert(std::make_pair("G91", &gcode_position::process_g91));
	newMap.insert(std::make_pair("G92", &gcode_position::process_g92));
	newMap.insert(std::make_pair("M82", &gcode_position::process_m82));
	newMap.insert(std::make_pair("M83", &gcode_position::process_m83));
	newMap.insert(std::make_pair("M207", &gcode_position::process_m207));
	newMap.insert(std::make_pair("M208", &gcode_position::process_m208));
	return newMap;
}

void gcode_position::update_position(
	position* pos, 
	const double x, 
	const bool update_x, 
	const double y, 
	const bool update_y, 
	const double z, 
	const bool update_z, 
	const double e, 
	const bool update_e, 
	const double f, 
	const bool update_f, 
	const bool force, 
	const bool is_g1_g0)
{
	if (is_g1_g0)
	{
		if (!update_e)
		{
			if (update_z)
			{
				pos->is_xyz_travel = (update_x || update_y);
			}
			else
			{
				pos->is_xy_travel = (update_x || update_y);
			}
		}

	}
	if (update_f)
	{
		pos->f = f;
		pos->f_null = false;
	}

	if (force)
	{
		if (update_x)
		{
			pos->x = x + pos->x_offset;
			pos->x_null = false;
		}
		if (update_y)
		{
			pos->y = y + pos->y_offset;
			pos->y_null = false;
		}
		if (update_z)
		{
			pos->z = z + pos->z_offset;
			pos->z_null = false;
		}
		// note that e cannot be null and starts at 0
		if (update_e)
			pos->e = e + pos->e_offset;
		return;
	}

	if (!pos->is_relative_null)
	{
		if (pos->is_relative) {
			if (update_x)
			{
				if (!pos->x_null)
					pos->x = x + pos->x;
				else
				{
					octolapse_log(octolapse_log::GCODE_POSITION, octolapse_log::ERROR, "GcodePosition.update_position: Cannot update X because the XYZ axis mode is relative and X is null.");
				}
			}
			if (update_y)
			{
				if (!pos->y_null)
					pos->y = y + pos->y;
				else
				{
					octolapse_log(octolapse_log::GCODE_POSITION, octolapse_log::ERROR, "GcodePosition.update_position: Cannot update Y because the XYZ axis mode is relative and Y is null.");
				}
			}
			if (update_z)
			{
				if (!pos->z_null)
					pos->z = z + pos->z;
				else
				{
					octolapse_log(octolapse_log::GCODE_POSITION, octolapse_log::ERROR, "GcodePosition.update_position: Cannot update Z because the XYZ axis mode is relative and Z is null.");
				}
			}
		}
		else
		{
			if (update_x)
			{
				pos->x = x + pos->x_offset;
				pos->x_null = false;
			}
			if (update_y)
			{
				pos->y = y + pos->y_offset;
				pos->y_null = false;
			}
			if (update_z)
			{
				pos->z = z + pos->z_offset;
				pos->z_null = false;
			}
		}
	}
	else
	{
		std::string message = "The XYZ axis mode is not set, cannot update position.";
		octolapse_log(octolapse_log::GCODE_POSITION, octolapse_log::ERROR, message);
	}

	if (update_e)
	{
		if (!pos->is_extruder_relative_null)
		{
			if (pos->is_extruder_relative)
			{
				pos->e = e + pos->e;
			}
			else
			{
				pos->e = e + pos->e_offset;
			}
		}
		else
		{
			std::string message = "The E axis mode is not set, cannot update position.";
			octolapse_log(octolapse_log::GCODE_POSITION, octolapse_log::ERROR, message);
		}
	}

}

void gcode_position::process_g0_g1(position* pos, parsed_command& cmd)
{
	bool update_x = false;
	bool update_y = false;
	bool update_z = false;
	bool update_e = false;
	bool update_f = false;
	double x = 0;
	double y = 0;
	double z = 0;
	double e = 0;
	double f = 0;
	for (unsigned int index = 0; index < cmd.parameters.size(); index++)
	{
		const parsed_command_parameter p_cur_param = cmd.parameters[index];
		if (p_cur_param.name == 'X')
		{
			update_x = true;
			x = p_cur_param.double_value;
		}
		else if (p_cur_param.name == 'Y')
		{
			update_y = true;
			y = p_cur_param.double_value;
		}
		else if (p_cur_param.name == 'E')
		{
			update_e = true;
			e = p_cur_param.double_value;
		}
		else if (p_cur_param.name == 'Z')
		{
			update_z = true;
			z = p_cur_param.double_value;
		}
		else if (p_cur_param.name == 'F')
		{
			update_f = true;
			f = p_cur_param.double_value;
		}
	}
	update_position(pos, x, update_x, y, update_y, z, update_z, e, update_e, f, update_f, false, true);
}

void gcode_position::process_g2(position* pos, parsed_command& cmd)
{
	// ToDo:  Fix G2
}

void gcode_position::process_g3(position* pos, parsed_command& cmd)
{
	// Todo: Fix G3
}

void gcode_position::process_g10(position* pos, parsed_command& cmd)
{
	// Todo: Fix G10
}

void gcode_position::process_g11(position* pos, parsed_command& cmd)
{
	// Todo: Fix G11
}

void gcode_position::process_g20(position* pos, parsed_command& cmd)
{

}

void gcode_position::process_g21(position* pos, parsed_command& cmd)
{

}

void gcode_position::process_g28(position* pos, parsed_command& cmd)
{
	bool has_x = false;
	bool has_y = false;
	bool has_z = false;
	bool set_x_home = false;
	bool set_y_home = false;
	bool set_z_home = false;

	for (unsigned int index = 0; index < cmd.parameters.size(); index++)
	{
		parsed_command_parameter p_cur_param = cmd.parameters[index];
		if (p_cur_param.name == 'X')
			has_x = true;
		else if (p_cur_param.name == 'Y')
			has_y = true;
		else if (p_cur_param.name == 'Z')
			has_z = true;
	}
	if (has_x)
	{
		pos->x_homed = true;
		set_x_home = true;
	}
	if (has_y)
	{
		pos->y_homed = true;
		set_y_home = true;
	}
	if (has_z)
	{
		pos->z_homed = true;
		set_z_home = true;
	}
	if (!has_x && !has_y && !has_z)
	{
		pos->x_homed = true;
		pos->y_homed = true;
		pos->z_homed = true;
		set_x_home = true;
		set_y_home = true;
		set_z_home = true;
	}

	if (set_x_home && !home_x_none_)
	{
		pos->x = home_x_;
		pos->x_null = false;
	}
	// todo: set error flag on else
	if (set_y_home && !home_y_none_)
	{
		pos->y = home_y_;
		pos->y_null = false;
	}
	// todo: set error flag on else
	if (set_z_home && !home_z_none_)
	{
		pos->z = home_z_;
		pos->z_null = false;
	}
	// todo: set error flag on else
}

void gcode_position::process_g90(position* pos, parsed_command& cmd)
{
	// Set xyz to absolute mode
	if (pos->is_relative_null)
		pos->is_relative_null = false;

	pos->is_relative = false;

	if (g90_influences_extruder_)
	{
		// If g90/g91 influences the extruder, set the extruder to absolute mode too
		if (pos->is_extruder_relative_null)
			pos->is_extruder_relative_null = false;

		pos->is_extruder_relative = false;
	}

}

void gcode_position::process_g91(position* pos, parsed_command& cmd)
{
	// Set XYZ axis to relative mode
	if (pos->is_relative_null)
		pos->is_relative_null = false;

	pos->is_relative = true;

	if (g90_influences_extruder_)
	{
		// If g90/g91 influences the extruder, set the extruder to relative mode too
		if (pos->is_extruder_relative_null)
			pos->is_extruder_relative_null = false;

		pos->is_extruder_relative = true;
	}
}

void gcode_position::process_g92(position* pos, parsed_command& cmd)
{
	// Set position offset
	bool update_x = false;
	bool update_y = false;
	bool update_z = false;
	bool update_e = false;
	bool o_exists = false;
	double x = 0;
	double y = 0;
	double z = 0;
	double e = 0;
	for (unsigned int index = 0; index < cmd.parameters.size(); index++)
	{
		parsed_command_parameter p_cur_param = cmd.parameters[index];
		char cmdName = p_cur_param.name;
		if (cmdName == 'X')
		{
			update_x = true;
			x = p_cur_param.double_value;
		}
		else if (cmdName == 'Y')
		{
			update_y = true;
			y = p_cur_param.double_value;
		}
		else if (cmdName == 'E')
		{
			update_e = true;
			e = p_cur_param.double_value;
		}
		else if (cmdName == 'Z')
		{
			update_z = true;
			z = p_cur_param.double_value;
		}
		else if (cmdName == 'O')
		{
			o_exists = true;
		}
	}

	if (o_exists)
	{
		// Our fake O parameter exists, set axis to homed!
		// This is a workaround to allow folks to use octolapse without homing (for shame, lol!)
		pos->x_homed = true;
		pos->y_homed = true;
		pos->z_homed = true;
	}

	if (!o_exists && !update_x && !update_y && !update_z && !update_e)
	{
		if (!pos->x_null)
			pos->x_offset = pos->x;
		if (!pos->y_null)
			pos->y_offset = pos->y;
		if (!pos->z_null)
			pos->z_offset = pos->z;
		// Todo:  Does this reset E too?  Figure that $#$$ out Formerlurker!
		pos->e_offset = pos->e;
	}
	else
	{
		if (update_x)
		{
			if (!pos->x_null && pos->x_homed)
				pos->x_offset = pos->x - x;
			else
			{
				pos->x = x;
				pos->x_offset = 0;
				pos->x_null = false;
			}
		}
		if (update_y)
		{
			if (!pos->y_null && pos->y_homed)
				pos->y_offset = pos->y - y;
			else
			{
				pos->y = y;
				pos->y_offset = 0;
				pos->y_null = false;
			}
		}
		if (update_z)
		{
			if (!pos->z_null && pos->z_homed)
				pos->z_offset = pos->z - z;
			else
			{
				pos->z = z;
				pos->z_offset = 0;
				pos->z_null = false;
			}
		}
		if (update_e)
		{
			pos->e_offset = pos->e - e;
		}
	}
}

void gcode_position::process_m82(position* pos, parsed_command& cmd)
{
	// Set extrder mode to absolute
	if (pos->is_extruder_relative_null)
		pos->is_extruder_relative_null = false;

	pos->is_extruder_relative = false;
}

void gcode_position::process_m83(position* pos, parsed_command& cmd)
{
	// Set extrder mode to relative
	if (pos->is_extruder_relative_null)
		pos->is_extruder_relative_null = false;

	pos->is_extruder_relative = true;
}

void gcode_position::process_m207(position* pos, parsed_command& cmd)
{
	// Todo: impemente firmware retract
}

void gcode_position::process_m208(position* pos, parsed_command& cmd)
{
	// Todo: implement firmware retract
}
