#// -*- mode: espresso;-*-
{
    "targets": [
	{
            "target_name": "ems",
            "sources": ["collectives.cc", "ems.cc", "ems_alloc.cc", "loops.cc", "nodejs.cc", "primitives.cc", "rmw.cc"],
            "include_dirs" : [ "<!(node -e \"require('nan')\")" ]
        },
    ]
}
