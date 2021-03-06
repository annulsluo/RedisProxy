# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: redisproxy.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf.internal import enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='redisproxy.proto',
  package='RedisProxy',
  syntax='proto2',
  serialized_options=None,
  serialized_pb=_b('\n\x10redisproxy.proto\x12\nRedisProxy\"\x81\x01\n\x0bRequestHead\x12\x39\n\tnCallMode\x18\x01 \x01(\x0e\x32\x15.RedisProxy.ECallMode:\x0f\x43\x41LLMODE_SINGLE\x12\x37\n\tnCallType\x18\x02 \x01(\x0e\x32\x15.RedisProxy.ECallType:\rCALLTYPE_SYNC\"Y\n\x0eRequestElement\x12\x1b\n\x13nRequestElementType\x18\x01 \x01(\x05\x12\x11\n\tnCharType\x18\x02 \x01(\x05\x12\x17\n\x0fsRequestElement\x18\x03 \x01(\t\"\x83\x01\n\x0bRequestBody\x12\x0c\n\x04sCmd\x18\x01 \x01(\t\x12\x0c\n\x04sKey\x18\x02 \x01(\t\x12\x0e\n\x06sValue\x18\x03 \x01(\t\x12\x37\n\x13oRequestElementList\x18\x04 \x03(\x0b\x32\x1a.RedisProxy.RequestElement\x12\x0f\n\x04nVer\x18\x05 \x01(\x05:\x01\x31\"\xbb\x01\n\x11RedisProxyRequest\x12\x0e\n\x06sAppId\x18\x01 \x01(\t\x12\x0e\n\x06sAppPw\x18\x02 \x01(\t\x12\x10\n\x08sAppName\x18\x03 \x01(\t\x12\x12\n\nsTableName\x18\x04 \x01(\t\x12-\n\x0coRequestHead\x18\x05 \x01(\x0b\x32\x17.RedisProxy.RequestHead\x12\x31\n\x10oRequestBodyList\x18\x06 \x03(\x0b\x32\x17.RedisProxy.RequestBody\"J\n\rResultElement\x12\x0c\n\x04nRet\x18\x01 \x01(\x05\x12\x13\n\x0bnResultType\x18\x02 \x01(\x05\x12\x16\n\x0esResultElement\x18\x03 \x01(\t\"\x84\x01\n\nResultBody\x12\x0c\n\x04nRet\x18\x01 \x01(\x05\x12\x0e\n\x06sValue\x18\x02 \x01(\t\x12\x13\n\x0bnResultType\x18\x03 \x01(\x05\x12\x35\n\x12oResultElementList\x18\x04 \x03(\x0b\x32\x19.RedisProxy.ResultElement\x12\x0c\n\x04nVer\x18\x05 \x01(\x05\"T\n\x12RedisProxyResponse\x12/\n\x0foResultBodyList\x18\x01 \x03(\x0b\x32\x16.RedisProxy.ResultBody\x12\r\n\x05nGret\x18\x02 \x01(\x05*\xc7\x01\n\x0b\x45ResultCode\x12\x0f\n\x0bRESULT_SUCC\x10\x00\x12\x18\n\x0bRESULT_FAIL\x10\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12!\n\x14RESULT_DATA_NOTFOUND\x10\x9c\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12#\n\x16RESULT_UNKNOWN_COMMAND\x10\xb8\xfe\xff\xff\xff\xff\xff\xff\xff\x01\x12#\n\x16RESULT_UNKNOWN_VERSION\x10\xb7\xfe\xff\xff\xff\xff\xff\xff\xff\x01\x12 \n\x13RESULT_SYSTEM_ERROR\x10\xd4\xfd\xff\xff\xff\xff\xff\xff\xff\x01*\xea\x01\n\x0b\x45GlobalCode\x12\x0f\n\x0bGLOBAL_SUCC\x10\x00\x12\x18\n\x0bGLOBAL_FAIL\x10\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12!\n\x14GLOBAL_INVALID_PARAM\x10\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12&\n\x19GLOBAL_INVALID_USERPASSWD\x10\xfd\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12\x1e\n\x11GLOBAL_INVALID_IP\x10\xfc\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12\x1e\n\x11GLOBAL_QUOTAEXCEE\x10\xfb\xff\xff\xff\xff\xff\xff\xff\xff\x01\x12%\n\x18GLOBAL_TOO_MANY_REQUESTS\x10\xfa\xff\xff\xff\xff\xff\xff\xff\xff\x01*E\n\tECallType\x12\x11\n\rCALLTYPE_NONE\x10\x00\x12\x11\n\rCALLTYPE_SYNC\x10\x01\x12\x12\n\x0e\x43\x41LLTYPE_ASYNC\x10\x02*\\\n\tECallMode\x12\x11\n\rCALLMODE_NONE\x10\x00\x12\x13\n\x0f\x43\x41LLMODE_SINGLE\x10\x01\x12\x10\n\x0c\x43\x41LLMODE_MUL\x10\x02\x12\x15\n\x11\x43\x41LLMODE_PIPELINE\x10\x03*X\n\x08\x45Version\x12\x10\n\x0cVERSION_NONE\x10\x00\x12\x0e\n\nVERSION_KV\x10\x01\x12\x0f\n\x0bVERSION_CMD\x10\x02\x12\x19\n\x15VERSION_ELEMENTVECTOR\x10\x03*H\n\tECharType\x12\x11\n\rCHARTYPE_NONE\x10\x00\x12\x13\n\x0f\x43HARTYPE_STRING\x10\x01\x12\x13\n\x0f\x43HARTYPE_BINARY\x10\x02*\xd1\x02\n\x13\x45RequestElementType\x12\x1b\n\x17REQUESTELEMENTTYPE_NONE\x10\x00\x12\x1a\n\x16REQUESTELEMENTTYPE_CMD\x10\x01\x12\x1a\n\x16REQUESTELEMENTTYPE_KEY\x10\x02\x12\x1c\n\x18REQUESTELEMENTTYPE_VALUE\x10\x03\x12\x1c\n\x18REQUESTELEMENTTYPE_FILED\x10\x04\x12\x1e\n\x1aREQUESTELEMENTTYPE_TIMEOUT\x10\x05\x12\x1d\n\x19REQUESTELEMENTTYPE_MEMBER\x10\x06\x12\x19\n\x15REQUESTELEMENTTYPE_EX\x10\x07\x12\x19\n\x15REQUESTELEMENTTYPE_PX\x10\x08\x12\x19\n\x15REQUESTELEMENTTYPE_NX\x10\t\x12\x19\n\x15REQUESTELEMENTTYPE_XX\x10\n*}\n\x0b\x45ResultType\x12\x13\n\x0fRESULTTYPE_NONE\x10\x00\x12\x15\n\x11RESULTTYPE_STRING\x10\x01\x12\x16\n\x12RESULTTYPE_INTEGER\x10\x02\x12\x14\n\x10RESULTTYPE_ARRAY\x10\x03\x12\x14\n\x10RESULTTYPE_FLOAT\x10\x04\x32\x63\n\x11RedisProxyService\x12N\n\rRedisProxyCmd\x12\x1d.RedisProxy.RedisProxyRequest\x1a\x1e.RedisProxy.RedisProxyResponse')
)

