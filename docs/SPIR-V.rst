=====================================
HLSL to SPIR-V Feature Mapping Manual
=====================================

.. contents::
   :local:
   :depth: 3

Introduction
============

This document describes the mappings from HLSL features to SPIR-V for Vulkan
adopted by the SPIR-V codegen. For how to build, use, or contribute to the
SPIR-V codegen and its internals, please see the
`wiki <https://github.com/Microsoft/DirectXShaderCompiler/wiki/SPIR%E2%80%90V-CodeGen>`_
page.

`SPIR-V <https://www.khronos.org/registry/spir-v/>`_ is a binary intermediate
language for representing graphical-shader stages and compute kernels for
multiple Khronos APIs, such as Vulkan, OpenGL, and OpenCL. At the moment we
only intend to support the Vulkan flavor of SPIR-V.

DirectXShaderCompiler is the reference compiler for HLSL. Adding SPIR-V codegen
in DirectXShaderCompiler will enable the usage of HLSL as a frontend language
for Vulkan shader programming. Sharing the same code base also means we can
track the evolution of HLSL more closely and always deliver the best of HLSL to
developers. Moreover, developers will also have a unified compiler toolchain for
targeting both DirectX and Vulkan. We believe this effort will benefit the
general graphics ecosystem.

Note that this document is expected to be an ongoing effort and grow as we
implement more and more HLSL features.

Overview
========

Although they share the same basic concepts, DirectX and Vulkan are still
different graphics APIs with semantic gaps. HLSL is the native shading language
for DirectX, so certain HLSL features do not have corresponding mappings in
Vulkan, and certain Vulkan specific information does not have native ways to
express in HLSL source code. This section describes the general translation
paradigms and how we close some of the major semantic gaps.

Note that the term "semantic" is overloaded. In HLSL, it can mean the string
attached to shader input or output. For such cases, we refer it as "HLSL
semantic" or "semantic string". For other cases, we just use the normal
"semantic" term.

Shader entry function
---------------------

HLSL entry functions can read data from the previous shader stage and write
data to the next shader stage via function parameters and return value. On the
contrary, Vulkan requires all SPIR-V entry functions taking no parameters and
returning void. All data passing between stages should use global variables
in the ``Input`` and ``Output`` storage class.

To handle this difference, we emit a wrapper function as the SPIR-V entry
function around the HLSL source code entry function. The wrapper function is
responsible to read data from SPIR-V ``Input`` global variables and prepare
them to the types required in the source code entry function signature, call
the source code entry function, and then decompose the contents in return value
(and ``out``/``inout`` parameters) to the types required by the SPIR-V
``Output`` global variables, and then write out. For details about the wrapper
function, please refer to the `entry function wrapper`_ section.

Shader stage IO interface matching
----------------------------------

HLSL leverages semantic strings to link variables and pass data between shader
stages. Great flexibility is allowed as for how to use the semantic strings.
They can appear on function parameters, function returns, and struct members.
In Vulkan, linking variables and passing data between shader stages is done via
numeric ``Location`` decorations on SPIR-V global variables in the ``Input`` and
``Output`` storage class.

To help handling such differences, we provide `Vulkan specific attributes`_ to
let the developer to express precisely their intents. The compiler will also try
its best to deduce the mapping from semantic strings to SPIR-V ``Location``
numbers when such explicit Vulkan specific attributes are absent. Please see the
`HLSL semantic and Vulkan Location`_ section for more details about the mapping
and ``Location`` assignment.

What makes the story complicated is Vulkan's strict requirements on interface
matching. Basically, a variable in the previous stage is considered a match to
a variable in the next stage if and only if they are decorated with the same
``Location`` number and with the exact same type, except for the outermost
arrayness in hull/domain/geometry shader, which can be ignored regarding
interface matching. This is causing problems together with the flexibility of
HLSL semantic strings.

Some HLSL system-value (SV) semantic strings will be mapped into SPIR-V
variables with builtin decorations, some are not. HLSL non-SV semantic strings
should all be mapped to SPIR-V variables without builtin decorations (but with
``Location`` decorations).

With these complications, if we are grouping multiple semantic strings in a
struct in the HLSL source code, that struct should be flattened and each of
its members should be mapped separately. For example, for the following:

.. code:: hlsl

  struct T {
    float2 clip0 : SV_ClipDistance0;
    float3 cull0 : SV_CullDistance0;
    float4 foo   : FOO;
  };

  struct S {
    float4 pos   : SV_Position;
    float2 clip1 : SV_ClipDistance1;
    float3 cull1 : SV_CullDistance1;
    float4 bar   : BAR;
    T      t;
  };

If we have an ``S`` input parameter in pixel shader, we should flatten it
recursively to generate five SPIR-V ``Input`` variables. Three of them are
decorated by the ``Position``, ``ClipDistance``, ``CullDistance`` builtin,
and two of them are decorated by the ``Location`` decoration. (Note that
``clip0`` and ``clip1`` are concatenated, also ``cull0`` and ``cull1``.
The ``ClipDistance`` and ``CullDistance`` builtins are special and explained
in the `gl_PerVertex`_ section.)

Flattening is infective because of Vulkan interface matching rules. If we
flatten a struct in the output of a previous stage, which may create multiple
variables decorated with different ``Location`` numbers, we also need to
flatten it in the input of the next stage. otherwise we may have ``Location``
mismatch even if we share the same definition of the struct. Because
hull/domain/geometry shader is optional, we can have different chains of shader
stages, which means we need to flatten all shader stage interfaces. For
hull/domain/geometry shader, their inputs/outputs have an additional arrayness.
So if we are seeing an array of structs in these shaders, we need to flatten
them into arrays of its fields.

Lastly, to satisfy the type requirements on builtins, after flattening, the
variables decorated with ``Position``, ``ClipDistance``, and ``CullDistance``
builtins are grouped into struct, like ``gl_PerVertex`` for certain shader stage
interface:

============ ===== ======
Shader Stage Input Output
============ ===== ======
    VS         X     G
    HS         G     G
    DS         G     G
    GS         G     S
    PS         S     X
============ ===== ======

(``X``: Not applicable, ``G``: Grouped, ``S``: separated)

More details in the `gl_PerVertex`_ section.

Vulkan specific features
------------------------

We try to implement Vulkan specific features using the most intuitive and
non-intrusive ways in HLSL, which means we will prefer native language
constructs when possible. If that is inadequate, we then consider attaching
`Vulkan specific attributes`_ to them, or introducing new syntax.

Descriptors
~~~~~~~~~~~

To specify which Vulkan descriptor a particular resource binds to, use the
``[[vk::binding(X[, Y])]]`` attribute.

Push constants
~~~~~~~~~~~~~~

Vulkan push constant blocks are represented using normal global variables of
struct types in HLSL. The variables (not the underlying struct types) should be
annotated with the ``[[vk::push_constant]]`` attribute.

Please note as per the requirements of Vulkan, "there must be no more than one
push constant block statically used per shader entry point."

Builtin variables
~~~~~~~~~~~~~~~~~

Some of the Vulkan builtin variables have no equivalents in native HLSL
language. To support them, ``[[vk::builtin("<builtin>")]]`` is introduced.
Right now only two ``<builtin>`` are supported:

* ``PointSize``: The GLSL equivalent is ``gl_PointSize``.
* ``HelperInvocation``: The GLSL equivalent is ``gl_HelperInvocation``.

Please see Vulkan spec. `14.6. Built-In Variables <https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#interfaces-builtin-variables>`_
for detailed explanation of these builtins.

Vulkan specific attributes
--------------------------

`C++ attribute specifier sequence <http://en.cppreference.com/w/cpp/language/attributes>`_
is a non-intrusive way of providing Vulkan specific information in HLSL.

The namespace ``vk`` will be used for all Vulkan attributes:

- ``location(X)``: For specifying the location (``X``) numbers for stage
  input/output variables. Allowed on function parameters, function returns,
  and struct fields.
- ``binding(X[, Y])``: For specifying the descriptor set (``Y``) and binding
  (``X``) numbers for resource variables. The descriptor set (``Y``) is
  optional; if missing, it will be set to 0. Allowed on global variables.
- ``counter_binding(X)``: For specifying the binding number (``X``) for the
  associated counter for RW/Append/Consume structured buffer. The descriptor
  set number for the associated counter is always the same as the main resource.
- ``push_constant``: For marking a variable as the push constant block. Allowed
  on global variables of struct type. At most one variable can be marked as
  ``push_constant`` in a shader.
- ``builtin("X")``: For specifying an entity should be translated into a certain
  Vulkan builtin variable. Allowed on function parameters, function returns,
  and struct fields.

Only ``vk::`` attributes in the above list are supported. Other attributes will
result in warnings and be ignored by the compiler. All C++11 attributes will
only trigger warnings and be ignored if not compiling towards SPIR-V.

For example, to specify the layout of resource variables and the location of
interface variables:

.. code:: hlsl

  struct S { ... };

  [[vk::binding(X, Y), vk::counter_binding(Z)]]
  RWStructuredBuffer<S> mySBuffer;

  [[vk::location(M)]] float4
  main([[vk::location(N)]] float4 input: A) : B
  { ... }

HLSL Types
==========

This section lists how various HLSL types are mapped.

Normal scalar types
-------------------

