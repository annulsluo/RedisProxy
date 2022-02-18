# Copyright 2015 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""The Python implementation of the GRPC helloworld.Greeter client."""

from __future__ import print_function
import logging

import grpc

import redisproxy_pb2
import redisproxy_pb2_grpc

# consul 是否支持py客户端
def run():
    # NOTE(gRPC Python Team): .close() is possible on a channel and should be
    # used in circumstances in which the with statement does not fit the needs
    # of the code.
    #with grpc.insecure_channel('172.16.155.210:50051') as channel:
    ip      = '172.16.155.210'
    port    = '50051'
    with grpc.insecure_channel('{0}:{1}'.format(ip,port)) as channel:
        stub = redisproxy_pb2_grpc.GreeterStub(channel)
        redisproxy_pb2.RequestHead oRequestHead 
            = redisproxy_pb2.RequestHead( nCallMode=redisproxy_pb2.CALLMODE_PIPELINE, nCallType=redisproxy_pb2.CALLTYPE_SYNC )

        response = stub.RedisProxyCmd(redisproxy_pb2.RedisProxyRequest(=namelist))
    print("Greeter client received: " + response.msg)

if __name__ == '__main__':
    logging.basicConfig()
    run()
