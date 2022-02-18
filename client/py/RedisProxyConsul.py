import consul

class RedisProxyConsul(object):
    def __init__( self, host, port, token ):
        '''初始化，连接consul服务器'''
        self._consul    = consul.Consul( host=host, port=int(port), token=token )
        self._token     = token

    def RegisterService(self, name, host, port, tags=None):
        tags = tags or []
        # 注册服务
        self._consul.agent.service.register(
            name,
            name,
            host,
            port,
            tags,
            # 健康检查ip端口，检查时间：5,超时时间：30，注销时间：30s
            check=consul.Check().tcp(host, port, "5s", "30s", "30s"))

    def GetService(self, name):
        services = self._consul.agent.services()
        service = services.get(name)
        if not service:
            return None, None
        addr = "{0}:{1}".format(service['Address'], service['Port'])
        return service, addr
    
    def GetServiceByCatalog( self, name ):
        servicelist = self._consul.service( name )
        for value in servicelist:
            print("account add:%s port:%s" % ( value['ServiceAddress'], value['ServicePort'] ) 

if __name__ == '__main__':
    consul_host     = ""
    consul_port     = ""
    consul_token    = ""
    oRedisProxyConsul   =  RedisProxyConsul( consul_host, consul_port, consul_token )
    service_name    = ""
    res             = oRedisProxyConsul.GetService( service_name )
    print( res )