`Normal scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_
in HLSL are relatively easy to handle and can be mapped directly to SPIR-V
type instructions:

================== ================== =========== ====================
      HLSL               SPIR-V       Capability       Decoration
================== ================== =========== ====================
``bool``           ``OpTypeBool``
``int``            ``OpTypeInt 32 1``
``uint``/``dword`` ``OpTypeInt 32 0``
``half``           ``OpTypeFloat 32``             ``RelexedPrecision``
``float``          ``OpTypeFloat 32``
``snorm float``    ``OpTypeFloat 32``
``unorm float``    ``OpTypeFloat 32``
``double``         ``OpTypeFloat 64`` ``Float64``
================== ================== =========== ====================

Please note that ``half`` is translated into 32-bit floating point numbers
right now because MSDN says that "this data type is provided only for language
compatibility. Direct3D 10 shader targets map all ``half`` data types to
``float`` data types." This may change in the future to map to 16-bit floating
point numbers (possibly via a command-line option).

Minimal precision scalar types
------------------------------

HLSL also supports various
`minimal precision scalar types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509646(v=vs.85).aspx>`_,
which graphics drivers can implement by using any precision greater than or
equal to their specified bit precision.
There are no direct mappings in SPIR-V for these types. We translate them into
the corresponding 32-bit scalar types with the ``RelexedPrecision`` decoration:

============== ================== ====================
    HLSL            SPIR-V            Decoration
============== ================== ====================
``min16float`` ``OpTypeFloat 32`` ``RelexedPrecision``
``min10float`` ``OpTypeFloat 32`` ``RelexedPrecision``
``min16int``   ``OpTypeInt 32 1`` ``RelexedPrecision``
``min12int``   ``OpTypeInt 32 1`` ``RelexedPrecision``
``min16uint``  ``OpTypeInt 32 0`` ``RelexedPrecision``
============== ================== ====================

Vectors and matrices
--------------------

`Vectors <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509707(v=vs.85).aspx>`_
and `matrices <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509623(v=vs.85).aspx>`_
are translated into:

==================================== ====================================================
              HLSL                                         SPIR-V
==================================== ====================================================
``|type|N`` (``N`` > 1)              ``OpTypeVector |type| N``
``|type|1``                          The scalar type for ``|type|``
``|type|MxN`` (``M`` > 1, ``N`` > 1) ``%v = OpTypeVector |type| N`` ``OpTypeMatrix %v M``
``|type|Mx1`` (``M`` > 1)            ``OpTypeVector |type| M``
``|type|1xN`` (``N`` > 1)            ``OpTypeVector |type| N``
``|type|1x1``                        The scalar type for ``|type|``
==================================== ====================================================

A MxN HLSL matrix is translated into a SPIR-V matrix with M vectors, each with
N elements. Conceptually HLSL matrices are row-major while SPIR-V matrices are
column-major, thus all HLSL matrices are represented by their transposes.
Doing so may require special handling of certain matrix operations:

- **Indexing**: no special handling required. ``matrix[m][n]`` will still access
  the correct element since ``m``/``n`` means the ``m``-th/``n``-th row/column
  in HLSL but ``m``-th/``n``-th vector/element in SPIR-V.
- **Per-element operation**: no special handling required.
- **Matrix multiplication**: need to swap the operands. ``mat1 x mat2`` should
  be translated as ``transpose(mat2) x transpose(mat1)``. Then the result is
  ``transpose(mat1 x mat2)``.
- **Storage layout**: ``row_major``/``column_major`` will be translated into
  SPIR-V ``ColMajor``/``RowMajor`` decoration. This is because HLSL matrix
  row/column becomes SPIR-V matrix column/row. If elements in a row/column are
  packed together, they should be loaded into a column/row correspondingly.

Structs
-------

`Structs <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509668(v=vs.85).aspx>`_
in HLSL are defined in the a format similar to C structs. They are translated
into SPIR-V ``OpTypeStruct``. Depending on the storage classes of the instances,
a single struct definition may generate multiple ``OpTypeStruct`` instructions
in SPIR-V. For example, for the following HLSL source code:

.. code:: hlsl

  struct S { ... }

  ConstantBuffer<S>   myCBuffer;
  StructuredBuffer<S> mySBuffer;

  float4 main() : A {
    S myLocalVar;
    ...
  }

There will be three different ``OpTypeStruct`` generated, one for each variable
defined in the above source code. This is because the ``OpTypeStruct`` for
both ``myCBuffer`` and ``mySBuffer`` will have layout decorations (``Offset``,
``MatrixStride``, ``ArrayStride``, ``RowMajor``, ``ColMajor``). However, their
layout rules are different (by default); ``myCBuffer`` will use GLSL ``std140``
while ``mySBuffer`` will use GLSL ``std430``. ``myLocalVar`` will have its
``OpTypeStruct`` without layout decorations. Read more about storage classes
in the `Buffers`_ section.

Structs used as stage inputs/outputs will have semantics attached to their
members. These semantics are handled in the `entry function wrapper`_.

Structs used as pixel shader inputs can have optional interpolation modifiers
for their members, which will be translated according to the following table:

=========================== ================= =====================
HLSL Interpolation Modifier SPIR-V Decoration   SPIR-V Capability
=========================== ================= =====================
``linear``                  <none>
``centroid``                ``Centroid``
``nointerpolation``         ``Flat``
``noperspective``           ``NoPerspective``
``sample``                  ``Sample``        ``SampleRateShading``
=========================== ================= =====================

User-defined types
------------------

`User-defined types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509702(v=vs.85).aspx>`_
are type aliases introduced by typedef. No new types are introduced and we can
rely on Clang to resolve to the original types.

Samplers
--------

All `sampler types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509644(v=vs.85).aspx>`_
will be translated into SPIR-V ``OpTypeSampler``.

SPIR-V ``OpTypeSampler`` is an opaque type that cannot be parameterized;
therefore state assignments on sampler types is not supported (yet).

Textures
--------

`Texture types <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509700(v=vs.85).aspx>`_
are translated into SPIR-V ``OpTypeImage``, with parameters:

======================= ========== ===== ======= == ======= ================ =================
HLSL Texture Type           Dim    Depth Arrayed MS Sampled  Image Format       Capability
======================= ========== ===== ======= == ======= ================ =================
``Texture1D``           ``1D``      0       0    0    1     ``Unknown``
``Texture2D``           ``2D``      0       0    0    1     ``Unknown``
``Texture3D``           ``3D``      0       0    0    1     ``Unknown``
``TextureCube``         ``Cube``    0       0    0    1     ``Unknown``
``Texture1DArray``      ``1D``      0       1    0    1     ``Unknown``
``Texture2DArray``      ``2D``      0       1    0    1     ``Unknown``
``Texture2DMS``         ``2D``      0       0    1    1     ``Unknown``
``Texture2DMSArray``    ``2D``      0       1    1    1     ``Unknown``      ``ImageMSArray``
``TextureCubeArray``    ``3D``      0       1    0    1     ``Unknown``
``Buffer<T>``           ``Buffer``  0       0    0    1     Depends on ``T`` ``SampledBuffer``
``RWBuffer<T>``         ``Buffer``  0       0    0    2     Depends on ``T`` ``SampledBuffer``
``RWTexture1D<T>``      ``1D``      0       0    0    2     Depends on ``T``
``RWTexture2D<T>``      ``2D``      0       0    0    2     Depends on ``T``
``RWTexture3D<T>``      ``3D``      0       0    0    2     Depends on ``T``
``RWTexture1DArray<T>`` ``1D``      0       1    0    2     Depends on ``T``
``RWTexture2DArray<T>`` ``2D``      0       1    0    2     Depends on ``T``
======================= ========== ===== ======= == ======= ================ =================

The meanings of the headers in the above table is explained in ``OpTypeImage``
of the SPIR-V spec.

Buffers
-------

There are serveral buffer types in HLSL:

- ``cbuffer`` and ``ConstantBuffer``
- ``tbuffer`` and ``TextureBuffer``
- ``StructuredBuffer`` and ``RWStructuredBuffer``
- ``AppendStructuredBuffer`` and ``ConsumeStructuredBuffer``
- ``ByteAddressBuffer`` and ``RWByteAddressBuffer``

Note that ``Buffer`` and ``RWBuffer`` are considered as texture object in HLSL.
They are listed in the above section.

Please see the following sections for the details of each type. As a summary:

=========================== ================== ========================== ==================== =================
         HLSL Type          Vulkan Buffer Type Default Memory Layout Rule SPIR-V Storage Class SPIR-V Decoration
=========================== ================== ========================== ==================== =================
``cbuffer``                   Uniform Buffer      GLSL ``std140``            ``Uniform``        ``Block``
``ConstantBuffer``            Uniform Buffer      GLSL ``std140``            ``Uniform``        ``Block``
``tbuffer``                   Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``TextureBuffer``             Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``StructuredBuffer``          Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``RWStructuredBuffer``        Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``AppendStructuredBuffer``    Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``ConsumeStructuredBuffer``   Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``ByteAddressBuffer``         Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
``RWByteAddressBuffer``       Storage Buffer      GLSL ``std430``            ``Uniform``        ``BufferBlock``
=========================== ================== ========================== ==================== =================

To know more about the Vulkan buffer types, please refer to the Vulkan spec
`13.1 Descriptor Types <https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#descriptorsets-types>`_.

``cbuffer`` and ``ConstantBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These two buffer types are treated as uniform buffers using Vulkan's
terminology. They are translated into an ``OpTypeStruct`` with the
necessary layout decorations (``Offset``, ``ArrayStride``, ``MatrixStride``,
``RowMajor``, ``ColMajor``) and the ``Block`` decoration. The layout rule
used is GLSL ``std140`` (by default). A variable declared as one of these
types will be placed in the ``Uniform`` storage class.

For example, for the following HLSL source code:

.. code:: hlsl

  struct T {
    float  a;
    float3 b;
  };

  ConstantBuffer<T> myCBuffer;

will be translated into

.. code:: spirv

  ; Layout decoration
  OpMemberDecorate %type_ConstantBuffer_T 0 Offset 0
  OpMemberDecorate %type_ConstantBuffer_T 0 Offset 16
  ; Block decoration
  OpDecorate %type_ConstantBuffer_T Block

  ; Types
  %type_ConstantBuffer_T = OpTypeStruct %float %v3float
  %_ptr_Uniform_type_ConstantBuffer_T = OpTypePointer Uniform %type_ConstantBuffer_T

  ; Variable
  %myCbuffer = OpVariable %_ptr_Uniform_type_ConstantBuffer_T Uniform

``tbuffer`` and ``TextureBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These two buffer types are treated as storage buffers using Vulkan's
terminology. They are translated into an ``OpTypeStruct`` with the
necessary layout decorations (``Offset``, ``ArrayStride``, ``MatrixStride``,
``RowMajor``, ``ColMajor``) and the ``BufferBlock`` decoration. All the struct
members are also decorated with ``NonWritable`` decoration. The layout rule
used is GLSL ``std430`` (by default). A variable declared as one of these
types will be placed in the ``Uniform`` storage class.


``StructuredBuffer`` and ``RWStructuredBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``StructuredBuffer<T>``/``RWStructuredBuffer<T>`` is treated as storage buffer
using Vulkan's terminology. It is translated into an ``OpTypeStruct`` containing
an ``OpTypeRuntimeArray`` of type ``T``, with necessary layout decorations
(``Offset``, ``ArrayStride``, ``MatrixStride``, ``RowMajor``, ``ColMajor``) and
the ``BufferBlock`` decoration.  The default layout rule used is GLSL
``std430``. A variable declared as one of these types will be placed in the
``Uniform`` storage class.

For ``RWStructuredBuffer<T>``, each variable will have an associated counter
variable generated. The counter variable will be of ``OpTypeStruct`` type, which
only contains a 32-bit integer. The counter variable takes its own binding
number. ``.IncrementCounter()``/``.DecrementCounter()`` will modify this counter
variable.

For example, for the following HLSL source code:

.. code:: hlsl

  struct T {
    float  a;
    float3 b;
  };

  StructuredBuffer<T> mySBuffer;

will be translated into

.. code:: spirv

  ; Layout decoration
  OpMemberDecorate %T 0 Offset 0
  OpMemberDecorate %T 1 Offset 16
  OpDecorate %_runtimearr_T ArrayStride 32
  OpMemberDecorate %type_StructuredBuffer_T 0 Offset 0
  OpMemberDecorate %type_StructuredBuffer_T 0 NoWritable
  ; BufferBlock decoration
  OpDecorate %type_StructuredBuffer_T BufferBlock

  ; Types
  %T = OpTypeStruct %float %v3float
  %_runtimearr_T = OpTypeRuntimeArray %T
  %type_StructuredBuffer_T = OpTypeStruct %_runtimearr_T
  %_ptr_Uniform_type_StructuredBuffer_T = OpTypePointer Uniform %type_StructuredBuffer_T

  ; Variable
  %myCbuffer = OpVariable %_ptr_Uniform_type_ConstantBuffer_T Uniform

``AppendStructuredBuffer`` and ``ConsumeStructuredBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``AppendStructuredBuffer<T>``/``ConsumeStructuredBuffer<T>`` is treated as
storage buffer using Vulkan's terminology. It is translated into an
``OpTypeStruct`` containing an ``OpTypeRuntimeArray`` of type ``T``, with
necessary layout decorations (``Offset``, ``ArrayStride``, ``MatrixStride``,
``RowMajor``, ``ColMajor``) and the ``BufferBlock`` decoration. The default
layout rule used is GLSL ``std430``.

A variable declared as one of these types will be placed in the ``Uniform``
storage class. Besides, each variable will have an associated counter variable
generated. The counter variable will be of ``OpTypeStruct`` type, which only
contains a 32-bit integer. The integer is the total number of elements in the
buffer. The counter variable takes its own binding number.
``.Append()``/``.Consume()`` will use the counter variable as the index and
adjust it accordingly.

For example, for the following HLSL source code:

.. code:: hlsl

  struct T {
    float  a;
    float3 b;
  };

  AppendStructuredBuffer<T> mySBuffer;

will be translated into