_ERESULTCODE = _descriptor.EnumDescriptor(
  name='EResultCode',
  full_name='RedisProxy.EResultCode',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='RESULT_SUCC', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULT_FAIL', index=1, number=-1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULT_DATA_NOTFOUND', index=2, number=-100,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULT_UNKNOWN_COMMAND', index=3, number=-200,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULT_UNKNOWN_VERSION', index=4, number=-201,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULT_SYSTEM_ERROR', index=5, number=-300,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=877,
  serialized_end=1076,
)
_sym_db.RegisterEnumDescriptor(_ERESULTCODE)

EResultCode = enum_type_wrapper.EnumTypeWrapper(_ERESULTCODE)
_EGLOBALCODE = _descriptor.EnumDescriptor(
  name='EGlobalCode',
  full_name='RedisProxy.EGlobalCode',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_SUCC', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_FAIL', index=1, number=-1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_INVALID_PARAM', index=2, number=-2,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_INVALID_USERPASSWD', index=3, number=-3,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_INVALID_IP', index=4, number=-4,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_QUOTAEXCEE', index=5, number=-5,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='GLOBAL_TOO_MANY_REQUESTS', index=6, number=-6,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1079,
  serialized_end=1313,
)
_sym_db.RegisterEnumDescriptor(_EGLOBALCODE)

