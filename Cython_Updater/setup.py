from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize
from Cython.Compiler.Options import get_directive_defaults
directive_defaults = get_directive_defaults()
directive_defaults['linetrace'] = True
directive_defaults['binding'] = True

import os
# os.environ['LDSHARED'] = 'g++ -shared'
os.environ['CC'] = 'clang'
os.environ['CXX'] = 'clang++'

examples_extension = Extension(
    name="mem_extract_c",
    sources=["mem_extract_c.pyx"],
    language="c++",
    libraries=["mem_extract_c"],
    library_dirs=[""],
    extra_compile_args=["-std=c++11", "-O3","-mavx512f","-mavx512bw"],
    include_dirs=["./"],
    define_macros=[('CYTHON_TRACE', '1')]
)
setup(
    name="mem_extract_c",

    ext_modules=cythonize([examples_extension])
)