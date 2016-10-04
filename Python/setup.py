from distutils.core import setup, Extension

import sys
if sys.platform == "linux" or sys.platform == "linux2":
    link_args = ["-lrt"]
elif sys.platform == "darwin":
    link_args = []
else:
    link_args = []


src_path = './src/'
setup(
    name="ems",
    version="1.4.0",
    py_modules=["ems"],
    setup_requires=["cffi>=1.0.0"],
    install_requires=["cffi>=1.0.0"],
    # cffi_modules=["EMS.py:ffi"],

    # Author details
    author='Jace A Mogill',
    author_email='mogill@synsem.com',

    description='Extended Memory Semantics (EMS) for Python',
    license='BSD',

    # The project's main homepage.
    url='https://github.com/SyntheticSemantics/ems',

    ext_modules=[Extension('libems.so',
                           [src_path + filename for filename in
                            ['collectives.cc', 'ems.cc', 'ems_alloc.cc', 'loops.cc', 'primitives.cc', 'rmw.cc']],
                           extra_link_args=link_args
                           ) ],
    long_description='Persistent Shared Memory and Parallel Programming Model',
    keywords=["non volatile memory",
              "NVM",
              "NVMe",
              "multithreading",
              "multithreaded",
              "parallel",
              "parallelism",
              "concurrency",
              "shared memory",
              "multicore",
              "manycore",
              "transactional memory",
              "TM",
              "persistent memory",
              "pmem",
              "Extended Memory Semantics",
              "EMS"]
)