EGlobalCode = enum_type_wrapper.EnumTypeWrapper(_EGLOBALCODE)
_ECALLTYPE = _descriptor.EnumDescriptor(
  name='ECallType',
  full_name='RedisProxy.ECallType',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='CALLTYPE_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CALLTYPE_SYNC', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CALLTYPE_ASYNC', index=2, number=2,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1315,
  serialized_end=1384,
)
_sym_db.RegisterEnumDescriptor(_ECALLTYPE)

ECallType = enum_type_wrapper.EnumTypeWrapper(_ECALLTYPE)
_ECALLMODE = _descriptor.EnumDescriptor(
  name='ECallMode',
  full_name='RedisProxy.ECallMode',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='CALLMODE_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CALLMODE_SINGLE', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CALLMODE_MUL', index=2, number=2,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CALLMODE_PIPELINE', index=3, number=3,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1386,
  serialized_end=1478,
)
_sym_db.RegisterEnumDescriptor(_ECALLMODE)

ECallMode = enum_type_wrapper.EnumTypeWrapper(_ECALLMODE)
_EVERSION = _descriptor.EnumDescriptor(
  name='EVersion',
  full_name='RedisProxy.EVersion',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='VERSION_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='VERSION_KV', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='VERSION_CMD', index=2, number=2,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='VERSION_ELEMENTVECTOR', index=3, number=3,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1480,
  serialized_end=1568,
)
_sym_db.RegisterEnumDescriptor(_EVERSION)

EVersion = enum_type_wrapper.EnumTypeWrapper(_EVERSION)
_ECHARTYPE = _descriptor.EnumDescriptor(
  name='ECharType',
  full_name='RedisProxy.ECharType',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='CHARTYPE_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CHARTYPE_STRING', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='CHARTYPE_BINARY', index=2, number=2,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1570,
  serialized_end=1642,
)
_sym_db.RegisterEnumDescriptor(_ECHARTYPE)

ECharType = enum_type_wrapper.EnumTypeWrapper(_ECHARTYPE)
_EREQUESTELEMENTTYPE = _descriptor.EnumDescriptor(
  name='ERequestElementType',
  full_name='RedisProxy.ERequestElementType',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_CMD', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_KEY', index=2, number=2,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_VALUE', index=3, number=3,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_FILED', index=4, number=4,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_TIMEOUT', index=5, number=5,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_MEMBER', index=6, number=6,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_EX', index=7, number=7,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_PX', index=8, number=8,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_NX', index=9, number=9,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='REQUESTELEMENTTYPE_XX', index=10, number=10,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1645,
  serialized_end=1982,
)
_sym_db.RegisterEnumDescriptor(_EREQUESTELEMENTTYPE)