.. code:: spirv

  ; Layout decorations
  OpMemberDecorate %T 0 Offset 0
  OpMemberDecorate %T 1 Offset 16
  OpDecorate %_runtimearr_T ArrayStride 32
  OpMemberDecorate %type_AppendStructuredBuffer_T 0 Offset 0
  OpDecorate %type_AppendStructuredBuffer_T BufferBlock
  OpMemberDecorate %type_ACSBuffer_counter 0 Offset 0
  OpDecorate %type_ACSBuffer_counter BufferBlock

  ; Binding numbers
  OpDecorate %myASbuffer DescriptorSet 0
  OpDecorate %myASbuffer Binding 0
  OpDecorate %counter_var_myASbuffer DescriptorSet 0
  OpDecorate %counter_var_myASbuffer Binding 1

  ; Types
  %T = OpTypeStruct %float %v3float
  %_runtimearr_T = OpTypeRuntimeArray %T
  %type_AppendStructuredBuffer_T = OpTypeStruct %_runtimearr_T
  %_ptr_Uniform_type_AppendStructuredBuffer_T = OpTypePointer Uniform %type_AppendStructuredBuffer_T
  %type_ACSBuffer_counter = OpTypeStruct %int
  %_ptr_Uniform_type_ACSBuffer_counter = OpTypePointer Uniform %type_ACSBuffer_counter

  ; Variables
  %myASbuffer = OpVariable %_ptr_Uniform_type_AppendStructuredBuffer_T Uniform
  %counter_var_myASbuffer = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform

``ByteAddressBuffer`` and ``RWByteAddressBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``ByteAddressBuffer``/``RWByteAddressBuffer`` is treated as storage buffer using
Vulkan's terminology. It is translated into an ``OpTypeStruct`` containing an
``OpTypeRuntimeArray`` of 32-bit unsigned integers, with ``BufferBlock``
decoration.

A variable declared as one of these types will be placed in the ``Uniform``
storage class.

For example, for the following HLSL source code:

.. code:: hlsl

  ByteAddressBuffer   myBuffer1;
  RWByteAddressBuffer myBuffer2;

will be translated into

.. code:: spirv

  ; Layout decorations

  OpDecorate %_runtimearr_uint ArrayStride 4

  OpDecorate %type_ByteAddressBuffer BufferBlock
  OpMemberDecorate %type_ByteAddressBuffer 0 Offset 0
  OpMemberDecorate %type_ByteAddressBuffer 0 NonWritable

  OpDecorate %type_RWByteAddressBuffer BufferBlock
  OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0

  ; Types

  %_runtimearr_uint = OpTypeRuntimeArray %uint

  %type_ByteAddressBuffer = OpTypeStruct %_runtimearr_uint
  %_ptr_Uniform_type_ByteAddressBuffer = OpTypePointer Uniform %type_ByteAddressBuffer

  %type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
  %_ptr_Uniform_type_RWByteAddressBuffer = OpTypePointer Uniform %type_RWByteAddressBuffer

  ; Variables

  %myBuffer1 = OpVariable %_ptr_Uniform_type_ByteAddressBuffer Uniform
  %myBuffer2 = OpVariable %_ptr_Uniform_type_RWByteAddressBuffer Uniform

HLSL Variables and Resources
============================

This section lists how various HLSL variables and resources are mapped.

Storage class
-------------

Normal local variables (without any modifier) will be placed in the ``Function``
SPIR-V storage class. Normal global variables (without any modifer) will be
placed in the ``Uniform`` or ``UniformConstant`` storage class.

- ``static``

  - Global variables with ``static`` modifier will be placed in the ``Private``
    SPIR-V storage class. Initalizers of such global variables will be translated
    into SPIR-V ``OpVariable`` initializers if possible; otherwise, they will be
    initialized at the very beginning of the `entry function wrapper`_ using
    SPIR-V ``OpStore``.
  - Local variables with ``static`` modifier will also be placed in the
    ``Private`` SPIR-V storage class. initializers of such local variables will
    also be translated into SPIR-V ``OpVariable`` initializers if possible;
    otherwise, they will be initialized at the very beginning of the enclosing
    function. To make sure that such a local variable is only initialized once,
    a second boolean variable of the ``Private`` SPIR-V storage class will be
    generated to mark its initialization status.

- ``groupshared``

  - Global variables with ``groupshared`` modifier will be placed in the
    ``Workgroup`` storage class.

- ``uinform``

  - This does not affect codegen. Variables will be treated like normal global
    variables.

- ``extern``

  - This does not affect codegen. Variables will be treated like normal global
    variables.

- ``shared``

  - This is a hint to the compiler. It will be ingored.

- ``volatile``

  - This is a hint to the compiler. It will be ingored.

HLSL semantic and Vulkan ``Location``
-------------------------------------

Direct3D uses HLSL "`semantics <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509647(v=vs.85).aspx>`_"
to compose and match the interfaces between subsequent stages. These semantic
strings can appear after struct members, function parameters and return
values. E.g.,

.. code:: hlsl

  struct VSInput {
    float4 pos  : POSITION;
    float3 norm : NORMAL;
  };

  float4 VSMain(in  VSInput input,
                in  float4  tex   : TEXCOORD,
                out float4  pos   : SV_Position) : TEXCOORD {
    pos = input.pos;
    return tex;
  }

In contrary, Vulkan stage input and output interface matching is via explicit
``Location`` numbers. Details can be found `here <https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#interfaces-iointerfaces>`_.

To translate HLSL to SPIR-V for Vulkan, semantic strings need to be mapped to
Vulkan ``Location`` numbers properly. This can be done either explicitly via
information provided by the developer or implicitly by the compiler.

Explicit ``Location`` number assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``[[vk::location(X)]]`` can be attached to the entities where semantic are
allowed to attach (struct fields, function parameters, and function returns).
For the above exmaple we can have:

.. code:: hlsl

  struct VSInput {
    [[vk::location(0)]] float4 pos  : POSITION;
    [[vk::location(1)]] float3 norm : NORMAL;
  };

  [[vk::location(1)]]
  float4 VSMain(in  VSInput input,
                [[vk::location(2)]]
                in  float4  tex     : TEXCOORD,
                out float4  pos     : SV_Position) : TEXCOORD {
    pos = input.pos;
    return tex;
  }

In the above, input ``POSITION``, ``NORMAL``, and ``TEXCOORD`` will be mapped to
``Location`` 0, 1, and 2, respectively, and output ``TEXCOORD`` will be mapped
to ``Location`` 1.

[TODO] Another explicit way: using command-line options

Please note that the compiler prohibits mixing the explicit and implicit
approach for the same SigPoint to avoid complexity and fallibility. However,
for a certain shader stage, one SigPoint using the explicit approach while the
other adopting the implicit approach is permitted.

Implicit ``Location`` number assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Without hints from the developer, the compiler will try its best to map
semantics to ``Location`` numbers. However, there is no single rule for this
mapping; semantic strings should be handled case by case.

Firstly, under certain `SigPoints <https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/DXIL.rst#hlsl-signatures-and-semantics>`_,
some system-value (SV) semantic strings will be translated into SPIR-V
``BuiltIn`` decorations:

.. table:: Mapping from HLSL SV semantic to SPIR-V builtin and execution mode

+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| HLSL Semantic             | SigPoint    | SPIR-V ``BuiltIn``       | SPIR-V Execution Mode |   SPIR-V Capability   |
+===========================+=============+==========================+=======================+=======================+
|                           | VSOut       | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPIn      | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPOut     | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSCPIn      | ``Position``             | N/A                   | ``Shader``            |
| SV_Position               +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSOut       | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSVIn       | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``Position``             | N/A                   | ``Shader``            |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``FragCoord``            | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | VSOut       | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPIn      | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPOut     | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSCPIn      | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
| SV_ClipDistance           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSOut       | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSVIn       | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``ClipDistance``         | N/A                   | ``ClipDistance``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | VSOut       | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPIn      | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSCPOut     | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSCPIn      | ``CullDistance``         | N/A                   | ``CullDistance``      |
| SV_CullDistance           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSOut       | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSVIn       | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``CullDistance``         | N/A                   | ``CullDistance``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``CullDistance``         | N/A                   | ``CullDistance``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_VertexID               | VSIn        | ``VertexIndex``          | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_InstanceID             | VSIn        | ``InstanceIndex``        | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_Depth                  | PSOut       | ``FragDepth``            | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_DepthGreaterEqual      | PSOut       | ``FragDepth``            | ``DepthGreater``      | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_DepthLessEqual         | PSOut       | ``FragDepth``            | ``DepthLess``         | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_IsFrontFace            | PSIn        | ``FrontFacing``          | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_DispatchThreadID       | CSIn        | ``GlobalInvocationId``   | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_GroupID                | CSIn        | ``WorkgroupId``          | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_GroupThreadID          | CSIn        | ``LocalInvocationId``    | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_GroupIndex             | CSIn        | ``LocalInvocationIndex`` | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_OutputControlPointID   | HSIn        | ``InvocationId``         | N/A                   | ``Tessellation``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_GSInstanceID           | GSIn        | ``InvocationId``         | N/A                   | ``Geometry``          |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_DomainLocation         | DSIn        | ``TessCoord``            | N/A                   | ``Tessellation``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | HSIn        | ``PrimitiveId``          | N/A                   | ``Tessellation``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PCIn        | ``PrimitiveId``          | N/A                   | ``Tessellation``      |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | DsIn        | ``PrimitiveId``          | N/A                   | ``Tessellation``      |
| SV_PrimitiveID            +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSIn        | ``PrimitiveId``          | N/A                   | ``Geometry``          |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``PrimitiveId``          | N/A                   | ``Geometry``          |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``PrimitiveId``          | N/A                   | ``Geometry``          |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | PCOut       | ``TessLevelOuter``       | N/A                   | ``Tessellation``      |
| SV_TessFactor             +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSIn        | ``TessLevelOuter``       | N/A                   | ``Tessellation``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | PCOut       | ``TessLevelInner``       | N/A                   | ``Tessellation``      |
| SV_InsideTessFactor       +-------------+--------------------------+-----------------------+-----------------------+
|                           | DSIn        | ``TessLevelInner``       | N/A                   | ``Tessellation``      |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_SampleIndex            | PSIn        | ``SampleId``             | N/A                   | ``SampleRateShading`` |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_StencilRef             | PSOut       | ``FragStencilRefEXT``    | N/A                   | ``StencilExportEXT``  |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
| SV_Barycentrics           | PSIn        | ``BaryCoord*AMD``        | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``Layer``                | N/A                   | ``Geometry``          |
| SV_RenderTargetArrayIndex +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``Layer``                | N/A                   | ``Geometry``          |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | GSOut       | ``ViewportIndex``        | N/A                   | ``MultiViewport``     |
| SV_ViewportArrayIndex     +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``ViewportIndex``        | N/A                   | ``MultiViewport``     |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``SampleMask``           | N/A                   | ``Shader``            |
| SV_Coverage               +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSOut       | ``SampleMask``           | N/A                   | ``Shader``            |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+
|                           | VSIn        | ``ViewIndex``            | N/A                   | ``MultiView``         |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | HSIn        | ``ViewIndex``            | N/A                   | ``MultiView``         |
|                           +-------------+--------------------------+-----------------------+-----------------------+
| SV_ViewID                 | DSIn        | ``ViewIndex``            | N/A                   | ``MultiView``         |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | GSIn        | ``ViewIndex``            | N/A                   | ``MultiView``         |
|                           +-------------+--------------------------+-----------------------+-----------------------+
|                           | PSIn        | ``ViewIndex``            | N/A                   | ``MultiView``         |
+---------------------------+-------------+--------------------------+-----------------------+-----------------------+

For entities (function parameters, function return values, struct fields) with
the above SV semantic strings attached, SPIR-V variables of the
``Input``/``Output`` storage class will be created. They will have the
corresponding SPIR-V ``Builtin``  decorations according to the above table.

