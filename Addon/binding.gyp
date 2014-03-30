#// -*- mode: espresso;-*-
{
    "targets": [
	{
            "target_name": "EMS",
            "sources": [ "ems.cpp", "ems_alloc.cpp" ],

	    'xcode_settings': {	'OTHER_CFLAGS': [ '-Wunused-variable' ],
				'CFLAGS': [ '-Wunused-variable' ],
				'cflags': [ '-Wunused-variable' ],
			      },

	    'OTHER_CFLAGS': ['-Wunused-variable'],

	    'CFLAGS': ['-Wunused-variable'],
	    'cflags': ['-Wunused-variable'],

#	    'conditions': [
#		[ 'OS=="mac"', 
#		  { 'cflags': [ '-Wunused-variable' ] }, 
#		  { 'cflags': [ '-Wunused-variable' ] } ]
#	    ]
	}
  ]
}
