from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

examples_extension = Extension(
    name="copy_c",
    sources=["copy_c.pyx"],
    libraries=["copy_c"],
    library_dirs=[""],
    include_dirs=["./"]
)
setup(
    name="copy_c",
    ext_modules=cythonize([examples_extension])
)