ERequestElementType = enum_type_wrapper.EnumTypeWrapper(_EREQUESTELEMENTTYPE)
_ERESULTTYPE = _descriptor.EnumDescriptor(
  name='EResultType',
  full_name='RedisProxy.EResultType',
  filename=None,
  file=DESCRIPTOR,
  values=[
    _descriptor.EnumValueDescriptor(
      name='RESULTTYPE_NONE', index=0, number=0,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULTTYPE_STRING', index=1, number=1,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULTTYPE_INTEGER', index=2, number=2,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULTTYPE_ARRAY', index=3, number=3,
      serialized_options=None,
      type=None),
    _descriptor.EnumValueDescriptor(
      name='RESULTTYPE_FLOAT', index=4, number=4,
      serialized_options=None,
      type=None),
  ],
  containing_type=None,
  serialized_options=None,
  serialized_start=1984,
  serialized_end=2109,
)
_sym_db.RegisterEnumDescriptor(_ERESULTTYPE)

EResultType = enum_type_wrapper.EnumTypeWrapper(_ERESULTTYPE)
RESULT_SUCC = 0
RESULT_FAIL = -1
RESULT_DATA_NOTFOUND = -100
RESULT_UNKNOWN_COMMAND = -200
RESULT_UNKNOWN_VERSION = -201
RESULT_SYSTEM_ERROR = -300
GLOBAL_SUCC = 0
GLOBAL_FAIL = -1
GLOBAL_INVALID_PARAM = -2
GLOBAL_INVALID_USERPASSWD = -3
GLOBAL_INVALID_IP = -4
GLOBAL_QUOTAEXCEE = -5
GLOBAL_TOO_MANY_REQUESTS = -6
CALLTYPE_NONE = 0
CALLTYPE_SYNC = 1
CALLTYPE_ASYNC = 2
CALLMODE_NONE = 0
CALLMODE_SINGLE = 1
CALLMODE_MUL = 2
CALLMODE_PIPELINE = 3
VERSION_NONE = 0
VERSION_KV = 1
VERSION_CMD = 2
VERSION_ELEMENTVECTOR = 3
CHARTYPE_NONE = 0
CHARTYPE_STRING = 1
CHARTYPE_BINARY = 2
REQUESTELEMENTTYPE_NONE = 0
REQUESTELEMENTTYPE_CMD = 1
REQUESTELEMENTTYPE_KEY = 2
REQUESTELEMENTTYPE_VALUE = 3
REQUESTELEMENTTYPE_FILED = 4
REQUESTELEMENTTYPE_TIMEOUT = 5
REQUESTELEMENTTYPE_MEMBER = 6
REQUESTELEMENTTYPE_EX = 7
REQUESTELEMENTTYPE_PX = 8
REQUESTELEMENTTYPE_NX = 9
REQUESTELEMENTTYPE_XX = 10
RESULTTYPE_NONE = 0
RESULTTYPE_STRING = 1
RESULTTYPE_INTEGER = 2
RESULTTYPE_ARRAY = 3
RESULTTYPE_FLOAT = 4



_REQUESTHEAD = _descriptor.Descriptor(
  name='RequestHead',
  full_name='RedisProxy.RequestHead',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='nCallMode', full_name='RedisProxy.RequestHead.nCallMode', index=0,
      number=1, type=14, cpp_type=8, label=1,
      has_default_value=True, default_value=1,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nCallType', full_name='RedisProxy.RequestHead.nCallType', index=1,
      number=2, type=14, cpp_type=8, label=1,
      has_default_value=True, default_value=1,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=33,
  serialized_end=162,
)


_REQUESTELEMENT = _descriptor.Descriptor(
  name='RequestElement',
  full_name='RedisProxy.RequestElement',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='nRequestElementType', full_name='RedisProxy.RequestElement.nRequestElementType', index=0,
      number=1, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nCharType', full_name='RedisProxy.RequestElement.nCharType', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sRequestElement', full_name='RedisProxy.RequestElement.sRequestElement', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=164,
  serialized_end=253,
)


_REQUESTBODY = _descriptor.Descriptor(
  name='RequestBody',
  full_name='RedisProxy.RequestBody',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='sCmd', full_name='RedisProxy.RequestBody.sCmd', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sKey', full_name='RedisProxy.RequestBody.sKey', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sValue', full_name='RedisProxy.RequestBody.sValue', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='oRequestElementList', full_name='RedisProxy.RequestBody.oRequestElementList', index=3,
      number=4, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nVer', full_name='RedisProxy.RequestBody.nVer', index=4,
      number=5, type=5, cpp_type=1, label=1,
      has_default_value=True, default_value=1,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=256,
  serialized_end=387,
)