SV semantic strings not translated into SPIR-V ``BuiltIn`` decorations will be
handled similarly as non-SV (arbitrary) semantic strings: a SPIR-V variable
of the ``Input``/``Output`` storage class will be created for each entity with
such semantic string. Then sort all semantic strings according to declaration
(the default, or if ``-fvk-stage-io-order=decl`` is given) or alphabetical
(if ``-fvk-stage-io-order=alpha`` is given) order, and assign ``Location``
numbers sequentially to the corresponding SPIR-V variables. Note that this means
flattening all structs if structs are used as function parameters or returns.

There is an exception to the above rule for SV_Target[N]. It will always be
mapped to ``Location`` number N.

``gl_PerVertex``
~~~~~~~~~~~~~~~~

Variables annotated with ``SV_Position``, ``SV_ClipDistanceX``, and
``SV_CullDistanceX`` are mapped into fields of a ``gl_PerVertex`` struct:

.. code:: hlsl

    struct gl_PerVertex {
        float4 gl_Position;       // SPIR-V BuiltIn Position
        float  gl_PointSize;      // No HLSL equivalent
        float  gl_ClipDistance[]; // SPIR-V BuiltIn ClipDistance
        float  gl_CullDistance[]; // SPIR-V BuiltIn CullDistance
    };

This mimics how these builtins are handled in GLSL.

Variables decorated with ``SV_ClipDistanceX`` can be float or vector of float
type. To map them into one float array in the struct, we firstly sort them
asecendingly according to ``X``, and then concatenate them tightly. For example,

.. code:: hlsl

  struct T {
    float clip0: SV_ClipDistance0,
  };

  struct S {
    float3 clip5: SV_ClipDistance5;
    ...
  };

  void main(T t, S s, float2 clip2 : SV_ClipDistance2) { ... }

Then we have an float array of size (1 + 2 + 3 =) 6 for ``ClipDistance``, with
``clip0`` at offset 0, ``clip2`` at offset 1, ``clip5`` at offset 3.

Decorating a variable or struct member with the ``ClipDistance`` builtin but not
requiring the ``ClipDistance`` capability is legal as long as we don't read or
write the variable or struct member. But as per the way we handle `shader entry
function`_, this is not satisfied because we need to read their contents to
prepare for the source code entry function call or write back them after the
call. So annotating a variable or struct member with ``SV_ClipDistanceX`` means
requiring the ``ClipDistance`` capability in the generated SPIR-V.

Variables decorated with ``SV_CullDistanceX`` are mapped similarly as above.

HLSL register and Vulkan binding
--------------------------------

In shaders for DirectX, resources are accessed via registers; while in shaders
for Vulkan, it is done via descriptor set and binding numbers. The developer
can explicitly annotate variables in HLSL to specify descriptor set and binding
numbers, or leave it to the compiler to derive implicitly from registers.

Explicit binding number assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``[[vk::binding(X[, Y])]]`` can be attached to global variables to specify the
descriptor set as ``Y`` and binding number as ``X``. The descriptor set number
is optional; if missing, it will be zero. RW/append/consume structured buffers
have associated counters, which will occupy their own Vulkan descriptors.
``[vk::counter_binding(Z)]`` can be attached to a RW/append/consume structured
buffers to specify the binding number for the associated counter to ``Z``. Note
that the set number of the counter is always the same as the main buffer.

Implicit binding number assignment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Without explicit annotations, the compiler will try to deduce descriptor sets
and binding numbers in the following way:

If there is ``:register(xX, spaceY)`` specified for the given global variable,
the corresponding resource will be assigned to descriptor set ``Y`` and binding
number ``X``, regardless of the register type ``x``. Note that this will cause
binding number collision if, say, two resources are of different register
type but the same register number. To solve this problem, four command-line
options, ``-fvk-b-shift N M``, ``-fvk-s-shift N M``, ``-fvk-t-shift N M``, and
``-fvk-u-shift N M``, are provided to shift by ``N`` all binding numbers
inferred for register type ``b``, ``s``, ``t``, and ``u`` in space ``M``,
respectively.

If there is no register specification, the corresponding resource will be
assigned to the next available binding number, starting from 0, in descriptor
set #0.

Error checking
~~~~~~~~~~~~~~

Trying to reuse the same binding number of the same descriptor set results in
a compiler error, unless we have exactly two resources and one is an image and
the other is a sampler. This is to support the Vulkan combined image sampler.

Summary
~~~~~~~

In summary, the compiler essentially assigns binding numbers in three passes.

- Firstly it handles all declarations with explicit ``[[vk::binding(X[, Y])]]``
  annotation.
- Then the compiler processes all remaining declarations with
  ``:register(xX, spaceY)`` annotation, by applying the shift passed in using
  command-line option ``-fvk-{b|s|t|u}-shift N M``, if provided.
- Finally, the compiler assigns next available binding numbers to the rest in
  the declaration order.

As an example, for the following code:

.. code:: hlsl

  struct S { ... };

  ConstantBuffer<S> cbuffer1 : register(b0);
  Texture2D<float4> texture1 : register(t0);
  Texture2D<float4> texture2 : register(t1, space1);
  SamplerState      sampler1;
  [[vk::binding(3)]]
  RWBuffer<float4> rwbuffer1 : register(u5, space2);

If we compile with ``-fvk-t-shift 10 0 -fvk-t-shift 20 1``:

- ``rwbuffer1`` will take binding #3 in set #0, since explicit binding
  assignment has precedence over the rest.
- ``cbuffer1`` will take binding #0 in set #0, since that's what deduced from
  the register assignment, and there is no shift requested from command line.
- ``texture1`` will take binding #10 in set #0, and ``texture2`` will take
  binding #21 in set #1, since we requested an 10 shift on t-type registers.
- ``sampler1`` will take binding 1 in set #0, since that's the next available
  binding number in set #0.

.. code:: hlsl
HLSL Expressions
================

Unless explicitly noted, matrix per-element operations will be conducted on
each component vector and then collected into the result matrix. The following
sections lists the SPIR-V opcodes for scalars and vectors.

Arithmetic operators
--------------------

`Arithmetic operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Additive_and_Multiplicative_Operators>`_
(``+``, ``-``, ``*``, ``/``, ``%``) are translated into their corresponding
SPIR-V opcodes according to the following table.

+-------+-----------------------------+-------------------------------+--------------------+
|       | (Vector of) Signed Integers | (Vector of) Unsigned Integers | (Vector of) Floats |
+=======+=============================+===============================+====================+
| ``+`` |                         ``OpIAdd``                          |     ``OpFAdd``     |
+-------+-------------------------------------------------------------+--------------------+
| ``-`` |                         ``OpISub``                          |     ``OpFSub``     |
+-------+-------------------------------------------------------------+--------------------+
| ``*`` |                         ``OpIMul``                          |     ``OpFMul``     |
+-------+-----------------------------+-------------------------------+--------------------+
| ``/`` |    ``OpSDiv``               |       ``OpUDiv``              |     ``OpFDiv``     |
+-------+-----------------------------+-------------------------------+--------------------+
| ``%`` |    ``OpSRem``               |       ``OpUMod``              |     ``OpFRem``     |
+-------+-----------------------------+-------------------------------+--------------------+

Note that for modulo operation, SPIR-V has two sets of instructions: ``Op*Rem``
and ``Op*Mod``. For ``Op*Rem``, the sign of a non-0 result comes from the first
operand; while for ``Op*Mod``, the sign of a non-0 result comes from the second
operand. HLSL doc does not mandate which set of instructions modulo operations
should be translated into; it only says "the % operator is defined only in cases
where either both sides are positive or both sides are negative." So technically
it's undefined behavior to use the modulo operation with operands of different
signs. But considering HLSL's C heritage and the behavior of Clang frontend, we
translate modulo operators into ``Op*Rem`` (there is no ``OpURem``).

For multiplications of float vectors and float scalars, the dedicated SPIR-V
operation ``OpVectorTimesScalar`` will be used. Similarly, for multiplications
of float matrices and float scalars, ``OpMatrixTimesScalar`` will be generated.

Bitwise operators
-----------------

`Bitwise operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Bitwise_Operators>`_
(``~``, ``&``, ``|``, ``^``, ``<<``, ``>>``) are translated into their
corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |
+========+=============================+===============================+
| ``~``  |                         ``OpNot``                           |
+--------+-------------------------------------------------------------+
| ``&``  |                      ``OpBitwiseAnd``                       |
+--------+-------------------------------------------------------------+
| ``|``  |                      ``OpBitwiseOr``                        |
+--------+-----------------------------+-------------------------------+
| ``^``  |                      ``OpBitwiseXor``                       |
+--------+-----------------------------+-------------------------------+
| ``<<`` |                   ``OpShiftLeftLogical``                    |
+--------+-----------------------------+-------------------------------+
| ``>>`` | ``OpShiftRightArithmetic``  | ``OpShiftRightLogical``       |
+--------+-----------------------------+-------------------------------+

Comparison operators
--------------------

`Comparison operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Comparison_Operators>`_
(``<``, ``<=``, ``>``, ``>=``, ``==``, ``!=``) are translated into their
corresponding SPIR-V opcodes according to the following table.

+--------+-----------------------------+-------------------------------+------------------------------+
|        | (Vector of) Signed Integers | (Vector of) Unsigned Integers |     (Vector of) Floats       |
+========+=============================+===============================+==============================+
| ``<``  |  ``OpSLessThan``            |  ``OpULessThan``              |  ``OpFOrdLessThan``          |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``<=`` |  ``OpSLessThanEqual``       |  ``OpULessThanEqual``         |  ``OpFOrdLessThanEqual``     |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``>``  |  ``OpSGreaterThan``         |  ``OpUGreaterThan``           |  ``OpFOrdGreaterThan``       |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``>=`` |  ``OpSGreaterThanEqual``    |  ``OpUGreaterThanEqual``      |  ``OpFOrdGreaterThanEqual``  |
+--------+-----------------------------+-------------------------------+------------------------------+
| ``==`` |                     ``OpIEqual``                            |  ``OpFOrdEqual``             |
+--------+-------------------------------------------------------------+------------------------------+
| ``!=`` |                     ``OpINotEqual``                         |  ``OpFOrdNotEqual``          |
+--------+-------------------------------------------------------------+------------------------------+

Note that for comparison of (vectors of) floats, SPIR-V has two sets of
instructions: ``OpFOrd*``, ``OpFUnord*``. We translate into ``OpFOrd*`` ones.

Boolean math operators
----------------------

`Boolean match operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Boolean_Math_Operators>`_
(``&&``, ``||``, ``?:``) are translated into their corresponding SPIR-V opcodes
according to the following table.

+--------+----------------------+
|        | (Vector of) Booleans |
+========+======================+
| ``&&`` |  ``OpLogicalAnd``    |
+--------+----------------------+
| ``||`` |  ``OpLogicalOr``     |
+--------+----------------------+
| ``?:`` |  ``OpSelect``        |
+--------+----------------------+

Please note that "unlike short-circuit evaluation of ``&&``, ``||``, and ``?:``
in C, HLSL expressions never short-circuit an evaluation because they are vector
operations. All sides of the expression are always evaluated."

Unary operators
---------------

For `unary operators <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509631(v=vs.85).aspx#Unary_Operators>`_:

- ``!`` is translated into ``OpLogicalNot``. Parsing will gurantee the operands
  are of boolean types by inserting necessary casts.
- ``+`` requires no additional SPIR-V instructions.
- ``-`` is translated into ``OpSNegate`` and ``OpFNegate`` for (vectors of)
  integers and floats, respectively.

Casts
-----

Casting between (vectors) of scalar types is translated according to the following table:

