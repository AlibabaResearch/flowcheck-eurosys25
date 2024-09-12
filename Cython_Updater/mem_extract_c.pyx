# cython: language_level=3
# distutils: language = c++
from libcpp.vector cimport vector
from libcpp.string cimport string
from libc.stdlib cimport malloc,free
from libc.stdint cimport uintptr_t

cdef extern from "mem_extract.cpp":
    pass

cdef extern from "mem_extract.hpp":
    ctypedef signed char int8_t
    ctypedef unsigned char uint8_t
    ctypedef unsigned short uint16_t
    ctypedef unsigned int uint32_t
    ctypedef unsigned long uint64_t
    ctypedef float _Float32
    ctypedef double _Float64
    
    cdef cppclass Extractor:
        Extractor() except +
        char* get_data_ptr()

    # cdef bool get_shared_mem_ptr()
    # cdef char* get_data_ptr()
    cdef uint8_t* getT(uint64_t len)

#cdef union FloatIntUnion:
#    uint32_t as_uint
#    float as_float

# 用位操作做大小端转换，尽可能少地编译机器指令
#cdef inline uint32_t swap_endian(uint32_t value):
#    return ((value >> 24) & 0xff) | ((value >> 8) & 0xff00) | \
#           ((value << 8) & 0xff0000) | ((value << 24) & 0xff000000)

#cdef inline float read_float_le(char* addr):
#    cdef uint32_t val = (<uint32_t*>addr)[0]
#    val = swap_endian(val)
#    cdef FloatIntUnion u
#    u.as_uint = val
#    # return *((float*)&val)
#    return u.as_float

cdef class PyExtractor:
    cdef Extractor* thisptr  # 持有一个指向C++ Extractor类的指针

    def __cinit__(self):
        self.thisptr = new Extractor()  # 构造C++对象

    def __dealloc__(self):
        if self.thisptr:
            del self.thisptr  # 析构C++对象

    def getResT(self):
        cdef char* ptr
        if self.thisptr:
            ptr = self.thisptr.get_data_ptr()
            return <uintptr_t>ptr
        else:
            raise ValueError("Extractor ptr is not init.")
    
    def getTensor(self, uint64_t len):
        cdef uintptr_t addr = self.getResT()
        cdef _Float32* float_ptr = <_Float32*>addr
        cdef _Float32[:] p_view = <_Float32[:len]>float_ptr
        return p_view
        
        
        #cdef char* addr
        #cdef int i
        #addr = self.thisptr.get_data_ptr()
        #cdef float* float_array = <float*>malloc(len * sizeof(float))
        #for i in range(len):
        #    float_array[i] = read_float_le(<char*>(addr + i * sizeof(uint32_t)))
        #cdef _Float32[:] p_view = <_Float32[:len]>float_array
        #return p_view

    # 需要额外添加一个置零操作，在每次成功取出数据后清除共享内存中的所有内容等待下一iteration数据【是否有必要清除？直接覆写就行了】 【是需要的，这一操作由 extractor类完成】

