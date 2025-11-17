/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#pragma once
inline const char* defaultStyle =
    "name: \"hybrid\"\n\natom {\n	trapped {\n		color: "
    "#a2ad00\n	}\n	shuttling {\n		color: #005293\n	"
    "}\n	legend {\n		name {\n			"
    "^atom(.*)$: \"$1\"\n			^.*$: \"$0\"\n		"
    "}\n		font {\n			family: \"DejaVu "
    "Sans\"\n			size: 3\n			color: "
    "#000000\n		}\n	}\n	radius: 2\n}\n\nzone {\n	config "
    "^zone_cz.*$ {\n		color: #e37222\n		line "
    "{\n			thickness: 1\n			dash "
    "{\n				length: 0\n			"
    "	duty: 100%\n			}\n		}\n		name: "
    "\"CZ\"\n	}\n	config ^zone.*$ {\n		color: "
    "#64a0c8\n		line {\n			thickness: 1\n	"
    "		dash {\n				length: 0\n	"
    "			duty: 100%\n			}\n		"
    "}\n		name: \"\"\n	}\n	legend {\n		"
    "display: true\n		title: \"Zones\"\n	}\n}\n\noperation "
    "{\n	config {\n		ry {\n			color: "
    "#98c6ea\n			name: \"RY\"\n			radius: "
    "150%\n		}\n		rz {\n			color: "
    "#64a0c8\n			name: \"RZ\"\n			radius: "
    "150%\n		}\n		cz {\n			color: "
    "#e37222\n			name: \"CZ\"\n			radius: "
    "XXX\n		}\n	}\n	legend {\n		display: "
    "true\n		title: \"Gates\"\n	}\n}\n\nmachine {\n	trap "
    "{\n		color: #999999\n		radius: 2\n	"
    "	line_width: 0.5\n		name: \"SLM\"\n	}\n	shuttle "
    "{\n		color: #999999\n		line {\n	"
    "		thickness: 0.5\n			dash {\n	"
    "			length: 5\n				duty: "
    "50%\n			}\n		}\n		name: "
    "\"AOD\"\n	}\n	legend {\n		display: true\n		title: "
    "\"Atom States\"\n	}\n}\n\ncoordinate {\n	tick {\n		x: "
    "20\n		y: 20\n		color: #999999\n		line "
    "{\n			thickness: 0.0\n			dash "
    "{\n				length: 0\n			"
    "	duty: 100%\n			}\n		}\n	    display: "
    "true\n     }\n	number "
    "{\n		x {\n			distance: 40\n		"
    "	position: bottom\n		}\n		y {\n		"
    "	distance: 40\n			position: left\n		"
    "}\n		display: true\n		font {\n		"
    "	family: \"DajaVu Sans\"\n			size: 8\n	"
    "		color: #000000\n		}\n	}\n	axis "
    "{\n		x: \"x\"\n		y: \"y\"\n		"
    "display: true\n		font {\n			family: "
    "\"DajaVu Sans\"\n			size: 8\n			color: "
    "#000000\n		}\n	}\n	margin: 20\n}\n\nsidebar {\n	font "
    "{\n		family: \"DejaVu Sans\"\n		size: "
    "32\n		color: #000000\n	}\n	margin: 8\n	"
    "padding {\n		color: 16\n		heading: 52\n	"
    "	entry: 45\n	}\n	color_radius: 16\n}\n\ntime {\n	display: "
    "true\n	prefix: \"Time: \"\n	precision: 1\n	font {\n	"
    "	family: \"DejaVu Sans\"\n		size: 12\n		color: "
    "#000000\n	}\n}\n\nviewport {\n	margin: 4\n	color: #ffffff\n}\n";
