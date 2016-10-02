from distutils.core import setup, Extension

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

    description='Extended Memory Semantics for Python',
    license='BSD',

    # The project's main homepage.
    url='https://github.com/SyntheticSemantics/ems',

    #   rm -f libems.so *.o; clang -dynamiclib -dynamiclib *.cc -o libems.so
    ext_modules=[Extension('libems.so',
                           ['src/collectives.cc', 'src/ems.cc', 'src/ems_alloc.cc',
                            'src/loops.cc', 'src/primitives.cc', 'src/rmw.cc'])],
    xextra_compile_args="-dynamiclib -o libems.so"


    """
    long_description='long_description',


    # What does your project relate to?
    keywords='sample setuptools development',

    # You can just specify the packages manually here if your project is
    # simple. Or you can use find_packages().
    packages=find_packages(exclude=['contrib', 'docs', 'tests']),

    # Alternatively, if you want to distribute just a my_module.py, uncomment
    # this:
    #   py_modules=["my_module"],

    # List run-time dependencies here.  These will be installed by pip when
    # your project is installed. For an analysis of "install_requires" vs pip's
    # requirements files see:
    # https://packaging.python.org/en/latest/requirements.html
    install_requires=['ffi'],

    # List additional groups of dependencies here (e.g. development
    # dependencies). You can install these using the following syntax,
    # for example:
    # $ pip install -e .[dev,test]
    extras_require={
        'dev': ['check-manifest'],
        'test': ['coverage'],
    },

    # If there are data files included in your packages that need to be
    # installed, specify them here.  If using Python 2.6 or less, then these
    # have to be included in MANIFEST.in as well.
    package_data={
        'sample': ['package_data.dat'],
    },

    # Although 'package_data' is the preferred approach, in some case you may
    # need to place data files outside of your packages. See:
    # http://docs.python.org/3.4/distutils/setupscript.html#installing-additional-files # noqa
    # In this case, 'data_file' will be installed into '<sys.prefix>/my_data'
    data_files=[('my_data', ['data/data_file'])],

    # To provide executable scripts, use entry points in preference to the
    # "scripts" keyword. Entry points provide cross-platform support and allow
    # pip to create the appropriate form of executable for the target platform.
    entry_points={
        'console_scripts': [
            'sample=sample:main',
        ],
    },
)
"""
)