+------------+-------------------+-------------------+-------------------+-------------------+
| From \\ To |        Bool       |       SInt        |      UInt         |       Float       |
+============+===================+===================+===================+===================+
|   Bool     |       no-op       |                 select between one and zero               |
+------------+-------------------+-------------------+-------------------+-------------------+
|   SInt     |                   |     no-op         |  ``OpBitcast``    | ``OpConvertSToF`` |
+------------+                   +-------------------+-------------------+-------------------+
|   UInt     | compare with zero |   ``OpBitcast``   |      no-op        | ``OpConvertUToF`` |
+------------+                   +-------------------+-------------------+-------------------+
|   Float    |                   | ``OpConvertFToS`` | ``OpConvertFToU`` |      no-op        |
+------------+-------------------+-------------------+-------------------+-------------------+

It is also feasible in HLSL to cast a float matrix to another float matrix with a smaller size.
This is known as matrix truncation cast. For instance, the following code casts a 3x4 matrix
into a 2x3 matrix.

.. code:: hlsl

  float3x4 m = { 1,  2,  3, 4,
                 5,  6,  7, 8,
                 9, 10, 11, 12 };

  float2x3 a = (float2x3)m;

Such casting takes the upper-left most corner of the original matrix to generate the result.
In the above example, matrix ``a`` will have 2 rows, with 3 columns each. First row will be
``1, 2, 3`` and the second row will be ``5, 6, 7``.

Indexing operator
-----------------

The ``[]`` operator can also be used to access elements in a matrix or vector.
A matrix whose row and/or column count is 1 will be translated into a vector or
scalar. If a variable is used as the index for the dimension whose count is 1,
that variable will be ignored in the generated SPIR-V code. This is because
out-of-bound indexing triggers undefined behavior anyway. For example, for a
1xN matrix ``mat``, ``mat[index][0]`` will be translated into
``OpAccessChain ... %mat %uint_0``. Similarly, variable index into a size 1
vector will also be ignored and the only element will be always returned.

Assignment operators
--------------------

Assigning to struct object may involve decomposing the source struct object and
assign each element separately and recursively. This happens when the source
struct object is of different memory layout from the destination struct object.
For example, for the following source code:

.. code:: hlsl

  struct S {
    float    a;
    float2   b;
    float2x3 c;
  };

      ConstantBuffer<S> cbuf;
  RWStructuredBuffer<S> sbuf;

  ...
  sbuf[0] = cbuf[0];
  ...

We need to assign each element because ``ConstantBuffer`` and
``RWStructuredBuffer`` has different memory layout.

HLSL Control Flows
==================

This section lists how various HLSL control flows are mapped.

Switch statement
----------------

HLSL `switch statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509669(v=vs.85).aspx>`_
are translated into SPIR-V using:

- **OpSwitch**: if (all case values are integer literals or constant integer
  variables) and (no attribute or the ``forcecase`` attribute is specified)
- **A series of if statements**: for all other scenarios (e.g., when
  ``flatten``, ``branch``, or ``call`` attribute is specified)

Loops (for, while, do)
----------------------

HLSL `for statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509602(v=vs.85).aspx>`_,
`while statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509708(v=vs.85).aspx>`_,
and `do statements <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509593(v=vs.85).aspx>`_
are translated into SPIR-V by constructing all necessary basic blocks and using
``OpLoopMerge`` to organize as structured loops.

The HLSL attributes for these statements are translated into SPIR-V loop control
masks according to the following table:

+-------------------------+--------------------------------------------------+
|   HLSL loop attribute   |            SPIR-V Loop Control Mask              |
+=========================+==================================================+
|        ``unroll(x)``    |                ``Unroll``                        |
+-------------------------+--------------------------------------------------+
|         ``loop``        |              ``DontUnroll``                      |
+-------------------------+--------------------------------------------------+
|        ``fastopt``      |              ``DontUnroll``                      |
+-------------------------+--------------------------------------------------+
| ``allow_uav_condition`` |           Currently Unimplemented                |
+-------------------------+--------------------------------------------------+

HLSL Functions
==============

All functions reachable from the entry-point function will be translated into
SPIR-V code. Functions not reachable from the entry-point function will be
ignored.

Entry function wrapper
----------------------

HLSL entry functions takes in parameters and returns values. These parameters
and return values can have semantics attached or if they are struct type,
the struct fields can have semantics attached. However, in Vulkan, the entry
function must be of the ``void(void)`` signature. To handle this difference,
for a given entry function ``main``, we will emit a wrapper function for it.

The wrapper function will take the name of the source code entry function,
while the source code entry function will have its name prefixed with "src.".
The wrapper function reads in stage input/builtin variables created according
to semantics and groups them into composites meeting the requirements of the
source code entry point. Then the wrapper calls the source code entry point.
The return value is extracted and components of it will be written to stage
output/builtin variables created according to semantics. For example:


.. code:: hlsl

  // HLSL source code

  struct S {
    bool a : A;
    uint2 b: B;
    float2x3 c: C;
  };

  struct T {
    S x;
    int y: D;
  };

  T main(T input) {
    return input;
  }


.. code:: spirv

  ; SPIR-V code

  %in_var_A = OpVariable %_ptr_Input_bool Input
  %in_var_B = OpVariable %_ptr_Input_v2uint Input
  %in_var_C = OpVariable %_ptr_Input_mat2v3float Input
  %in_var_D = OpVariable %_ptr_Input_int Input

  %out_var_A = OpVariable %_ptr_Output_bool Output
  %out_var_B = OpVariable %_ptr_Output_v2uint Output
  %out_var_C = OpVariable %_ptr_Output_mat2v3float Output
  %out_var_D = OpVariable %_ptr_Output_int Output

  ; Wrapper function starts

  %main    = OpFunction %void None ...
  ...      = OpLabel

  %param_var_input = OpVariable %_ptr_Function_T Function

  ; Load stage input variables and group into the expected composite

  %inA = OpLoad %bool %in_var_A
  %inB = OpLoad %v2uint %in_var_B
  %inC = OpLoad %mat2v3float %in_var_C
  %inS = OpCompositeConstruct %S %inA %inB %inC
  %inD = OpLoad %int %in_var_D
  %inT = OpCompositeConstruct %T %inS %inD
         OpStore %param_var_input %inT

  %ret = OpFunctionCall %T %src_main %param_var_input

  ; Extract component values from the composite and store into stage output variables

  %outS = OpCompositeExtract %S %ret 0
  %outA = OpCompositeExtract %bool %outS 0
          OpStore %out_var_A %outA
  %outB = OpCompositeExtract %v2uint %outS 1
          OpStore %out_var_B %outB
  %outC = OpCompositeExtract %mat2v3float %outS 2
          OpStore %out_var_C %outC
  %outD = OpCompositeExtract %int %ret 1
          OpStore %out_var_D %outD

  OpReturn
  OpFunctionEnd

  ; Source code entry point starts

  %src_main = OpFunction %T None ...

In this way, we can concentrate all stage input/output/builtin variable
manipulation in the wrapper function and handle the source code entry function
just like other nomal functions.

Function parameter
------------------

For a function ``f`` which has a parameter of type ``T``, the generated SPIR-V
signature will use type ``T*`` for the parameter. At every call site of ``f``,
additional local variables will be allocated to hold the actual arguments.
The local variables are passed in as direct function arguments. For example:

.. code:: hlsl

  // HLSL source code

  float4 f(float a, int b) { ... }

  void caller(...) {
    ...
    float4 result = f(...);
    ...
  }

.. code:: spirv

  ; SPIR-V code

                ...
  %i32PtrType = OpTypePointer Function %int
  %f32PtrType = OpTypePointer Function %float
      %fnType = OpTypeFunction %v4float %f32PtrType %i32PtrType
                ...

           %f = OpFunction %v4float None %fnType
           %a = OpFunctionParameter %f32PtrType
           %b = OpFunctionParameter %i32PtrType
                ...

      %caller = OpFunction ...
                ...
     %aAlloca = OpVariable %_ptr_Function_float Function
     %bAlloca = OpVariable %_ptr_Function_int Function
                ...
                OpStore %aAlloca ...
                OpStore %bAlloca ...
      %result = OpFunctioncall %v4float %f %aAlloca %bAlloca
                ...

This approach gives us unified handling of function parameters and local
variables: both of them are accessed via load/store instructions.

Intrinsic functions
-------------------

The following intrinsic HLSL functions have no direct SPIR-V opcode or GLSL
extended instruction mapping, so they are handled with additional steps:

- ``dot`` : performs dot product of two vectors, each containing floats or
  integers. If the two parameters are vectors of floats, we use SPIR-V's
  ``OpDot`` instruction to perform the translation. If the two parameters are
  vectors of integers, we multiply corresponding vector elements using
  ``OpIMul`` and accumulate the results using ``OpIAdd`` to compute the dot
  product.
- ``mul``: performs multiplications. Each argument may be a scalar, vector,
  or matrix. Depending on the argument type, this will be translated into
  one of the multiplication instructions.
- ``all``: returns true if all components of the given scalar, vector, or
  matrix are true. Performs conversions to boolean where necessary. Uses SPIR-V
  ``OpAll`` for scalar arguments and vector arguments. For matrix arguments,
  performs ``OpAll`` on each row, and then again on the vector containing the
  results of all rows.
- ``any``: returns true if any component of the given scalar, vector, or matrix
  is true. Performs conversions to boolean where necessary. Uses SPIR-V
  ``OpAny`` for scalar arguments and vector arguments. For matrix arguments,
  performs ``OpAny`` on each row, and then again on the vector containing the
  results of all rows.
- ``asfloat``: converts the component type of a scalar/vector/matrix from float,
  uint, or int into float. Uses ``OpBitcast``. This method currently does not
  support taking non-float matrix arguments.
- ``asint``: converts the component type of a scalar/vector/matrix from float or
  uint into int. Uses ``OpBitcast``. This method currently does not support
  conversion into integer matrices.
- ``asuint``: converts the component type of a scalar/vector/matrix from float
  or int into uint. Uses ``OpBitcast``. This method currently does not support
- ``asuint``: Converts a double into two 32-bit unsigned integers. Uses SPIR-V ``OpBitCast``.
- ``asdouble``: Converts two 32-bit unsigned integers into a double, or four 32-bit unsigned
  integers into two doubles. Uses SPIR-V ``OpVectorShuffle`` and ``OpBitCast``.
  conversion into unsigned integer matrices.
- ``isfinite`` : Determines if the specified value is finite. Since ``OpIsFinite``
  requires the ``Kernel`` capability, translation is done using ``OpIsNan`` and
  ``OpIsInf``.  A given value is finite iff it is not NaN and not infinite.
- ``clip``: Discards the current pixel if the specified value is less than zero.
  Uses conditional control flow as well as SPIR-V ``OpKill``.
- ``rcp``: Calculates a fast, approximate, per-component reciprocal.
  Uses SIR-V ``OpFDiv``.
- ``lit``: Returns a lighting coefficient vector. This vector is a float4 with
  components of (ambient, diffuse, specular, 1). How ``diffuse`` and ``specular``
  are calculated are explained `here <https://msdn.microsoft.com/en-us/library/windows/desktop/bb509619(v=vs.85).aspx>`_.
- ``D3DCOLORtoUBYTE4``: Converts a floating-point, 4D vector set by a D3DCOLOR to a UBYTE4.
  This is achieved by performing ``int4(input.zyxw * 255.002)`` using SPIR-V ``OpVectorShuffle``,
  ``OpVectorTimesScalar``, and ``OpConvertFToS``, respectively.
- ``dst``: Calculates a distance vector. The resulting vector, ``dest``, has the following specifications:
  ``dest.x = 1.0``, ``dest.y = src0.y * src1.y``, ``dest.z = src0.z``, and ``dest.w = src1.w``.
  Uses SPIR-V ``OpCompositeExtract`` and ``OpFMul``.

Using SPIR-V opcode
~~~~~~~~~~~~~~~~~~~

The following intrinsic HLSL functions have direct SPIR-V opcodes for them:

==================================== =================================
   HLSL Intrinsic Function                   SPIR-V Opcode