_REDISPROXYREQUEST = _descriptor.Descriptor(
  name='RedisProxyRequest',
  full_name='RedisProxy.RedisProxyRequest',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='sAppId', full_name='RedisProxy.RedisProxyRequest.sAppId', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sAppPw', full_name='RedisProxy.RedisProxyRequest.sAppPw', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sAppName', full_name='RedisProxy.RedisProxyRequest.sAppName', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sTableName', full_name='RedisProxy.RedisProxyRequest.sTableName', index=3,
      number=4, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='oRequestHead', full_name='RedisProxy.RedisProxyRequest.oRequestHead', index=4,
      number=5, type=11, cpp_type=10, label=1,
      has_default_value=False, default_value=None,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='oRequestBodyList', full_name='RedisProxy.RedisProxyRequest.oRequestBodyList', index=5,
      number=6, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=390,
  serialized_end=577,
)


_RESULTELEMENT = _descriptor.Descriptor(
  name='ResultElement',
  full_name='RedisProxy.ResultElement',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='nRet', full_name='RedisProxy.ResultElement.nRet', index=0,
      number=1, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nResultType', full_name='RedisProxy.ResultElement.nResultType', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sResultElement', full_name='RedisProxy.ResultElement.sResultElement', index=2,
      number=3, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=579,
  serialized_end=653,
)


_RESULTBODY = _descriptor.Descriptor(
  name='ResultBody',
  full_name='RedisProxy.ResultBody',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='nRet', full_name='RedisProxy.ResultBody.nRet', index=0,
      number=1, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='sValue', full_name='RedisProxy.ResultBody.sValue', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nResultType', full_name='RedisProxy.ResultBody.nResultType', index=2,
      number=3, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='oResultElementList', full_name='RedisProxy.ResultBody.oResultElementList', index=3,
      number=4, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nVer', full_name='RedisProxy.ResultBody.nVer', index=4,
      number=5, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=656,
  serialized_end=788,
)


_REDISPROXYRESPONSE = _descriptor.Descriptor(
  name='RedisProxyResponse',
  full_name='RedisProxy.RedisProxyResponse',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='oResultBodyList', full_name='RedisProxy.RedisProxyResponse.oResultBodyList', index=0,
      number=1, type=11, cpp_type=10, label=3,
      has_default_value=False, default_value=[],
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='nGret', full_name='RedisProxy.RedisProxyResponse.nGret', index=1,
      number=2, type=5, cpp_type=1, label=1,
      has_default_value=False, default_value=0,
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto2',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=790,
  serialized_end=874,
)

