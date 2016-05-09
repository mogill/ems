{
  "targets": [
    {
      "target_name": "ems",
      "sources": [
        "src/collectives.cc", "src/ems.cc", "src/ems_alloc.cc", "src/loops.cc",
        "nodejs/nodejs.cc", "src/primitives.cc", "src/rmw.cc"],
      "include_dirs" : [ "<!(node -e \"require('nan')\")" ]
    }
  ]
}