==================================== =================================
``AllMemoryBarrier``                 ``OpMemoryBarrier``
``AllMemoryBarrierWithGroupSync``    ``OpControlBarrier``
``countbits``                        ``OpBitCount``
``DeviceMemoryBarrier``              ``OpMemoryBarrier``
``DeviceMemoryBarrierWithGroupSync`` ``OpControlBarrier``
``ddx``                              ``OpDPdx``
``ddy``                              ``OpDPdy``
``ddx_coarse``                       ``OpDPdxCoarse``
``ddy_coarse``                       ``OpDPdyCoarse``
``ddx_fine``                         ``OpDPdxFine``
``ddy_fine``                         ``OpDPdyFine``
``fmod``                             ``OpFMod``
``fwidth``                           ``OpFwidth``
``GroupMemoryBarrier``               ``OpMemoryBarrier``
``GroupMemoryBarrierWithGroupSync``  ``OpControlBarrier``
``InterlockedAdd``                   ``OpAtomicIAdd``
``InterlockedAnd``                   ``OpAtomicAnd``
``InterlockedOr``                    ``OpAtomicOr``
``InterlockedXor``                   ``OpAtomicXor``
``InterlockedMin``                   ``OpAtomicUMin``/``OpAtomicSMin``
``InterlockedMax``                   ``OpAtomicUMax``/``OpAtomicSMax``
``InterlockedExchange``              ``OpAtomicExchange``
``InterlockedCompareExchange``       ``OpAtomicCompareExchange``
``InterlockedCompareStore``          ``OpAtomicCompareExchange``
``isnan``                            ``OpIsNan``
``isInf``                            ``OpIsInf``
``reversebits``                      ``OpBitReverse``
``transpose``                        ``OpTranspose``
``CheckAccessFullyMapped``           ``OpImageSparseTexelsResident``
==================================== =================================

Using GLSL extended instructions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following intrinsic HLSL functions are translated using their equivalent
instruction in the `GLSL extended instruction set <https://www.khronos.org/registry/spir-v/specs/1.0/GLSL.std.450.html>`_.

======================= ===================================
HLSL Intrinsic Function   GLSL Extended Instruction
======================= ===================================
``abs``                 ``SAbs``/``FAbs``
``acos``                ``Acos``
``asin``                ``Asin``
``atan``                ``Atan``
``atan2``               ``Atan2``
``ceil``                ``Ceil``
``clamp``               ``SClamp``/``UClamp``/``FClamp``
``cos``                 ``Cos``
``cosh``                ``Cosh``
``cross``               ``Cross``
``degrees``             ``Degrees``
``distance``            ``Distance``
``radians``             ``Radian``
``determinant``         ``Determinant``
``exp``                 ``Exp``
``exp2``                ``exp2``
``f16tof32``            ``UnpackHalf2x16``
``f32tof16``            ``PackHalf2x16``
``faceforward``         ``FaceForward``
``firstbithigh``        ``FindSMsb`` / ``FindUMsb``
``firstbitlow``         ``FindILsb``
``floor``               ``Floor``
``fma``                 ``Fma``
``frac``                ``Fract``
``frexp``               ``FrexpStruct``
``ldexp``               ``Ldexp``
``length``              ``Length``
``lerp``                ``FMix``
``log``                 ``Log``
``log10``               ``Log2`` (scaled by ``1/log2(10)``)
``log2``                ``Log2``
``mad``                 ``Fma``
``max``                 ``SMax``/``UMax``/``FMax``
``min``                 ``SMin``/``UMin``/``FMin``
``modf``                ``ModfStruct``
``normalize``           ``Normalize``
``pow``                 ``Pow``
``reflect``             ``Reflect``
``refract``             ``Refract``
``round``               ``Round``
``rsqrt``               ``InverseSqrt``
``saturate``            ``FClamp``
``sign``                ``SSign``/``FSign``
``sin``                 ``Sin``
``sincos``              ``Sin`` and ``Cos``
``sinh``                ``Sinh``
``smoothstep``          ``SmoothStep``
``sqrt``                ``Sqrt``
``step``                ``Step``
``tan``                 ``Tan``
``tanh``                ``Tanh``
``trunc``               ``Trunc``
======================= ===================================

HLSL OO features
================

A HLSL struct/class member method is translated into a normal SPIR-V function,
whose signature has an additional first parameter for the struct/class called
upon. Every calling site of the method is generated to pass in the object as
the first argument.

HLSL struct/class static member variables are translated into SPIR-V variables
in the ``Private`` storage class.

HLSL Methods
============

This section lists how various HLSL methods are mapped.

Buffers
-------

``Buffer``
~~~~~~~~~~

``.Load()``
+++++++++++
Since Buffers are represented as ``OpTypeImage`` with ``Sampled`` set to 1
(meaning to be used with a sampler), ``OpImageFetch`` is used to perform this
operation. The return value of ``OpImageFetch`` is always a four-component
vector; so proper additional instructions are generated to truncate the vector
and return the desired number of elements.
If an output unsigned integer ``status`` argument is present, ``OpImageSparseFetch``
is used instead. The resulting SPIR-V ``Residency Code`` will be written to ``status``.

``operator[]``
++++++++++++++
Handled similarly as ``.Load()``.

``.GetDimensions()``
++++++++++++++++++++
Since Buffers are represented as ``OpTypeImage`` with dimension of ``Buffer``,
``OpImageQuerySize`` is used to perform this operation.

``RWBuffer``
~~~~~~~~~~~~

``.Load()``
+++++++++++
Since RWBuffers are represented as ``OpTypeImage`` with ``Sampled`` set to 2
(meaning to be used without a sampler), ``OpImageRead`` is used to perform this
operation. If an output unsigned integer ``status`` argument is present, ``OpImageSparseRead``
is used instead. The resulting SPIR-V ``Residency Code`` will be written to ``status``.

``operator[]``
++++++++++++++
Using ``operator[]`` for reading is handled similarly as ``.Load()``, while for
writing, the ``OpImageWrite`` instruction is generated.

``.GetDimensions()``
++++++++++++++++++++
Since RWBuffers are represented as ``OpTypeImage`` with dimension of ``Buffer``,
``OpImageQuerySize`` is used to perform this operation.

``StructuredBuffer`` and ``RWStructuredBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``.GetDimensions()``
++++++++++++++++++++
Since StructuredBuffers/RWStructuredBuffers are represented as a struct with one
member that is a runtime array of structures, ``OpArrayLength`` is invoked on
the runtime array in order to find the dimension.

``ByteAddressBuffer``
~~~~~~~~~~~~~~~~~~~~~

``.GetDimensions()``
++++++++++++++++++++
Since ByteAddressBuffers are represented as a struct with one member that is a
runtime array of unsigned integers, ``OpArrayLength`` is invoked on the runtime array
in order to find the number of unsigned integers. This is then multiplied by 4 to find
the number of bytes.

``.Load()``, ``.Load2()``, ``.Load3()``, ``.Load4()``
+++++++++++++++++++++++++++++++++++++++++++++++++++++
ByteAddressBuffers are represented as a struct with one member that is a runtime array of
unsigned integers. The ``address`` argument passed to the function is first divided by 4
in order to find the offset into the array (because each array element is 4 bytes). The
SPIR-V ``OpAccessChain`` instruction is then used to access that offset, and ``OpLoad`` is
used to load a 32-bit unsigned integer. For ``Load2``, ``Load3``, and ``Load4``, this is
done 2, 3, and 4 times, respectively. Each time the word offset is incremented by 1 before
performing ``OpAccessChain``. After all ``OpLoad`` operations are performed, a vector is
constructed with all the resulting values.

``RWByteAddressBuffer``
~~~~~~~~~~~~~~~~~~~~~~~

``.GetDimensions()``
++++++++++++++++++++
Since RWByteAddressBuffers are represented as a struct with one member that is a
runtime array of unsigned integers, ``OpArrayLength`` is invoked on the runtime array
in order to find the number of unsigned integers. This is then multiplied by 4 to find
the number of bytes.

``.Load()``, ``.Load2()``, ``.Load3()``, ``.Load4()``
+++++++++++++++++++++++++++++++++++++++++++++++++++++
RWByteAddressBuffers are represented as a struct with one member that is a runtime array of
unsigned integers. The ``address`` argument passed to the function is first divided by 4
in order to find the offset into the array (because each array element is 4 bytes). The
SPIR-V ``OpAccessChain`` instruction is then used to access that offset, and ``OpLoad`` is
used to load a 32-bit unsigned integer. For ``Load2``, ``Load3``, and ``Load4``, this is
done 2, 3, and 4 times, respectively. Each time the word offset is incremented by 1 before
performing ``OpAccessChain``. After all ``OpLoad`` operations are performed, a vector is
constructed with all the resulting values.

``.Store()``, ``.Store2()``, ``.Store3()``, ``.Store4()``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
RWByteAddressBuffers are represented as a struct with one member that is a runtime array of
unsigned integers. The ``address`` argument passed to the function is first divided by 4
in order to find the offset into the array (because each array element is 4 bytes). The
SPIR-V ``OpAccessChain`` instruction is then used to access that offset, and ``OpStore`` is
used to store a 32-bit unsigned integer. For ``Store2``, ``Store3``, and ``Store4``, this is
done 2, 3, and 4 times, respectively. Each time the word offset is incremented by 1 before
performing ``OpAccessChain``.

``.Interlocked*()``
+++++++++++++++++++

================================= =================================
     HLSL Intrinsic Method                SPIR-V Opcode
================================= =================================
``.InterlockedAdd()``             ``OpAtomicIAdd``
``.InterlockedAnd()``             ``OpAtomicAnd``
``.InterlockedOr()``              ``OpAtomicOr``
``.InterlockedXor()``             ``OpAtomicXor``
``.InterlockedMin()``             ``OpAtomicUMin``/``OpAtomicSMin``
``.InterlockedMax()``             ``OpAtomicUMax``/``OpAtomicSMax``
``.InterlockedExchange()``        ``OpAtomicExchange``
``.InterlockedCompareExchange()`` ``OpAtomicCompareExchange``
``.InterlockedCompareStore()``    ``OpAtomicCompareExchange``
================================= =================================

``AppendStructuredBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~

``.Append()``
+++++++++++++

The associated counter number will be increased by 1 using ``OpAtomicIAdd``.
The return value of ``OpAtomicIAdd``, which is the original count number, will
be used as the index for storing the new element. E.g., for ``buf.Append(vec)``:

.. code:: spirv

  %counter = OpAccessChain %_ptr_Uniform_int %counter_var_buf %uint_0
    %index = OpAtomicIAdd %uint %counter %uint_1 %uint_0 %uint_1
      %ptr = OpAccessChain %_ptr_Uniform_v4float %buf %uint_0 %index
      %val = OpLoad %v4float %vec
             OpStore %ptr %val

``.GetDimensions()``
++++++++++++++++++++
Since AppendStructuredBuffers are represented as a struct with one member that
is a runtime array, ``OpArrayLength`` is invoked on the runtime array in order
to find the number of elements. The stride is also calculated based on GLSL
``std430`` as explained above.

``ConsumeStructuredBuffer``
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``.Consume()``
++++++++++++++

The associated counter number will be decreased by 1 using ``OpAtomicISub``.
The return value of ``OpAtomicISub`` minus 1, which is the new count number,
will be used as the index for reading the new element. E.g., for
``buf.Consume(vec)``:

.. code:: spirv

  %counter = OpAccessChain %_ptr_Uniform_int %counter_var_buf %uint_0
     %prev = OpAtomicISub %uint %counter %uint_1 %uint_0 %uint_1
    %index = OpISub %uint %prev %uint_1
      %ptr = OpAccessChain %_ptr_Uniform_v4float %buf %uint_0 %index
      %val = OpLoad %v4float %vec
             OpStore %ptr %val