_REQUESTHEAD.fields_by_name['nCallMode'].enum_type = _ECALLMODE
_REQUESTHEAD.fields_by_name['nCallType'].enum_type = _ECALLTYPE
_REQUESTBODY.fields_by_name['oRequestElementList'].message_type = _REQUESTELEMENT
_REDISPROXYREQUEST.fields_by_name['oRequestHead'].message_type = _REQUESTHEAD
_REDISPROXYREQUEST.fields_by_name['oRequestBodyList'].message_type = _REQUESTBODY
_RESULTBODY.fields_by_name['oResultElementList'].message_type = _RESULTELEMENT
_REDISPROXYRESPONSE.fields_by_name['oResultBodyList'].message_type = _RESULTBODY
DESCRIPTOR.message_types_by_name['RequestHead'] = _REQUESTHEAD
DESCRIPTOR.message_types_by_name['RequestElement'] = _REQUESTELEMENT
DESCRIPTOR.message_types_by_name['RequestBody'] = _REQUESTBODY
DESCRIPTOR.message_types_by_name['RedisProxyRequest'] = _REDISPROXYREQUEST
DESCRIPTOR.message_types_by_name['ResultElement'] = _RESULTELEMENT
DESCRIPTOR.message_types_by_name['ResultBody'] = _RESULTBODY
DESCRIPTOR.message_types_by_name['RedisProxyResponse'] = _REDISPROXYRESPONSE
DESCRIPTOR.enum_types_by_name['EResultCode'] = _ERESULTCODE
DESCRIPTOR.enum_types_by_name['EGlobalCode'] = _EGLOBALCODE
DESCRIPTOR.enum_types_by_name['ECallType'] = _ECALLTYPE
DESCRIPTOR.enum_types_by_name['ECallMode'] = _ECALLMODE
DESCRIPTOR.enum_types_by_name['EVersion'] = _EVERSION
DESCRIPTOR.enum_types_by_name['ECharType'] = _ECHARTYPE
DESCRIPTOR.enum_types_by_name['ERequestElementType'] = _EREQUESTELEMENTTYPE
DESCRIPTOR.enum_types_by_name['EResultType'] = _ERESULTTYPE
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

RequestHead = _reflection.GeneratedProtocolMessageType('RequestHead', (_message.Message,), {
  'DESCRIPTOR' : _REQUESTHEAD,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.RequestHead)
  })
_sym_db.RegisterMessage(RequestHead)

RequestElement = _reflection.GeneratedProtocolMessageType('RequestElement', (_message.Message,), {
  'DESCRIPTOR' : _REQUESTELEMENT,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.RequestElement)
  })
_sym_db.RegisterMessage(RequestElement)

RequestBody = _reflection.GeneratedProtocolMessageType('RequestBody', (_message.Message,), {
  'DESCRIPTOR' : _REQUESTBODY,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.RequestBody)
  })
_sym_db.RegisterMessage(RequestBody)

RedisProxyRequest = _reflection.GeneratedProtocolMessageType('RedisProxyRequest', (_message.Message,), {
  'DESCRIPTOR' : _REDISPROXYREQUEST,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.RedisProxyRequest)
  })
_sym_db.RegisterMessage(RedisProxyRequest)

ResultElement = _reflection.GeneratedProtocolMessageType('ResultElement', (_message.Message,), {
  'DESCRIPTOR' : _RESULTELEMENT,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.ResultElement)
  })
_sym_db.RegisterMessage(ResultElement)

ResultBody = _reflection.GeneratedProtocolMessageType('ResultBody', (_message.Message,), {
  'DESCRIPTOR' : _RESULTBODY,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.ResultBody)
  })
_sym_db.RegisterMessage(ResultBody)

RedisProxyResponse = _reflection.GeneratedProtocolMessageType('RedisProxyResponse', (_message.Message,), {
  'DESCRIPTOR' : _REDISPROXYRESPONSE,
  '__module__' : 'redisproxy_pb2'
  # @@protoc_insertion_point(class_scope:RedisProxy.RedisProxyResponse)
  })
_sym_db.RegisterMessage(RedisProxyResponse)



_REDISPROXYSERVICE = _descriptor.ServiceDescriptor(
  name='RedisProxyService',
  full_name='RedisProxy.RedisProxyService',
  file=DESCRIPTOR,
  index=0,
  serialized_options=None,
  serialized_start=2111,
  serialized_end=2210,
  methods=[
  _descriptor.MethodDescriptor(
    name='RedisProxyCmd',
    full_name='RedisProxy.RedisProxyService.RedisProxyCmd',
    index=0,
    containing_service=None,
    input_type=_REDISPROXYREQUEST,
    output_type=_REDISPROXYRESPONSE,
    serialized_options=None,
  ),
])
_sym_db.RegisterServiceDescriptor(_REDISPROXYSERVICE)

DESCRIPTOR.services_by_name['RedisProxyService'] = _REDISPROXYSERVICE

# @@protoc_insertion_point(module_scope)
