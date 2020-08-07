{
  "targets": [
    {
      "target_name": "ems",
      "sources": [
        "src/collectives.cc", "src/ems.cc", "src/ems_alloc.cc", "src/loops.cc",
        "nodejs/nodejs.cc", "src/primitives.cc", "src/rmw.cc"],
      'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")"],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'conditions': [
          ['OS=="linux"', {
            'libraries': ["-lrt"],
          }],
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}