``.GetDimensions()``
++++++++++++++++++++
Since ConsumeStructuredBuffers are represented as a struct with one member that
is a runtime array, ``OpArrayLength`` is invoked on the runtime array in order
to find the number of elements. The stride is also calculated based on GLSL
``std430`` as explained above.

Read-only textures
------------------

Methods common to all texture types are explained in the "common texture methods"
section. Methods unique to a specific texture type is explained in the section
for that texture type.

Common texture methods
~~~~~~~~~~~~~~~~~~~~~~

``.Sample(sampler, location[, offset][, clamp][, Status])``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture2DMS`` and ``Texture2DMSArray``.

The ``OpImageSampleImplicitLod`` instruction is used to translate ``.Sample()``
since texture types are represented as ``OpTypeImage``. An ``OpSampledImage`` is
created based on the ``sampler`` passed to the function. The resulting sampled
image and the ``location`` passed to the function are used as arguments to
``OpImageSampleImplicitLod``, with the optional ``offset`` tranlated into
addtional SPIR-V image operands ``ConstOffset`` or ``Offset`` on it. The optional
``clamp`` argument will be translated to the ``MinLod`` image operand.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleImplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.SampleLevel(sampler, location, lod[, offset][, Status])``
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture2DMS`` and ``Texture2DMSArray``.

The ``OpImageSampleExplicitLod`` instruction is used to translate this method.
An ``OpSampledImage`` is created based on the ``sampler`` passed to the function.
The resulting sampled image and the ``location`` passed to the function are used
as arguments to ``OpImageSampleExplicitLod``. The ``lod`` passed to the function
is attached to the instruction as an SPIR-V image operands ``Lod``. The optional
``offset`` is also tranlated into addtional SPIR-V image operands ``ConstOffset``
or ``Offset`` on it.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleExplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.SampleGrad(sampler, location, ddx, ddy[, offset][, clamp][, Status])``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture2DMS`` and ``Texture2DMSArray``.

Similarly to ``.SampleLevel``, the ``ddx`` and ``ddy`` parameter are attached to
the ``OpImageSampleExplicitLod`` instruction as an SPIR-V image operands
``Grad``. The optional ``clamp`` argument will be translated into the ``MinLod``
image operand.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleExplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.SampleBias(sampler, location, bias[, offset][, clamp][, Status])``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture2DMS`` and ``Texture2DMSArray``.

The translation is similar to ``.Sample()``, with the ``bias`` parameter
attached to the ``OpImageSampleImplicitLod`` instruction as an SPIR-V image
operands ``Bias``.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleImplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.SampleCmp(sampler, location, comparator[, offset][, clamp][, Status])``
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture3D``, ``Texture2DMS``, and ``Texture2DMSArray``.

The translation is similar to ``.Sample()``, but the
``OpImageSampleDrefImplicitLod`` instruction are used.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleDrefImplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.SampleCmpLevelZero(sampler, location, comparator[, offset][, Status])``
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Not available to ``Texture3D``, ``Texture2DMS``, and ``Texture2DMSArray``.

The translation is similar to ``.Sample()``, but the
``OpImageSampleDrefExplicitLod`` instruction are used, with the additional
``Lod`` image operands set to 0.0.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseSampleDrefExplicitLod`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.Gather()``
+++++++++++++

Available to ``Texture2D``, ``Texture2DArray``, ``TextureCube``, and
``TextureCubeArray``.

The translation is similar to ``.Sample()``, but the ``OpImageGather``
instruction is used, with component setting to 0.

If an output unsigned integer ``status`` argument is present,
``OpImageSparseGather`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``.GatherRed()``, ``.GatherGreen()``, ``.GatherBlue()``, ``.GatherAlpha()``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Available to ``Texture2D``, ``Texture2DArray``, ``TextureCube``, and
``TextureCubeArray``.

The ``OpImageGather`` instruction is used to translate these functions, with
component setting to 0, 1, 2, and 3 respectively.

There are a few overloads for these functions:

- For those overloads taking 4 offset parameters, those offset parameters will
  be conveyed as an additional ``ConstOffsets`` image operands to the
  instruction. So those offset parameters must all be constant values.
- For those overloads with the ``status`` parameter, ``OpImageSparseGather``
  is used instead, and the resulting SPIR-V ``Residency Code`` will be
  written to ``status``.

``.GatherCmp()``
++++++++++++++++

Available to ``Texture2D``, ``Texture2DArray``, ``TextureCube``, and
``TextureCubeArray``.

The translation is similar to ``.Sample()``, but the ``OpImageDrefGather``
instruction is used.

For the overload with the output unsigned integer ``status`` argument,
``OpImageSparseDrefGather`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.


``.GatherCmpRed()``
+++++++++++++++++++

Available to ``Texture2D``, ``Texture2DArray``, ``TextureCube``, and
``TextureCubeArray``.

The translation is the same as ``.GatherCmp()``.

``.Load(location[, sampleIndex][, offset])``
++++++++++++++++++++++++++++++++++++++++++++

The ``OpImageFetch`` instruction is used for translation because texture types
are represented as ``OpTypeImage``. The last element in the ``location``
parameter will be used as arguments to the ``Lod`` SPIR-V image operand attached
to the ``OpImageFetch`` instruction, and the rest are used as the coordinate
argument to the instruction. ``offset`` is handled similarly to ``.Sample()``.
The return value of ``OpImageFetch`` is always a four-component vector; so
proper additional instructions are generated to truncate the vector and return
the desired number of elements.

For the overload with the output unsigned integer ``status`` argument,
``OpImageSparseFetch`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``operator[]``
++++++++++++++
Handled similarly as ``.Load()``.

``.mips[lod][position]``
++++++++++++++++++++++++

Not available to ``TextureCube``, ``TextureCubeArray``, ``Texture2DMS``, and
``Texture2DMSArray``.

This method is translated into the ``OpImageFetch`` instruction. The ``lod``
parameter is attached to the instruction as the parameter to the ``Lod`` SPIR-V
image operands. The ``position`` parameter are used as the coordinate to the
instruction directly.

``.CalculateLevelOfDetail()``
+++++++++++++++++++++++++++++

Not available to ``Texture2DMS`` and ``Texture2DMSArray``.

Since texture types are represented as ``OpTypeImage``, the ``OpImageQueryLod``
instruction is used for translation. An ``OpSampledImage`` is created based on
the ``SamplerState`` passed to the function. The resulting sampled image and
the coordinate passed to the function are used to invoke ``OpImageQueryLod``.
The result of ``OpImageQueryLod`` is a ``float2``. The first element contains
the mipmap array layer.

``Texture1D``
~~~~~~~~~~~~~

``.GetDimensions(width)`` or ``.GetDimensions(MipLevel, width, NumLevels)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture1D is represented as ``OpTypeImage``, the ``OpImageQuerySizeLod`` instruction
is used for translation. If a ``MipLevel`` argument is passed to ``GetDimensions``, it will
be used as the ``Lod`` parameter of the query instruction. Otherwise, ``Lod`` of ``0`` be used.

``Texture1DArray``
~~~~~~~~~~~~~~~~~~

``.GetDimensions(width, elements)`` or ``.GetDimensions(MipLevel, width, elements, NumLevels)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture1DArray is represented as ``OpTypeImage``, the ``OpImageQuerySizeLod`` instruction
is used for translation. If a ``MipLevel`` argument is present, it will be used as the
``Lod`` parameter of the query instruction. Otherwise, ``Lod`` of ``0`` be used.

``Texture2D``
~~~~~~~~~~~~~

``.GetDimensions(width, height)`` or ``.GetDimensions(MipLevel, width, height, NumLevels)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture2D is represented as ``OpTypeImage``, the ``OpImageQuerySizeLod`` instruction
is used for translation. If a ``MipLevel`` argument is present, it will be used as the
``Lod`` parameter of the query instruction. Otherwise, ``Lod`` of ``0`` be used.

``Texture2DArray``
~~~~~~~~~~~~~~~~~~

``.GetDimensions(width, height, elements)`` or ``.GetDimensions(MipLevel, width, height, elements, NumLevels)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture2DArray is represented as ``OpTypeImage``, the ``OpImageQuerySizeLod`` instruction
is used for translation. If a ``MipLevel`` argument is present, it will be used as the
``Lod`` parameter of the query instruction. Otherwise, ``Lod`` of ``0`` be used.

``Texture3D``
~~~~~~~~~~~~~

``.GetDimensions(width, height, depth)`` or ``.GetDimensions(MipLevel, width, height, depth, NumLevels)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture3D is represented as ``OpTypeImage``, the ``OpImageQuerySizeLod`` instruction
is used for translation. If a ``MipLevel`` argument is present, it will be used as the
``Lod`` parameter of the query instruction. Otherwise, ``Lod`` of ``0`` be used.

``Texture2DMS``
~~~~~~~~~~~~~~~

``.sample[sample][position]``
+++++++++++++++++++++++++++++
This method is translated into the ``OpImageFetch`` instruction. The ``sample``
parameter is attached to the instruction as the parameter to the ``Sample``
SPIR-V image operands. The ``position`` parameter are used as the coordinate to
the instruction directly.

``.GetDimensions(width, height, numSamples)``
+++++++++++++++++++++++++++++++++++++++++++++
Since Texture2DMS is represented as ``OpTypeImage`` with ``MS`` of ``1``, the ``OpImageQuerySize`` instruction
is used to get the width and the height. Furthermore, ``OpImageQuerySamples`` is used to get the numSamples.

``Texture2DMSArray``
~~~~~~~~~~~~~~~~~~~~

``.sample[sample][position]``
+++++++++++++++++++++++++++++
This method is translated into the ``OpImageFetch`` instruction. The ``sample``
parameter is attached to the instruction as the parameter to the ``Sample``
SPIR-V image operands. The ``position`` parameter are used as the coordinate to
the instruction directly.

``.GetDimensions(width, height, elements, numSamples)``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++
Since Texture2DMS is represented as ``OpTypeImage`` with ``MS`` of ``1``, the ``OpImageQuerySize`` instruction
is used to get the width, the height, and the elements. Furthermore, ``OpImageQuerySamples`` is used to get the numSamples.

``TextureCube``
~~~~~~~~~~~~~~~

``TextureCubeArray``
~~~~~~~~~~~~~~~~~~~~

Read-write textures
-------------------

Methods common to all texture types are explained in the "common texture methods"
section. Methods unique to a specific texture type is explained in the section
for that texture type.

Common texture methods
~~~~~~~~~~~~~~~~~~~~~~

``.Load()``
+++++++++++
Since read-write texture types are represented as ``OpTypeImage`` with
``Sampled`` set to 2 (meaning to be used without a sampler), ``OpImageRead`` is
used to perform this operation.

For the overload with the output unsigned integer ``status`` argument,
``OpImageSparseRead`` is used instead. The resulting SPIR-V
``Residency Code`` will be written to ``status``.

``operator[]``
++++++++++++++
Using ``operator[]`` for reading is handled similarly as ``.Load()``, while for
writing, the ``OpImageWrite`` instruction is generated.

``RWTexture1D``
~~~~~~~~~~~~~~~

``.GetDimensions(width)``
+++++++++++++++++++++++++
The ``OpImageQuerySize`` instruction is used to find the width.

``RWTexture1DArray``
~~~~~~~~~~~~~~~~~~~~

``.GetDimensions(width, elements)``
+++++++++++++++++++++++++++++++++++
The ``OpImageQuerySize`` instruction is used to get a uint2. The first element
is the width, and the second is the elements.

``RWTexture2D``
~~~~~~~~~~~~~~~

``.GetDimensions(width, height)``
+++++++++++++++++++++++++++++++++
The ``OpImageQuerySize`` instruction is used to get a uint2. The first element is the width, and the second
element is the height.

``RWTexture2DArray``
~~~~~~~~~~~~~~~~~~~~

``.GetDimensions(width, height, elements)``
+++++++++++++++++++++++++++++++++++++++++++
The ``OpImageQuerySize`` instruction is used to get a uint3. The first element is the width, the second
element is the height, and the third is the elements.

``RWTexture3D``
~~~~~~~~~~~~~~~

``.GetDimensions(width, height, depth)``
++++++++++++++++++++++++++++++++++++++++
The ``OpImageQuerySize`` instruction is used to get a uint3. The first element is the width, the second
element is the height, and the third element is the depth.


HLSL Shader Stages
==================

Hull Shaders
------------

Hull shaders corresponds to Tessellation Control Shaders (TCS) in Vulkan.
This section describes how Hull shaders are translated to SPIR-V for Vulkan.

Hull Entry Point Attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~
The following HLSL attributes are attached to the main entry point of hull shaders
and are translated to SPIR-V execution modes according to the table below:

.. table:: Mapping from HLSL attribute to SPIR-V execution mode

+-------------------------+---------------------+--------------------------+
| HLSL Attribute          |   value             | SPIR-V Execution Mode    |
+=========================+=====================+==========================+
|                         | ``quad``            | ``Quads``                |
|                         +---------------------+--------------------------+
|    ``domain``           | ``tri``             | ``Triangles``            |
|                         +---------------------+--------------------------+
|                         | ``isoline``         | ``Isoline``              |
+-------------------------+---------------------+--------------------------+
|                         | ``integer``         | ``SpacingEqual``         |
|                         +---------------------+--------------------------+
|                         | ``fractional_even`` | ``SpacingFractionalEven``|
|    ``partitioning``     +---------------------+--------------------------+
|                         | ``fractional_odd``  | ``SpacingFractionalOdd`` |
|                         +---------------------+--------------------------+
|                         | ``pow2``            |           N/A            |
+-------------------------+---------------------+--------------------------+
|                         | ``point``           | ``PointMode``            |
|                         +---------------------+--------------------------+
|                         | ``line``            |           N/A            |
|  ``outputtopology``     +---------------------+--------------------------+
|                         | ``triangle_cw``     | ``VertexOrderCw``        |
|                         +---------------------+--------------------------+
|                         | ``triangle_ccw``    | ``VertexOrderCcw``       |
+-------------------------+---------------------+--------------------------+
|``outputcontrolpoints``  | ``n``               | ``OutputVertices n``     |
+-------------------------+---------------------+--------------------------+

The ``patchconstfunc`` attribute does not have a direct equivalent in SPIR-V.
It specifies the name of the Patch Constant Function. This function is run only
once per patch. This is further described below.

InputPatch and OutputPatch
~~~~~~~~~~~~~~~~~~~~~~~~~~
Both of ``InputPatch<T, N>`` and ``OutputPatch<T, N>`` are translated to an array
of constant size ``N`` where each element is of type ``T``.

InputPatch can be passed to the Hull shader main entry function as well as the
patch constant function. This would include information about each of the ``N``
vertices that are input to the tessellation control shader.

OutputPatch is an array containing ``N`` elements (where ``N`` is the number of
output vertices). Each element of the array contains information about an
output vertex. OutputPatch may also be passed to the patch constant function.

The SPIR-V ``InvocationID`` (``SV_OutputControlPointID`` in HLSL) is used to index
into the InputPatch and OutputPatch arrays to read/write information for the given
vertex.

The hull main entry function in HLSL returns only one value (say, of type ``T``), but
that function is in fact executed once for each control point. The Vulkan spec requires that
"Tessellation control shader per-vertex output variables and blocks, and tessellation control,
tessellation evaluation, and geometry shader per-vertex input variables and blocks are required
to be declared as arrays, with each element representing input or output values for a single vertex
of a multi-vertex primitive". Therefore, we need to create a stage output variable that is an array
with elements of type ``T``. The number of elements of the array is equal to the number of
output control points. Each final output control point is written into the corresponding element in
the array using SV_OutputControlPointID as the index.

Patch Constant Function
~~~~~~~~~~~~~~~~~~~~~~~
As mentioned above, the patch constant function is to be invoked only once per patch.
As a result, in the SPIR-V module, the `entry function wrapper`_ will first invoke the
main entry function, and then use an ``OpControlBarrier`` to wait for all vertex
processing to finish. After the barrier, *only* the first thread (with InvocationID of 0)
will invoke the patch constant function.

The information resulting from the patch constant function will also be returned
as stage output variables. The output struct of the patch constant function must include
``SV_TessFactor`` and ``SV_InsideTessFactor`` fields which will translate to
``TessLevelOuter`` and ``TessLevelInner`` builtin variables, respectively. And the rest
will be flattened and translated into normal stage output variables, one for each field.

Geometry Shaders
----------------

This section describes how geometry shaders are translated to SPIR-V for Vulkan.

Geometry Shader Entry Point Attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The following HLSL attribute is attached to the main entry point of geometry shaders
and is translated to SPIR-V execution mode as follows:

.. table:: Mapping from geometry shader HLSL attribute to SPIR-V execution mode

+-------------------------+---------------------+--------------------------+
| HLSL Attribute          |   value             | SPIR-V Execution Mode    |
+=========================+=====================+==========================+
|``maxvertexcount``       | ``n``               | ``OutputVertices n``     |
+-------------------------+---------------------+--------------------------+
|``instance``             | ``n``               | ``Invocations n``        |
+-------------------------+---------------------+--------------------------+

Translation for Primitive Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Geometry shader vertex inputs may be qualified with primitive types. Only one primitive type
is allowed to be used in a given geometry shader. The following table shows the SPIR-V execution
mode that is used in order to represent the given primitive type.

.. table:: Mapping from geometry shader primitive type to SPIR-V execution mode

+---------------------+-----------------------------+
| HLSL Primitive Type | SPIR-V Execution Mode       |
+=====================+=============================+
|``point``            | ``InputPoints``             |
+---------------------+-----------------------------+
|``line``             | ``InputLines``              |
+---------------------+-----------------------------+
|``triangle``         | ``Triangles``               |
+---------------------+-----------------------------+
|``lineadj``          | ``InputLinesAdjacency``     |
+---------------------+-----------------------------+
|``triangleadj``      | ``InputTrianglesAdjacency`` |
+---------------------+-----------------------------+

Translation of Output Stream Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Supported output stream types in geometry shaders are: ``PointStream<T>``,
``LineStream<T>``, and ``TriangleStream<T>``. These types are translated as the underlying
type ``T``, which is recursively flattened into stand-alone variables for each field.

Furthermore, output stream objects passed to geometry shader entry points are
required to be annotated with ``inout``, but the generated SPIR-V only contains
stage output variables for them.

The following table shows the SPIR-V execution mode that is used in order to represent the
given output stream.

.. table:: Mapping from geometry shader output stream type to SPIR-V execution mode

+---------------------+-----------------------------+
| HLSL Output Stream  | SPIR-V Execution Mode       |
+=====================+=============================+
|``PointStream``      | ``OutputPoints``            |
+---------------------+-----------------------------+
|``LineStream``       | ``OutputLineStrip``         |
+---------------------+-----------------------------+
|``TriangleStream``   | ``OutputTriangleStrip``     |
+---------------------+-----------------------------+

In other shader stages, stage output variables are only written in the `entry
function wrapper`_ after calling the source code entry function. However,
geometry shaders can output as many vertices as they wish, by calling the
``.Append()`` method on the output stream object. Therefore, it is incorrect to
have only one flush in the entry function wrapper like other stages. Instead,
each time a ``*Stream<T>::Append()`` is encountered, all stage output variables
behind ``T`` will be flushed before SPIR-V ``OpEmitVertex`` instruction is
generated. ``.RestartStrip()`` method calls will be translated into the SPIR-V
``OpEndPrimitive`` instruction.

Vulkan Command-line Options
===========================

The following command line options are added into ``dxc`` to support SPIR-V
codegen for Vulkan:

- ``-spirv``: Generates SPIR-V code.
- ``-fvk-b-shift N M``: Shifts by ``N`` the inferred binding numbers for all
  resources in b-type registers of space ``M``. Specifically, for a resouce
  attached with ``:register(bX, spaceM)`` but not ``[vk::binding(...)]``,
  sets its Vulkan descriptor set to ``M`` and binding number to ``X + N``. If
  you need to shift the inferred binding numbers for more than one space,
  provide more than one such option. If more than one such option is provided
  for the same space, the last one takes effect. See `HLSL register and Vulkan
  binding`_ for explanation and examples.
- ``-fvk-t-shift N M``, similar to ``-fvk-b-shift``, but for t-type registers.
- ``-fvk-s-shift N M``, similar to ``-fvk-b-shift``, but for s-type registers.
- ``-fvk-u-shift N M``, similar to ``-fvk-b-shift``, but for u-type registers.
- ``-fvk-ignore-unused-resources``: Avoids emitting SPIR-V code for resources
  defined but not statically referenced by the call tree of the entry point
  in question.
- ``-fvk-stage-io-order={alpha|decl}``: Assigns the stage input/output variable
  location number according to alphabetical order or declaration order. See
  `HLSL semantic and Vulkan Location`_ for more details.

Unsupported HLSL Features
=========================

The following HLSL language features are not supported in SPIR-V codegen,
either because of no Vulkan equivalents at the moment, or because of deprecation.

* Literal/immediate sampler state: deprecated feature. The compiler will
  emit a warning and ignore it.
* ``abort()`` intrinsic function: no Vulkan equivalent. The compiler will emit
  an error.
* ``GetRenderTargetSampleCount()`` intrinsic function: no Vulkan equivalent.
  (Its GLSL counterpart is ``gl_NumSamples``, which is not available in GLSL for
  Vulkan.) The compiler will emit an error.
* ``GetRenderTargetSamplePosition()`` intrinsic function: no Vulkan equivalent.
  (``gl_SamplePosition`` provides similar functionality but it's only for the
  sample currently being processed.) The compiler will emit an error.
* ``tex*()`` intrinsic functions: deprecated features. The compiler will
  emit errors.
* ``.GatherCmpGreen()``, ``.GatherCmpBlue()``, ``.GatherCmpAlpha()`` intrinsic
  method: no Vulkan equivalent. (SPIR-V ``OpImageDrefGather`` instruction does
  not take component as input.) The compiler will emit an error.
* ``.CalculateLevelOfDetailUnclamped()`` intrinsic method: no Vulkan equivalent.
  (SPIR-V ``OpImageQueryLod`` returns the clamped LOD in Vulkan.) The compiler
  will emit an error.
* ``.GetSamplePosition()`` intrinsic method: no Vulkan equivalent.
  (``gl_SamplePosition`` provides similar functionality but it's only for the
  sample currently being processed.) The compiler will emit an error.
* ``SV_InnerCoverage`` semantic does not have a Vulkan equivalent. The compiler
  will emit an error.
* Since ``StructuredBuffer``, ``RWStructuredBuffer``, ``ByteAddressBuffer``, and
  ``RWByteAddressBuffer`` are not represented as image types in SPIR-V, using the
  output unsigned integer ``status`` argument in their ``Load*`` methods is not
  supported. Using these methods with the ``status`` argument will cause a compiler error.
* Applying ``row_major`` or ``column_major`` attributes to a stand-alone matrix will be
  ignored by the compiler because ``RowMajor`` and ``ColMajor`` decorations in SPIR-V are
  only allowed to be applied to members of structures. A warning will be issued by the compiler.
* The Hull shader ``partitioning`` attribute may not have the ``pow2`` value. The compiler
  will emit an error. Other attribute values are supported and described in the
  `Hull Entry Point Attributes`_ section.
* ``cbuffer``/``tbuffer`` member initializer: no Vulkan equivalent. The compiler
  will emit an warning and ignore it.
* ``:packoffset()``: Not supported right now. The compiler will emit an warning
  and ignore it.
